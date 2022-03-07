import numpy as np
import random
from transformers import GPT2Tokenizer
import torch
from torch import nn


class dt_dataset():
    def __init__(self, trajectories, state_dim=2048, act_dim=1024, max_ep_len=100, scale=100, device='cuda'):
        self.trajectories = trajectories 
        self.num_trajectories = len(trajectories)
        self.tokenizer = GPT2Tokenizer.from_pretrained("gpt2")

        self.state_dim=state_dim
        self.act_dim=act_dim
        self.max_ep_len=max_ep_len
        self.scale = scale
        self.device=device

        # We want to be able to handle up to around 200 tokens in a single string; so we'll
        # embed each token in a vector of length state_dim / 200.
        # The actions will be generally shorter, so we can limit ourselves to 100.

        self.state_embed = nn.Embedding(self.tokenizer.vocab_size, state_dim // 200)
        self.action_embed = nn.Embedding(self.tokenizer.vocab_size, act_dim // 100)
        

    def state_vectorize_list(self, state_str_list):
        """
        Given a list of states, return a numpy vector representation of the concatenatation of them.

        Note that for each string s in state_str_list, we'll get a (num_tokens(s),state_dim) embedding
        of that string.  To get a single embedding of the entire string, we simply flatten it
        """
        state_indices = [self.tokenizer(s)['input_ids'] for s in state_str_list]
        return [ self.state_embed(s).reshape(1,-1) for s in state_indices]

    def state_vectorize_str(self, state_str_list):
        """
        Given a state represented as a string, return a numpy vector representation.
        """
        state_embeddings = [ self.state_embed( torch.LongTensor(self.tokenizer(s)['input_ids']) ).reshape(1,-1) for s in state_str_list]
        for i in range(len(state_embeddings)):
            num_pad = max(0, self.state_dim - state_embeddings[i].shape[1])
            state_embeddings[i] = torch.cat((state_embeddings[i], torch.zeros(1,num_pad)),dim=1)
        return torch.stack(state_embeddings, dim=1)
    
    def action_vectorize_list(self, action):
        pass

    def action_vectorize_str(self, action_str_list):
        action_embeddings =  [ self.action_embed( torch.LongTensor(self.tokenizer(a)['input_ids'])).reshape(1,-1) for a in action_str_list]
        for i in range(len(action_embeddings)):
            num_pad = max(0, self.act_dim - action_embeddings[i].shape[1])
            action_embeddings[i] = torch.cat((action_embeddings[i],torch.zeros(1,num_pad)),dim=1)
        return torch.stack(action_embeddings, dim=1)

    def get_batch(self, batch_size=256, max_len=128):
        trajectories = self.trajectories

        batch_inds = np.random.choice(
            np.arange(self.num_trajectories),
            size=batch_size,
            replace=True,
            #p=p_sample,  # reweights so we sample according to timesteps
        )

        s, a, r, d, rtg, timesteps, mask = [], [], [], [], [], [], []
        for i in range(batch_size):
            traj = trajectories[int(batch_inds[i])]

            si = random.randint(0, traj['total']-1)
            trunc_traj_kb = traj['kb'][si:si + max_len]
            trunc_traj_actions = traj['actions'][si:si + max_len]
            truncated_traj = {'total': len(trunc_traj_kb),
                              'kb': trunc_traj_kb,
                              'actions': trunc_traj_actions}
            #straj = trajectory_unformatted_add_returns(trajectory_unformatted_sar(traj))
            straj = trajectory_sar_add_returns(sar_format(truncated_traj))
            states = [x[0] for x in straj]
            actions = [x[1] for x in straj]
            rewards = [x[2] for x in straj]
            returns = [x[4] for x in straj]

            s.append(self.state_vectorize_str(states))
            a.append(self.action_vectorize_str(actions))
            r.append(np.array(rewards).reshape(1,-1,1))
            dones = np.zeros(len(trunc_traj_kb)).reshape(1,-1); dones[0,-1] = 1
            d.append(dones)
            # timesteps.append(np.arange(si, si + s[-1].shape[1]).reshape(1, -1))
            num_timesteps = min(truncated_traj['total'], max_len)
            timesteps.append(np.arange(si, si + num_timesteps).reshape(1, -1))
            timesteps[-1][timesteps[-1] >= self.max_ep_len] = self.max_ep_len-1  # padding cutoff
            rtg.append(np.array(returns).reshape(1,-1,1))
            if rtg[-1].shape[1] <= num_timesteps:
                rtg[-1] = np.concatenate([rtg[-1], np.zeros((1, 1,1))], axis=1)

            # padding and state + reward normalization
            #tlen = s[-1].shape[1]
            tlen = num_timesteps
            
            # Skip normalization for now; requires calculating global stats for datset
            # s,a padded in vectorization.
            s[-1] = torch.cat([torch.zeros((1, max_len - tlen, self.state_dim)), s[-1]], axis=1)
            #s[-1] = (s[-1] - state_mean) / state_std
            a[-1] = torch.cat([torch.ones((1, max_len - tlen, self.act_dim)) * -10., a[-1]], axis=1)
            r[-1] = np.concatenate([np.zeros((1, max_len - tlen, 1)), r[-1]], axis=1)
            d[-1] = np.concatenate([np.ones((1, max_len - tlen)) * 2, d[-1]], axis=1)
            # Right now, rtg is one longer than then other vectors.  TODO:   Is this a bug?
            rtg[-1] = np.concatenate([np.zeros((1, max_len - (tlen+1), 1)), rtg[-1]], axis=1) / self.scale
            timesteps[-1] = np.concatenate([np.zeros((1, max_len - tlen)), timesteps[-1]], axis=1)
            mask.append(np.concatenate([np.zeros((1, max_len - tlen)), np.ones((1, tlen))], axis=1))

        #s = torch.from_numpy(np.concatenate(s, axis=0)).to(dtype=torch.float32, device=device)
        #a = torch.from_numpy(np.concatenate(a, axis=0)).to(dtype=torch.float32, device=device)
        s = torch.cat(s, axis=0).to(dtype=torch.float32, device=self.device)
        a = torch.cat(a, axis=0).to(dtype=torch.float32, device=self.device)
        r = torch.from_numpy(np.concatenate(r, axis=0)).to(dtype=torch.float32, device=self.device)
        d = torch.from_numpy(np.concatenate(d, axis=0)).to(dtype=torch.long, device=self.device)
        rtg = torch.from_numpy(np.concatenate(rtg, axis=0)).to(dtype=torch.float32, device=self.device)
        timesteps = torch.from_numpy(np.concatenate(timesteps, axis=0)).to(dtype=torch.long, device=self.device)
        mask = torch.from_numpy(np.concatenate(mask, axis=0)).to(device=self.device)

        return s, a, r, d, rtg, timesteps, mask

    def eval_episodes(target_rew):
            def fn(model):
                returns, lengths = [], []
                for _ in range(num_eval_episodes):
                    with torch.no_grad():
                        if model_type == 'dt':
                            ret, length = evaluate_episode_rtg(
                                env,
                                state_dim,
                                act_dim,
                                model,
                                max_ep_len=max_ep_len,
                                scale=scale,
                                target_return=target_rew/scale,
                                mode=mode,
                                state_mean=state_mean,
                                state_std=state_std,
                                device=device,
                            )
                        else:
                            ret, length = evaluate_episode(
                                env,
                                state_dim,
                                act_dim,
                                model,
                                max_ep_len=max_ep_len,
                                target_return=target_rew/scale,
                                mode=mode,
                                state_mean=state_mean,
                                state_std=state_std,
                                device=device,
                            )
                    returns.append(ret)
                    lengths.append(length)
                return {
                    f'target_{target_rew}_return_mean': np.mean(returns),
                    f'target_{target_rew}_return_std': np.std(returns),
                    f'target_{target_rew}_length_mean': np.mean(lengths),
                    f'target_{target_rew}_length_std': np.std(lengths),
                }
            return fn

# ================================================================================
# Utilitiy Functions
# ================================================================================
def format_kb(kb_list):
    return "; ".join([x for x in kb_list if "agentname" not in x and "wallnow" not in x])

def format_act(act_list):
    return "; ".join(act_list)

def reward1(kb):
    sum=0
    for s in kb:
        if ("f(a)" in s) or ("g(a)" in s):
            sum += s.count("f")
    return sum

def trajectory_unformatted_sar(trajectory, calc_reward = reward1 ):
    tkb = trajectory['kb']
    tact = trajectory['actions']
    total = trajectory['total']
    result=[]
    total_reward = calc_reward(tkb[0]) # The calc_reward gives the total_reward; we subtract the previous
                                       # reward to get the reward for each indvidual action.
    for i in range(total):
        kb = format_kb(tkb[i])
        act = format_act(tact[i])
        reward = calc_reward(tkb[i+1]) if i < total-1 else total_reward
        result.append( (tkb[i], tact[i], reward - total_reward, reward) )
        total_reward = reward
    return result


def sar_format(trajectory, calc_reward = reward1 ):
    """
    Takes a trajectory as input, 
    returns a list of (state, action, reward) triples as output
    """
    tkb = trajectory['kb']
    tact = trajectory['actions']
    total = trajectory['total']
    result=[]
    total_reward = calc_reward(tkb[0])  # The calc_reward gives the total_reward; we subtract the previous
    # reward to get the reward for each indvidual action.
    for i in range(total):
        kb = format_kb(tkb[i])
        act = format_act(tact[i])
        reward = calc_reward(tkb[i]) 
        result.append( (kb, act, reward - total_reward, reward) )
        total_reward = reward
    return result

def trajectory_sar_add_returns(trajectory_sar):
    trajectory_total = trajectory_sar[-1][3]
    return [ (s, a, r, tr, trajectory_total - tr) for (s,a,r,tr) in trajectory_sar ]

def trajectory_unformatted_sar_add_returns(trajectory_sar):
    trajectory_total = trajectory_sar[-1][3]
    return [ (s, a, r, tr, trajectory_total - tr) for (s,a,r,tr) in trajectory_sar ]


def format_trajectory(trajectory):
    """
    A trajectory will be a dictionary with attributes 'total', 'kb',
    'actions'.

    This will return a list of (state,action,returns_to_go) triples 
    formatted to be fed into a pre-trained model.

    """
    sartg_list = trajectory_sar_add_returns(sar_format(trajectory))
    return [ x[0] + "="*10 + x[1] + ":"*10 + str(x[4]) for x in sartg_list]

def format_trajectory_gpt3(trajectory):
    """
    A trajectory will be a dictionary with attributes 'total', 'kb',
    'actions'.

    This will return a list of (state,action,returns_to_go) triples 
    formatted to be fed into a pre-trained model.

    Want (R_1, s_1, a_1, R_2, s_2, a_2, ...)
    """
    sartg_list = trajectory_sar_add_returns(sar_format(trajectory))
    traj_list =  [ "<" + str(x[4]) + "="*4 + x[0] + ":"*4 + x[1] + ">" for x in sartg_list]
    return "[" + ("|"*8).join(traj_list) + "]"
