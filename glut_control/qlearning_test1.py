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
from rpb_dqn import rpb_dqn, get_rewards_test1, get_rewards_test3, get_bookshelf_rewards
import time
#import tracemalloc
import multiprocessing

_use_mp = False
_reuse_buffer = False

test_params = {
    'explosion_size': 1000,
    'alma_heap_print_size': 100
}
#alma_inst,res = alma.init(1,'/home/justin/alma-2.0/glut_control/qlearning2.pl', '0', 1, 1000, [], [])
alma_inst,res = None, None

def res_task_lits(lit_str):
    L = lit_str.split('\n')[:-1]
    return [ x.split('\t') for x in L]

def mp_collect_episode(network,  num_steps, kb, num_episodes, q):
    import alma

    replay_buffer = rl_dataset.experience_replay_buffer()
    for i in range(num_episodes):
        print("i = {}".format(i), end="\r")
        alma_inst,res = alma.init(1,kb, '0', 1, 1000, [], [])
        rl_utils.collect_episode(network, replay_buffer, alma_inst, num_steps)
        alma.halt(alma_inst)
    print("done!", end="\r")
    q.put(( replay_buffer.states0, replay_buffer.actions, replay_buffer.rewards, replay_buffer.states1))
    print("done put!")

#@profile
def train(num_steps=50, model_name="test1", use_gnn = True, num_episodes=100000, train_interval=500, update_target_network_interval=10000, debug_level=0, gnn_nodes = 20, exhaustive_training=False, kb='/home/justin/alma-2.0/glut_control/qlearning2.pl', subjects = ['a', 'f', 'g', 'l'], prior_network = None, max_reward=10000, eval_interval=2500):
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
    elif "bookshelf1.pl" in kb:
        reward_fn = get_bookshelf_rewards
    else:
        reward_fn = get_rewards_test1
    network = rpb_dqn(max_reward, reward_fn, subjects, [], use_gnn=use_gnn, gnn_nodes=gnn_nodes) if prior_network is None else prior_network    # Use max_reward of 10K
    replay_buffer = rl_dataset.experience_replay_buffer()
    start_time = time.time()
    episode_step = train_interval if _use_mp else 1
    #episode_step = 3 # debugging only

    _no_collect = False
    for episode in range(network.starting_episode, network.starting_episode + num_episodes, episode_step):
        print("Starting episode ", episode)
        network.current_episode = episode
        #del alma
        if _use_mp:
            with multiprocessing.Manager() as manager:
                #collection_queue=multiprocessing.Queue()
                collection_queue=manager.Queue()
                collect_process = multiprocessing.Process(target=mp_collect_episode, args = (network, num_steps, kb, episode_step, collection_queue))
                collect_process.start()
                collect_process.join()
                s, a, r, s2 = collection_queue.get()
                replay_buffer.extend(s, a, r, s2)
                        
        else:
            if _no_collect:
                print("false collection.\r")
            else:
                reload(alma)
                alma_inst,res = alma.init(1,kb, '0', 1, 1000, [], [])
                rl_utils.collect_episode(network, replay_buffer, alma_inst, num_steps)
                alma.halt(alma_inst)
        if (episode % train_interval == 0) and (episode > 0):
            if _reuse_buffer:
                rb_backup = replay_buffer.copy()
            total_reward = np.sum(np.maximum(0, replay_buffer.rewards))
            if total_reward > 0:
                print("Training with total reward of {}".format(total_reward))
                rl_utils.replay_train(network, replay_buffer, exhaustive_training)
            else:
                print("Total reward is 0")
            if episode % update_target_network_interval == 0:
                print("Updating at: ", time.time() - start_time)
                network.target_model.model.set_weights(network.current_model.model.get_weights())
            if _reuse_buffer:
                _no_collect = True
                replay_buffer = rb_backup.copy()
        if episode % 200 == 0 and False:
            del replay_buffer
            replay_buffer = rl_dataset.experience_replay_buffer()


        if episode % eval_interval == 0:
            if not _no_collect:
                res = test(network, kb, num_steps)
                print("-"*80)
                print("Rewards at episode {} (epsilon=={}): {}".format(episode, network.epsilon, res['rewards']))
                print("Prebuf sizes at episode {}: {}".format(episode, [len(x) for x in res['prebuf']]))
                print("Heap sizes at episode {}: {}".format(episode, [len(x) for x in res['heap']]))

            network.model_save(model_name + "_ckpt" + str(episode))
            print("-"*80)


        if debug_level > 2:
            print("episode: ", episode)
            alma_utils.prebuf_print(alma_inst)
            alma_utils.pr_heap_print(alma_inst)
            alma_utils.kb_print(alma_inst)

        network.epsilon_decay()

    network.model_save(model_name)
    return network

def test(network, kb, num_steps=500):
    global alma_inst,res
    alma_inst, res = alma.init(1,kb, '0', 1, 1000, [], [])
    play = rl_utils.play_episode(network, alma_inst, num_steps)
    return play


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
    parser.add_argument("--eval_interval", type=int, default=2500)
    parser.add_argument("--reload", action='store', required=False, default="NONE")
    parser.add_argument("--gnn", action='store_true')
    parser.add_argument("--gnn_nodes", type=int, default=50)
    parser.add_argument("--cpu_only", action='store_true')
    parser.add_argument("--exhaustive", action='store_true')
    parser.add_argument("--kb", default='/home/justin/alma-2.0/glut_control/qlearning2.pl', action='store')

    args = parser.parse_args()
    print("Running with arguments ", args)

    if args.cpu_only:
        # TODO:  This may need to be done before any tensorflow imports
        os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
    import resolution_prebuffer as rpb 

    max_reward = 10000
    if args.kb == 'qlearning3.pl':
        subjects = ['a', 'loc', 'up', 'down', 'left', 'right']
    elif ("bookshelf1.pl" in args.kb) or ("bookshelf2.pl" in args.kb):
        subjects = [ 'isA',
                     'bookshelf',
                     'contains',
                     'book',
                     'sequel',
                     'prequel',
                     'thisShelf',
                     'furniture',
                     'table',
                     'legs',
                     'affords',
                     'sitting',
                     'placing',
                     'desire',
                     'find',
                     'myNovel',
                     'lookFor']
        max_reward = 1
    else:
        subjects = ['a', 'f', 'g', 'l']

    network = None
    if args.reload != "NONE":
        model_name = args.reload
        #network = rpb_dqn(args.episode_length * 2, ['a', 'f', 'g'], [], use_gnn=args.gnn) #TODO: Should be able to read most of this from pkl file.
        network = rpb_dqn(0, subjects, [], use_gnn=args.gnn) #TODO: Check that, e.g. max_reward reloads correctly from pkl file.
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
            network = train(args.episode_length, args.train, True, args.num_episodes, train_interval=args.train_interval, update_target_network_interval=args.target_update_interval, gnn_nodes = args.gnn_nodes, exhaustive_training=args.exhaustive, kb=args.kb, subjects=subjects, max_reward=max_reward, eval_interval=args.eval_interval)
        else:
            network = train(args.episode_length, args.train, True, args.num_episodes, train_interval=args.train_interval, update_target_network_interval=args.target_update_interval, gnn_nodes = args.gnn_nodes, exhaustive_training=args.exhaustive, kb=args.kb, subjects=subjects, prior_network=network, max_reward=max_reward, eval_interval=args.eval_interval)
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


