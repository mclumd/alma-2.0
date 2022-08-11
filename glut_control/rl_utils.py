""" 
Functions for Q-learning.
"""
import tensorflow as tf
import alma
#from memory_profiler import profile
import alma_utils
import numpy as np


class ql_batch:
    def __init__(self):
        self.states, self.actions, self.rewards, self.new_states = [], [], [], []
        


#@profile
def collect_episode(network, replay_buffer, alma_inst, episode_length):
    """
    Run an episode, appending results to replay buffer.
    """
    import sys

    prebuf = alma.prebuf(alma_inst)
    full_actions = prebuf[0]
    actions_no_priorities = [x[:2] for x in full_actions]

    for i in range(episode_length):
        #import objgraph
        #objgraph.show_most_common_types(limit=20)
        state0 = sorted(alma.kb_to_pyobject(alma_inst, True))

        #res_task_input = [x[:2] for x in prb]
        if len(full_actions) > 0:
            if np.random.uniform() < network.epsilon:
                priorities =  np.random.uniform(size=len(full_actions))
                min_idx = np.argmin(priorities)
            else:
                if network.use_state:
                    priorities = (network.get_priorities([state0]*len(actions_no_priorities), actions_no_priorities) * 0.9).flatten()
                else:
                    priorities = 1 - (network.get_priorities(actions_no_priorities) * 0.9).flatten()
                min_idx = np.argmin(priorities)
                
            action = full_actions[min_idx][:2]
            # This is a bit of a hack -- we want to only send the minimum priority action to the
            # resolution heap.  We do this by decreasing it's corresponding priority and using
            # the originial priority as a threhold for set_priors_prb (which look for items strictly
            # below the given threshold).
            min_priority = priorities[min_idx]
            priorities[min_idx] = min_priority / 2.0
            alma.set_priors_prb(alma_inst, priorities.tolist())
            alma.single_prb_to_res_task(alma_inst, min_priority)
            #priorities = 1 - (network.get_priorities([action])*0.9).flatten()
            alma.astep(alma_inst)
            kb = alma.kbprint(alma_inst)[0]
            #reward = (network.reward_fn(kb) / network.max_reward) if i < (episode_length - 1) else -1  # -1 for the last episode
            #reward = (network.reward_fn(kb) / network.max_reward) if i < (
            #            episode_length - 1) else network.done_reward  #  for the last episode
            reward = network.reward_fn(kb) #if i < (                    episode_length ) else network.done_reward  # for the last episode

            # I don't think we need to worry about done's because the
            # episodes are all of a fixed length.

            state1 = sorted(alma.kb_to_pyobject(alma_inst, True))
            prebuf = alma.prebuf(alma_inst)
            full_actions = prebuf[0]
            actions_no_priorities = [x[:2] for x in full_actions]
            replay_buffer.append(state0, action, reward, state1, actions_no_priorities)







def play_episode(network, alma_inst, episode_length):
    """
    Run an episode, appending results to replay buffer.
    """
    record = {'prebuf': [], 'heap': [], 'kb': [], 'rewards': []}
    for i in range(episode_length):
        prebuf = alma.prebuf(alma_inst)
        record['prebuf'].append(prebuf)

        rth = alma.res_task_buf(alma_inst)
        record['heap'].append(list(enumerate(rth[1].split('\n')[:-1])))
        prb = prebuf[0]
        kb = sorted(alma.kb_to_pyobject(alma_inst, True))
        record['kb'].append(kb)
        
        if len(prb) > 0:
            #action = [prebuf[0][0][:2]]
            actions = [pres[:2] for pres in prb]
            if network.use_state:
                priorities = network.get_priorities( ([kb]*len(actions), actions)  )
            else:
                priorities = 1 - network.get_priorities(actions)*0.9

            min_idx = np.argmin(priorities)
            min_priority = priorities[min_idx]
            action = actions[min_idx][:2]
            # This is a bit of a hack -- we want to only send the minimum priority action to the
            # resolution heap.  We do this by decreasing it's corresponding priority and using
            # the originial priority as a threhold for set_priors_prb (which look for items strictly
            # below the given threshold).

            priorities[min_idx] = min_priority / 2.0
            alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
            alma.single_prb_to_res_task(alma_inst, min_priority)

            alma.astep(alma_inst)
            kb = alma.kbprint(alma_inst)[0]
            reward = network.reward_fn(kb)
            record['rewards'].append(reward)
            print("i=",i)
            print("-" * 80)
            print(kb)
            print("-" * 80)
            #alma_utils.pr_heap_print(alma_inst, 10)
            print("=" * 80)
    return record

#@profile
def replay_train(network, replay_buffer, exhaustive = False):
    if replay_buffer.num_entries() > network.batch_size:
        batch = replay_buffer.get_batch(network.batch_size)
        #network.train_on_given_batch(batch)
        network.fit(batch)
    if exhaustive:   # If exhaustive, try to use up the replay buffer
        while replay_buffer.num_entries() > network.batch_size:
            print("another batch...", end=" ")
            batch = replay_buffer.get_batch(network.batch_size)
            network.fit(batch)



def q_loss_function(updated_q_values, q_action):
    pass

