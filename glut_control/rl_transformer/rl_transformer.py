import sys
sys.path.append('..')
import os, pickle
import datetime
import wandb

import torch
from alma_utils import kb_action_to_text, alma_collection_to_strings

from resolution_prebuffer import res_prebuffer
from rl_transformer.trans_dqn import trans_dqn


class rl_transformer(res_prebuffer):
    def __init__(self, max_reward, reward_fn,
                 debug=True, 
                 seed=0, gamma=0.99, epsilon=1.0, eps_min=0.1,
                 eps_max=1.0, batch_size=16, starting_episode=0, use_state = True,
                 done_reward=0, debugging=False,
                 finetune=True, device="cpu",
                 dqn_base="rl_transformer/base_model/2hidden_layers_4attention_heads/checkpoint-2400000",
                 use_now=False,
                 reload_fldr=None, reload_id=None,
                 normalize_rewards=True):
        """
        Params:
          max_reward:  maximum reward for an episode; used to scale rewards for Q-function

        """
        #super().__init__([], [], False, debug, [], ())
        self.use_now = use_now
        self.device = device
        self.seed=seed
        self.gamma=gamma
        self.epsilon=epsilon
        self.eps_min = eps_min=0.1
        self.eps_max=eps_max
        self.epsilon_interval = eps_max - eps_min
        self.epsilon_greedy_episodes = 300000

        self.max_reward = max_reward
        self.normalize_rewards = normalize_rewards


        self.batch_size = batch_size
        self.debugging=debugging
        
        self.log_dir = "logs/fit/" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        if reload_fldr is None:
            self.mr = 1.0 if normalize_rewards else max_reward
            self.current_model = trans_dqn(self.debugging, device=device, base_model=dqn_base, max_reward=self.mr)
            self.target_model =  trans_dqn(self.debugging, device=device, base_model=dqn_base, max_reward=self.mr)
            self.update_target()
        else:
            self.model_load(reload_fldr, reload_id, device=device)

        self.current_model.to(device)
        self.target_model.to(device)

        #self.loss_fn = keras.losses.Huber()
        #self.bellman_loss = keras.losses.MeanSquaredError()
        #self.bellman_loss = torch.nn.MSELoss()
        self.bellman_loss = torch.nn.HuberLoss()

        if finetune:
            self.params = list(self.current_model.parameters())
            for p in self.params:
                p.requires_grad = True
        else:
            self.params = self.current_model.trainable_parameters()
        self.current_model.train()
        #self.optimizer = torch.optim.Adam(self.params)  # TODO: Be sure to update optimizer when switching current and target
        self.optimizer = torch.optim.RMSprop(self.params)
        #self.acc_fn = CategoricalAccuracy()

        if self.debugging:
            pass

        self.reward_fn = reward_fn
        self.starting_episode = starting_episode
        self.current_episode = starting_episode
        self.use_state = use_state
        self.done_reward = done_reward

    def epsilon_decay(self):
        self.epsilon -= (self.epsilon_interval / self.epsilon_greedy_episodes)
        if self.epsilon < self.eps_min:
            self.epsilon = self.eps_min

    def reset_start(self):
        self.starting_episode = self.current_episode + 1
        self.current_episode = self.starting_episode


    def preprocess(self, list_of_states, list_of_actions):
        texts =  [kb_action_to_text(alma_collection_to_strings(list_of_states[0]),
                                    alma_collection_to_strings(action), self.use_now)
                  for action in list_of_actions]
        #print("Preprocess:", texts)
        return texts

    def multi_preprocess(self, list_of_states, list_of_actions):

        # here we assume that each element of list of actions
        # corresponds to multiple actions.  this will be useful when
        # we have to get max_a Q(s,a).

        return [self.preprocess( [kb]*len(actions), actions) for kb, actions in zip(list_of_states, list_of_actions)]

    def get_qvalues(self, inputs, current_model=True, training=False, numpy=True):
        """
        inputs is either a list of actions (pre-inferences) or else a pair (state,action).
        In the latter case, the state is repeated, once for each action.

        This is only used in evalutation and episode collection; we'll make sure the returned preditions are numpy arrays.
        """
        kbs, actions = inputs
        inputs_text = self.preprocess(kbs, actions)


        # TODO:  Figure out the exact form here; BERT model won't take strings as input, maybe result of preprocessing?   
        preds = [ self.current_model(inp).to(self.device) for inp in inputs_text]
        preds = torch.tensor(preds)

        return preds.detach().numpy()

    def get_priorities(self, inputs, current_model=True, training=False, numpy=True):
        return 1 - (self.get_qvalues(inputs, current_model, training, numpy) / self.mr)

    #@profile
    def fit(self, batch, verbose=True):
        """ A batch consists of 4 lists, each of lenth batch_size:
            actions, rewards, states0, states1
        """
        next_sa = self.multi_preprocess(batch.states1, batch.potential_actions)
        batch_size = len(next_sa)
        future_rewards = torch.tensor([torch.max(self.target_model(sa)) for sa in next_sa]).to(self.device)

        rewards = torch.tensor(batch.rewards).to(self.device)
        if self.normalize_rewards:
            rewards = rewards / self.max_reward
        updated_q_values = rewards + self.gamma * future_rewards

        #for p in self.current_model.parameters():
        #    p.grad = None
        self.optimizer.zero_grad(set_to_none=True)
        #predictions = torch.tensor([self.current_model(inp) for inp in inputs_text],
        # requires_grad=True)

        current_sa = self.preprocess(batch.states0, batch.actions)
        predictions = self.current_model(current_sa).to(self.device)
        loss = self.bellman_loss(predictions, updated_q_values.unsqueeze(1))
        loss.backward()
        self.optimizer.step()
        #return self.current_model.fit(inputs_text, updated_q_values, batch_size)  # This should just be a step of optimizaiton; need to redo for pytorch though.
        wandb.log({"loss": loss.item()})
        return loss

    def update_target(self):
        self.target_model.load_state_dict(self.current_model.state_dict())

    #TODO:  update the next two methods to save all the state.
    def model_save(self, folder, id_str):
        if not os.path.exists(folder):
            os.makedirs(folder)
        pkl_file = open(  os.path.join(folder, "rl_trans_model_{}.pkl".format(id_str)), "wb")
        pickle.dump((self.debugging, self.current_model.base_model_dir,
                     self.current_model.tokenizer_prefix, self.current_model.hidden_size,
                     self.seed, self.gamma,
                     self.epsilon, self.eps_min, self.eps_max,
                     self.batch_size, self.max_reward,
                     self.starting_episode, self.current_episode,
                     self.device, self.max_reward), pkl_file)
        self.current_model.save(folder, "trans_dqn_current_model_{}".format(id_str))
        self.target_model.save(folder, "trans_dqn_target_model_{}".format(id_str))
    def model_load(self, folder, id_str, device):  
        pkl_file = open(os.path.join(folder, "rl_trans_model_{}.pkl".format(id_str)), "rb")
        (self.debugging, base_model_dir,
         tokenizer_prefix, hidden_size,
         self.seed, self.gamma,
         self.epsilon, self.eps_min, self.eps_max,
         self.batch_size, self.max_reward,
         self.starting_episode, self.current_episode,
         self.device, self.max_reward) = pickle.load(pkl_file)
        self.current_model = trans_dqn(debugging=self.debugging,
                                       tokenizer_prefix=tokenizer_prefix,
                                       hidden_size=hidden_size,
                                       device=device)
        self.current_model.load_state_dict(torch.load(os.path.join(folder, "rl_model_trans_dqn_current_model_{}".format(id_str))))
        self.target_model = trans_dqn(debugging=self.debugging,
                                       tokenizer_prefix=tokenizer_prefix,
                                       hidden_size=hidden_size,
                                      device=device)
        self.target_model.load_state_dict(torch.load(os.path.join(folder, "rl_model_trans_dqn_target_model_{}".format(id_str))))
