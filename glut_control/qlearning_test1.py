"""
Use graph representations to learn a Q-function with an abstract axiom set.
Forked from test3.py
"""
import os
import random
import numpy as np
import itertools
import pickle
import gc
from importlib import reload
import alma, alma_utils
import argparse
import rl_utils, rl_dataset
#import resolution_prebuffer as rpb
from rpb_dqn import rpb_dqn, get_rewards_test1, get_rewards_test3
import time
#import tracemalloc
from memory_profiler import profile

test_params = {
    'explosion_size': 1000,
    'alma_heap_print_size': 100
}
#alma_inst,res = alma.init(1,'/home/justin/alma-2.0/glut_control/qlearning2.pl', '0', 1, 1000, [], [])
alma_inst,res = None, None

def res_task_lits(lit_str):
    L = lit_str.split('\n')[:-1]
    return [ x.split('\t') for x in L]

#@profile
def train(num_steps=50, model_name="test1", use_gnn = True, num_episodes=100000, train_interval=500, update_target_network_interval=10000, debug_level=0, gnn_nodes = 20, exhaustive_training=False, kb='/home/justin/alma-2.0/glut_control/qlearning2.pl', subjects = ['a', 'f', 'g', 'l']):
    """
    Train Q-network on the initial qlearning task.   

    Basic Algorithm:
      1.  Initialize and load kb
      2.  For the given number of episodes:
      3.    Create an episode and save in experience replay structure
      4.    Periodically run the network Q on an episode; using Q to train Q*
      5.    Periodically replace Q<--Q*

    Args:
      num_steps(500):             Number of reasoning steps for each round
      model_name("test1"):        Base name for the saved model
      use_gnn(True):              Use graph networks (if false use bag-of-words)
      num_episodes(1000):         How many episodes to train on
      train_interval(500):        How frequently do we attempts to train
 

    """

    # Initialization
    global alma_inst,res, alma
    #network = rpb_dqn(num_steps ** 2, subjects, [], use_gnn=use_gnn)
    #network = rpb_dqn(num_steps *2, subjects, [], use_gnn=use_gnn, gnn_nodes=gnn_nodes)
    if "qlearning3.pl" in kb:
        reward_fn = get_rewards_test3
    else:
        reward_fn = get_rewards_test1
    network = rpb_dqn(10000, reward_fn, subjects, [], use_gnn=use_gnn, gnn_nodes=gnn_nodes)    # Use max_reward of 10K
    replay_buffer = rl_dataset.experience_replay_buffer()
    start_time = time.time()
    for episode in range(num_episodes):
        print("Starting episode ", episode)
        #del alma
        reload(alma)
        alma_inst,res = alma.init(1,kb, '0', 1, 1000, [], [])
        rl_utils.collect_episode(network, replay_buffer, alma_inst, num_steps)
        alma.halt(alma_inst)
        if (episode % train_interval == 0) and (episode > 0):
            rl_utils.replay_train(network, replay_buffer, exhaustive_training)
            if episode % update_target_network_interval == 0:
                print("Updating at: ", time.time() - start_time)
                network.target_model.model.set_weights(network.current_model.model.get_weights())
        if episode % 200 == 0:
            del replay_buffer
            replay_buffer = rl_dataset.experience_replay_buffer()


        if episode % 2500 == 0:
            res = test(network, kb, num_steps)
            print("-"*80)
            print("Rewards at episode {} (epsilon=={}): {}".format(episode, network.epsilon, res['rewards']))
            network.model_save(model_name + "_ckpt" + str(episode))
            print("-"*80)


        if debug_level > 2:
            print("episode: ", episode)
            alma_utils.prebuf_print(alma_inst)
            alma_utils.pr_heap_print(alma_inst)
            alma_utils.kb_print(alma_inst)


    network.model_save(model_name)
    return network

def test(network, kb, num_steps=500):
    global alma_inst,res
    alma_inst, res = alma.init(1,kb, '0', 1, 1000, [], [])
    return rl_utils.play_episode(network, alma_inst, num_steps)


def main():
    parser = argparse.ArgumentParser(description="Run test with specified parameters.")
    parser.add_argument("episode_length", type=int, default=500)
    parser.add_argument("num_episodes", type=int, default=10000)
    parser.add_argument("--heap_print_size", type=int, default=0)
    parser.add_argument("--prb_print_size", type=int, default=0)
    parser.add_argument("--numeric_bits", type=int, default=10)
    parser.add_argument("--prb_threshold", type=float, default=1.0)
    parser.add_argument("--random_network", action='store_true')
    parser.add_argument("--train", action='store', required=False, default="NONE")

    parser.add_argument("--train_interval", type=int, default=500)
    parser.add_argument("--target_update_interval", type=int, default=500)
    parser.add_argument("--reload", action='store', required=False, default="NONE")
    parser.add_argument("--gnn", action='store_true')
    parser.add_argument("--gnn_nodes", type=int, default=50)
    parser.add_argument("--cpu_only", action='store_true')
    parser.add_argument("--exhaustive", action='store_true')
    parser.add_argument("--kb", default='/home/justin/alma-2.0/glut_control/qlearning2.pl', action='store')

    args = parser.parse_args()

    if args.cpu_only:
        # TODO:  This may need to be done before any tensorflow imports
        os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
    import resolution_prebuffer as rpb 


    if args.kb == 'qlearning3.pl':
        subjects = ['a', 'loc', 'up', 'down', 'left', 'right']
    else:
            subjects = ['a', 'f', 'g', 'l']
    if args.train != "NONE":
        assert(args.reload == "NONE")
        print('Training; model name is ', args.train)
        model_name = args.train
        network = train(args.episode_length, args.train, True, args.num_episodes, train_interval=args.train_interval, update_target_network_interval=args.target_update_interval, gnn_nodes = args.gnn_nodes, exhaustive_training=args.exhaustive, kb=args.kb, subjects=subjects)
        use_net = True
    if args.reload != "NONE":
        assert(args.train == "NONE")

        model_name = args.reload
        #network = rpb_dqn(args.episode_length * 2, ['a', 'f', 'g'], [], use_gnn=args.gnn) #TODO: Should be able to read most of this from pkl file.
        network = rpb_dqn(10000, subjects, [], use_gnn=args.gnn) #TODO: Should be able to read most of this from pkl file.
        if "qlearning3.pl" in args.kb:
            network.reward_fn = get_rewards_test3
        else:
            netwrok.reward_fn = get_rewards_test1
        network.model_load(model_name)

    if args.random_network:
        network = rpb_dqn(args.episode_length * 2, subjects, [],
                          use_gnn=args.gnn)  # TODO: Should be able to read most of this from pkl file.
        model_name = ""


    res = test(network, args.kb, args.episode_length)

    print("Final rewards: ", res['rewards'])



#main()

#def test1():
#    train(5, "test1", True, 40, 10, 20, 2)
#test1()
if __name__ == "__main__":
  main()


