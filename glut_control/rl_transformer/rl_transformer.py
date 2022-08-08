import sys
sys.path.append('..')
import os, pickle
import datetime
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
                 dqn_base="rl_transformer/base_model/2hidden_layers_4attention_heads/checkpoint-2400000"):
        """
        Params:
          max_reward:  maximum reward for an episode; used to scale rewards for Q-function

        """
        #super().__init__([], [], False, debug, [], ())
        self.device = device
        self.seed=seed
        self.gamma=gamma
        self.epsilon=epsilon
        self.eps_min = eps_min=0.1
        self.eps_max=eps_max
        self.epsilon_interval = eps_max - eps_min
        self.epsilon_greedy_episodes = 300000

        self.batch_size = batch_size
        self.debugging=debugging
        
        self.log_dir = "logs/fit/" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        self.current_model = trans_dqn(self.debugging, device=device, base_model=dqn_base)
        self.target_model =  trans_dqn(self.debugging, device=device, base_model=dqn_base)
        self.current_model.to(device)
        self.target_model.to(device)

        #self.loss_fn = keras.losses.Huber()
        #self.bellman_loss = keras.losses.MeanSquaredError()
        self.bellman_loss = torch.nn.MSELoss()

        if finetune:
            self.params = list(self.current_model.parameters())
            for p in self.params:
                p.requires_grad = True
        else:
            self.params = self.current_model.trainable_parameters()
        self.current_model.train()
        self.optimizer = torch.optim.Adam(self.params)  # TODO: Be sure to update optimizer when switching current and target
        self.max_reward = max_reward
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
                                    alma_collection_to_strings(action)) for action in list_of_actions]
        print("Preprocess:", texts)
        return texts


    
    def get_priorities(self, inputs, current_model=True, training=False, numpy=True):
        """
        inputs is either a list of actions (pre-inferences) or else a pair (state,action).
        In the latter case, the state is repeated, once for each action.

        This is only used in evalutation and episode collection; we'll make sure the returned preditions are numpy arrays.
        """
        inputs_text = self.preprocess(inputs[0], inputs[1])


        # TODO:  Figure out the exact form here; BERT model won't take strings as input, maybe result of preprocessing?   
        model = self.current_model if current_model else self.target_model
        preds = [ self.current_model(inp).to(self.device) for inp in inputs_text]
        preds = torch.tensor(preds)

        return preds.detach().numpy()

    #@profile
    def fit(self, batch, verbose=True):
        """ A batch consists of 4 lists, each of lenth batch_size:
            actions, rewards, states0, states1
        """
        inputs_text = self.preprocess(batch.states0, batch.actions)
        batch_size = len(inputs_text)
        #future_rewards = [self.target_model(inp) for inp in inputs_text]
        future_rewards = self.target_model(inputs_text)
        updated_q_values = torch.tensor(batch.rewards).to(self.device) + self.gamma * future_rewards

        #for p in self.current_model.parameters():
        #    p.grad = None
        self.optimizer.zero_grad(set_to_none=True)
        #predictions = torch.tensor([self.current_model(inp) for inp in inputs_text],                                 requires_grad=True)
        predictions = self.current_model(inputs_text)
        loss = self.bellman_loss(predictions, updated_q_values.unsqueeze(1))
        loss.backward()
        self.optimizer.step()
        #return self.current_model.fit(inputs_text, updated_q_values, batch_size)  # This should just be a step of optimizaiton; need to redo for pytorch though.
        return loss

    def train_on_given_batch(self, inputs, target):
        """
        Note:  we explore using the target model (i.e. generate priorities) but train the current model. 
        TODO:  not working; use fit instead.
        """
        assert(False)   # This shouldn't be used right now
        #future_rewards = network.target_model.predict(batch.actions)
        future_rewards = self.get_priorities(batch.actions, current_model=False, numpy=False)  # Use target_model

        # The original (below) probably isn't right for our network structure.   
        #updated_q_values = batch.rewards + network.gamma * tf.reduce_max(   future_rewards, axis=1 )
        updated_q_values = np.array(batch.rewards).reshape(-1) + self.gamma * future_rewards

        # If final frame set the last value to -1.
        # TODO:  Not sure how to translate this?  For now let's not use it; we'll
        # consider our episodes to be potentially infinite.  This will probably come in if
        # we want to limit the depth of the inference tree.
        #updated_q_values = updated_q_values * (1 - batch.done) - batch.done

        # Create a mask so we only calculate loss on the updated Q-values
        #masks = tf.one_hot(batch.actions, num_actions)


        # Train the model on the states and updated Q-values
        q_values = self.get_priorities(batch.actions, current_model=True, training=True, numpy=False)
        temporary_test_q = q_values + 1
        # Apply the masks to the Q-values to get the Q-value for action taken
        #q_action = tf.reduce_sum(tf.multiply(q_values, masks), axis=1)
        # Calculate loss between new Q-value and old Q-value
        #loss = network.loss_fn(updated_q_values, q_action)
        #loss = self.loss_fn(updated_q_values, q_values)
        loss = self.loss_fn(temporary_test_q, q_values)

        with tf.GradientTape() as tape:
            # Backpropagation
            grads = tape.gradient(loss, self.current_model.graph_net.trainable_variables)
            self.optimizer.apply_gradients(zip(grads, self.current_model.graph_net.trainable_variables))

    def update_target(self):
        self.target_model.load_state_dict(self.current_model.state_dict())

    
    def model_save(self, folder, id_str):
        if not os.path.exists(folder):
            os.makedirs(folder)
        pkl_file = open(  os.path.join(folder, "rl_trans_model_{}.pkl".format(id_str)), "wb")
        pickle.dump((self.debugging, self.current_model.base_model_dir,
                     self.current_model.tokenizer_prefix, self.current_model.hidden_size,
                     self.seed, self.gamma,
                     self.epsilon, self.eps_min, self.eps_max,
                     self.batch_size, self.max_reward,
                     self.starting_episode, self.current_episode), pkl_file)
        self.current_model.save(folder, "trans_dqn_current_model_{}".format(id_str))
        self.target_model.save(folder, "trans_dqn_target_model_{}".format(id_str))
    def model_load(self, folder, id_str):
        pkl_file = open(os.path.join(model_prefix, "rl_trans_model_{}.pkl".format(id_str)), "rb")
        (self.debugging, base_model_dir,
         tokenizer_prefix, hidden_size,
         self.seed, self.gamma,
         self.epsilon, self.eps_min, self.eps_max,
         self.batch_size, self.max_reward,
         self.starting_episode, self.current_episode) = pickle.load(pkl_file)
        self.current_model = trans_dqn(debugging=self.debugging,
                                       base_model=base_model_dir,
                                       tokenizer_prefix=tokenizer_prefix,
                                       hidden_size=hidden_size)
        self.current_model.load_state_dict(torch.load(os.path.join(folder, "trans_dqn_current_model_{}".format(id_str))))
        self.target_model = trans_dqn(debugging=self.debugging,
                                       base_model=base_model_dir,
                                       tokenizer_prefix=tokenizer_prefix,
                                       hidden_size=hidden_size)
        self.target_model.load_state_dict(torch.load(os.path.join(folder, "trans_dqn_target_model_{}".format(id_str))))
