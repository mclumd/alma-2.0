""" 
Functions for Q-learning.
"""
import tensorflow as tf
import alma

class ql_batch:
    def __init__(self):
        self.states, self.actions, self.rewards, self.new_states = [], [], [], []
        
def get_rewards(alma_inst):
    kb = alma.kbprint(alma_inst)[0]
    sum=0
    for s in kb.split('\n'):
        if ("f(a)" in s) or ("g(a)" in s):
            sum += s.count("f")
    return [sum]

def collect_episode(network, replay_buffer, alma_inst, episode_length):
    """
    Run an episode, appending results to replay buffer.
    """
    for i in range(episode_length):
        prebuf = alma.prebuf(alma_inst)

        # TODO:  This is assuming that the first item is the minimal priority; this should be backed
        # with an investigation and an assertion.
        prb = prebuf[0]
        state0 = alma.kb_to_pyobject(alma_inst)
        #res_task_input = [x[:2] for x in prb]
        if len(prb) > 0:
            action = prebuf[0][0][:2]
            priorities = network.get_priorities([action])
            alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
            alma.single_prb_to_res_task(alma_inst, 1.0)
            alma.astep(alma_inst)
            reward = get_rewards(alma_inst)
            state1 = alma.kb_to_pyobject(alma_inst)
            replay_buffer.append(state0, action, reward, state1)
            

    


def replay_train(network, replay_buffer):
    if replay_buffer.num_entries() > network.batch_size:
        batch = replay_buffer.get_batch(network.batch_size)
        #network.train_on_given_batch(batch)
        network.fit(batch)



def q_loss_function(updated_q_values, q_action):
    pass

