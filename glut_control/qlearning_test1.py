"""
Use graph representations to learn a Q-function with an abstract axiom set.
Forked from test3.py
"""
import os
_CPU_ONLY=True
if _CPU_ONLY:
    os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
        
import random
import numpy as np
import itertools
import pickle
import gc
from importlib import reload
import alma, alma_utils
import argparse
import rl_utils, rl_dataset
import resolution_prebuffer as rpb
from rpb_dqn import rpb_dqn

test_params = {
    'explosion_size': 1000,
    'alma_heap_print_size': 100
}
alma_inst,res = alma.init(1,'/home/justin/alma-2.0/glut_control/qlearning1.pl', '0', 1, 1000, [], [])

def res_task_lits(lit_str):
    L = lit_str.split('\n')[:-1]
    return [ x.split('\t') for x in L]

def train(num_steps=50, model_name="test1", use_gnn = True, num_episodes=100000, train_interval=500, update_target_network_interval=2000, debug_level=0, pretrained_network = None):
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
    global alma_inst,res
    subjects = ['a', 'f', 'g']
    network = rpb_dqn(subjects, [], use_gnn=use_gnn)  if pretrained_network is None else pretrained_network

    replay_buffer = rl_dataset.experience_replay_buffer()
    for episode in range(network.start_episode, num_episodes):
        print("Starting episode ", episode)
        rl.training_current = episode
        alma_inst,res = alma.init(1,'/home/justin/alma-2.0/glut_control/qlearning1.pl', '0', 1, 1000, [], [])
        rl_utils.collect_episode(network, replay_buffer, alma_inst, num_steps)
        if (episode % train_interval == 0) and (episode > 0):
            rl_utils.replay_train(network, replay_buffer)
            if episode % update_target_network_interval == 0:
                network.target_model.set_weights(network.current_model.get_weights())

        if debug_level > 0:
            print("episode: ", episode)
            alma_utils.prebuf_print(alma_inst)
            alma_utils.pr_heap_print(alma_inst)
            alma_utils.kb_print(alma_inst)

    network.model_save(model_name)

def test(network_priors, exp_size=10, num_steps=500, alma_heap_print_size=100, prb_print_size=30, numeric_bits=10, heap_print_freq=10, model_name='test1', prb_threshold=-1, use_gnn = False):
    global alma_inst,res
    dbb_instances = []
    exp = explosion(exp_size)
    res_tasks = exp[0]
    if len(res_tasks) == 0:
        return []
    res_lits = res_task_lits(exp[2])
    #subjects = ['a0', 'a1', 'b0', 'b1']
    # Compute initial subjects.  We want 'a','b' and first 8K integers in each of three places
    subjects = []
    for place in range(3):
        for cat_subj in ['a', 'b']:
            subjects.append("{}/{}".format(cat_subj, place))
        for num_subj in range(2**numeric_bits):
            subjects.append("{}/{}".format(num_subj, place))
    res_task_input = [ x[:2] for x in res_tasks]
    if network_priors:
        network = rpf.rpf_load(model_name)
        priorities = 1 - network.get_priorities(res_task_input)
    else:
        priorities = np.random.uniform(size=len(res_task_input))

    alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
    alma.prb_to_res_task(alma_inst, prb_threshold)

    print("prb: ", alma.prebuf(alma_inst))
    kb = alma.kbprint(alma_inst)[0]
    print("kb: ")
    for s in kb.split('\n'):
        print(s)
    for idx in range(num_steps):
        prb = alma.prebuf(alma_inst)[0]
        if (idx % heap_print_freq == 0):
            print("Step: ", idx)
            print("prb size: ", len(prb))
            for fmla in prb:
                print(fmla)
            print("\n"*3)
            print("KB:")
            for fmla in alma.kbprint(alma_inst)[0].split('\n'):
                print(fmla)
                if ': distanceBetweenBoundedBy' in fmla:
                    dbb_instances.append(fmla)
            print("DBB {}: {}".format(idx, len(dbb_instances)))

            rth = alma.res_task_buf(alma_inst)
            print("Heap:")
            print("HEAP size {}: {} ".format(idx, len(rth[1].split('\n')[:-1])))
            for i, fmla in enumerate(rth[1].split('\n')[:-1]):
                pri = rth[0][i][-1]
                print("i={}:\t{}\tpri={}".format(i, fmla, pri))
                if i >  alma_heap_print_size:
                    break
            print("-"*80)

        if len(prb) > 0:
            res_task_input = [x[:2] for x in prb]
            priorities = 1 - network.get_priorities(res_task_input)  if network_priors else   np.random.uniform(size=len(res_task_input))
            alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
            alma.prb_to_res_task(alma_inst, prb_threshold)
        #alma.add(alma_inst, "distanceAt(a, {}, {}).".format(idx, idx))
        alma.astep(alma_inst)
    return dbb_instances

def main():
    global rpf
    
    parser = argparse.ArgumentParser(description="Run test with specified parameters.")
    parser.add_argument("explosion_steps", type=int, default=10)
    parser.add_argument("reasoning_steps", type=int, default=500)
    parser.add_argument("--heap_print_size", type=int, default=0)
    parser.add_argument("--prb_print_size", type=int, default=0)
    parser.add_argument("--numeric_bits", type=int, default=10)
    parser.add_argument("--prb_threshold", type=float, default=1.0)
    parser.add_argument("--use_network", action='store_true')
    parser.add_argument("--retrain", action='store', required=False, default="NONE")
    parser.add_argument("--num_trainings", type=int, default=10000)
    parser.add_argument("--train_interval", type=int, default=500)
    parser.add_argument("--reload", action='store', required=False, default="NONE")
    parser.add_argument("--gnn", action='store_true')
    parser.add_argument("--cpu_only", action='store_true')

    args = parser.parse_args()
    #use_net = True if args.use_network == "True" else False
    use_net = args.use_network
    assert(type(use_net) == type(True))

    if args.cpu_only:
        # TODO:  This may need to be done before any tensorflow imports
        os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
    import resolution_prefilter as rpf
    
    if args.retrain != "NONE":
        model_name = args.retrain
        assert(args.reload == "NONE")
        print("Read network {}, expsteps {}   rsteps {}    model_name {}".format(use_net, args.explosion_steps, args.reasoning_steps, model_name))
        print('Retraining; model name is ', args.retrain)
        train(args.explosion_steps, args.reasoning_steps, args.numeric_bits, model_name, args.gnn, args.num_trainings, args.train_interval)
    if args.reload != "NONE":
        assert(args.retrain == "NONE")
        model_name = args.reload
        print("Read network {}, expsteps {}   rsteps {}    model_name {}".format(use_net, args.explosion_steps, args.reasoning_steps, model_name))
    

    #res = test(use_net, args.explosion_steps, args.reasoning_steps, args.heap_print_size, args.prb_print_size, args.numeric_bits, heap_print_freq=10, model_name = model_name, prb_threshold=0.25)
    res = test(use_net, args.explosion_steps, args.reasoning_steps, args.heap_print_size, args.prb_print_size, args.numeric_bits, heap_print_freq=1, model_name = model_name, prb_threshold=args.prb_threshold, gnn=args.gnn)
    print("Final result is", res)
    print("Final number is", len(res))


#main()

def test1():
    train(5, "test1", True, 40, 10, 20, 2)
test1()
if __name__ == "__main__":
    main()

