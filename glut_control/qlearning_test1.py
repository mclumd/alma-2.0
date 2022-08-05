"""
Use graph representations to learn a Q-function with an abstract axiom set.
Forked from test3.py
"""
import os
import random
import numpy as np
import itertools
import pickle
import math
import gc
gc.disable()

from importlib import reload
import alma, alma_utils
import argparse
import rl_utils, rl_dataset
#import resolution_prebuffer as rpb
from rpb_dqn import rpb_dqn, get_rewards_test1, get_rewards_test3
import time, datetime
#import tracemalloc
#from memory_profiler import profile
import psutil
import rl_transformer
import rl_transformer.rl_transformer
from rl_transformer.rl_transformer import rl_transformer



test_params = {
    'explosion_size': 1000,
    'alma_heap_print_size': 100
}
#alma_inst,res = alma.init(1,'/home/justin/alma-2.0/glut_control/qlearning2.pl', '0', 1, 1000, [], [])
alma_inst,res = None, None

def res_task_lits(lit_str):
    L = lit_str.split('\n')[:-1]
    return [ x.split('\t') for x in L]

def print_gpu_mem():
    print(tf.config.experimental.get_memory_info('GPU:0'))
    print(tf.config.experimental.get_memory_info('GPU:1'))
#@profile
def train(num_steps=50, model_name="test1", use_gnn = True, num_episodes=100000, train_interval=500,
          update_target_network_interval=10000, debug_level=0, gnn_nodes = 20,
          exhaustive_training=False, kb='/home/justin/alma-2.0/glut_control/qlearning2.pl',
          subjects = ['a', 'f', 'g', 'l', 'now'], prior_network = None, debugging=False,
          testing_interval=math.inf, tboard = False, transformer=False,
          device="cpu"):
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
    if "qlearning3.pl" in kb or "qlearning4.pl" in kb:
        reward_fn = get_rewards_test3
    else:
        reward_fn = get_rewards_test1

    if prior_network is not None:
        network = prior_network
    elif transformer:
        network = rl_transformer(max_reward=100,
                                 reward_fn=reward_fn,
                                 debugging=debugging)
    else:
        network = rpb_dqn(100, reward_fn,
                          subjects, [], use_gnn=use_gnn,
                          gnn_nodes=gnn_nodes, use_state=True,
                          debugging=debugging,
                          pytorch_backend = use_pytorch,
                          transformer_based=transformer) # Use max_reward of 10K
        
    replay_buffer = rl_dataset.experience_replay_buffer()
    start_time = time.time()
    if tboard:
        reward_log_dir = 'logs/rewards/' + datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        reward_summary_writer = tf.summary.create_file_writer(reward_log_dir)
    for episode in range(network.starting_episode, network.starting_episode + num_episodes):
        print("Starting episode ", episode)
        network.current_episode = episode
        #del alma
        reload(alma)
        alma_inst,res = alma.init(1,kb, '0', 1, 1000, [], [])
        # print("Pre-collect")
        if "cuda" in device:
            print_gpu_mem()
        rl_utils.collect_episode(network, replay_buffer, alma_inst, num_steps)
        alma.halt(alma_inst)
        #print("Post-collect")
        if "cuda" in device:
            print_gpu_mem()
        if (episode % train_interval == 0) and (episode > 0):
            #print("Pre-train")
            if "cuda" in device:
                print_gpu_mem()
            rl_utils.replay_train(network, replay_buffer, exhaustive_training)
            #print("Post-train")
            if "cuda" in device:
                print_gpu_mem()
            if episode % update_target_network_interval == 0:
                print("Updating at: {}  \t Memory usage: {} \t   Replay buffer size: {} ".format( time.time() - start_time,
                                                                                                  psutil.Process().memory_info().rss / (1024 * 1024),
                                                                                                  replay_buffer.num_entries()))
                #network.target_model.model.set_weights(network.current_model.model.get_weights())
                network.update_target()
        if episode % 200 == 0:
            del replay_buffer
            replay_buffer = rl_dataset.experience_replay_buffer()


        if episode % testing_interval == 0:
            print("Pre-test")
            if "cuda" in device:
                print_gpu_mem()
            res = test(network, kb, num_steps)
            print("Post-test")
            if "cuda" in device:
                print_gpu_mem()
            print("-"*80)
            print("Rewards at episode {} (epsilon=={}): {}".format(episode, network.epsilon, res['rewards']))
            network.model_save("test_models_delete", model_name + "_ckpt" + str(episode))
            print("-"*80)
            if tboard:
                with reward_summary_writer.as_default():
                    for (i, rew) in enumerate(res['rewards']):
                        tf.summary.scalar("reward{}".format(i), rew, step=episode)


        if debug_level > 2:
            print("episode: ", episode)
            alma_utils.prebuf_print(alma_inst)
            alma_utils.pr_heap_print(alma_inst)
            alma_utils.kb_print(alma_inst)

        network.epsilon_decay()

    network.model_save("test_models_delete", model_name)
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
    parser.add_argument("--testing_interval", help="Test model during training", type=int, default=1000)
    parser.add_argument("--reload", action='store', required=False, default="NONE")
    parser.add_argument("--gnn", action='store_true')
    parser.add_argument("--gnn_nodes", type=int, default=50)
    parser.add_argument("--cpu_only", action='store_true')
    parser.add_argument("--exhaustive", action='store_true')
    parser.add_argument("--kb", default='/home/justin/alma-2.0/glut_control/qlearning2.pl', action='store')
    parser.add_argument("--debugging", action='store_true')
    parser.add_argument("--pytorch", help="use pytorch as backend (default is tensorflow)", action='store_true')
    parser.add_argument("--transformer", help="use BERT-like transformer", action="store_true")

    args = parser.parse_args()
    print("Running with arguments ", args)

    if args.cpu_only:
        # TODO:  This may need to be done before any tensorflow imports
        os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
    import resolution_prebuffer as rpb 

    global use_tensorflow
    global use_pytorch
    use_pytorch = args.pytorch
    use_tensorflow = not use_pytorch
    

    if use_tensorflow:
        import tensorflow as tf
        physical_devices = tf.config.list_physical_devices('GPU') 
        tf.config.experimental.set_memory_growth(physical_devices[0], True)
        tf.config.experimental.set_memory_growth(physical_devices[1], True)
    else:
        import torch



    if args.kb == 'qlearning3.pl':
        subjects = ['a', 'loc', 'up', 'down', 'left', 'right']
    else:
            subjects = ['a', 'f', 'g', 'l']

    subjects.append('now')
    network = None
    if args.reload != "NONE":
        model_name = args.reload
        #network = rpb_dqn(args.episode_length * 2, ['a', 'f', 'g'], [], use_gnn=args.gnn) #TODO: Should be able to read most of this from pkl file.
        network = rpb_dqn(10000, subjects, [], use_gnn=args.gnn) #TODO: Should be able to read most of this from pkl file.
        if "qlearning3.pl" in args.kb:
            network.reward_fn = get_rewards_test3
        else:
            network.reward_fn = get_rewards_test1
        network.model_load(model_name)
        network.reset_start()
            
    if args.train != "NONE":
        print('Training; model name is ', args.train)
        model_name = args.train
        if network is None:
            network = train(args.episode_length,
                            args.train,
                            True,
                            args.num_episodes,
                            train_interval=args.train_interval,
                            update_target_network_interval=args.target_update_interval,
                            gnn_nodes = args.gnn_nodes,
                            exhaustive_training=args.exhaustive,
                            kb=args.kb,
                            subjects=subjects,
                            debugging=args.debugging,
                            testing_interval=args.testing_interval,
                            transformer=args.transformer
                            )
        else:
            network = train(args.episode_length,
                            args.train,
                            True,
                            args.num_episodes,
                            train_interval=args.train_interval,
                            update_target_network_interval=args.target_update_interval,
                            gnn_nodes = args.gnn_nodes,
                            exhaustive_training=args.exhaustive,
                            kb=args.kb,
                            subjects=subjects,
                            prior_network=network,
                            testing_interval=args.testing_interval,
                            transformer=args.transformer)
        use_net = True
    
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


