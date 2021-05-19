"""
Start experimenting with graph representations.  Based off test1.py
"""
_CPU_ONLY=True

import random
import os
from guppy import hpy

if _CPU_ONLY:
    os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
        
import rl_utils, resolution_prebuffer
import numpy as np
import itertools
import pickle
import gc
from importlib import reload
import alma
from alma_utils import *
import argparse
from memory_profiler import profile
import random

#os.environ["LD_LIBRARY_PATH"] = "/home/justin/alma-2.0/"
test_params = {
    'explosion_size': 1000,
    'alma_heap_print_size': 100
}
alma_inst,res = alma.init(1,'/home/justin/alma-2.0/glut_control/test1_kb.pl', '0', 1, 1000, [], [])

def res_task_lits(lit_str):
    L = lit_str.split('\n')[:-1]
    return [ x.split('\t') for x in L]

def explosion(size, kb):
    """
    The one axiom we work with:
       if(and(distanceAt(Item1, D1, T), distanceAt(Item2, D2, T)), distanceBetweenBoundedBy(D1, Item1, Item2, T)).
    """
    global alma_inst,res
    ilist = list(range(size))
    random.shuffle(ilist)
    for i in ilist:
        print("i=", i)
        if "test1_kb.pl" in kb:
            obs_fmla_a = "distanceAt(a, {}, {}).".format(random.randint(0, 10), i)
            obs_fmla_b = "distanceAt(b, {}, {}).".format(random.randint(0, 10), i)
            alma.add(alma_inst, obs_fmla_a)
            alma.add(alma_inst, obs_fmla_b)
        elif "january_preglut.pl" in kb:
            obs_fmla = "location(a{}).".format(i)
            alma.add(alma_inst, obs_fmla)
        r = alma.prebuf(alma_inst)
        alma.astep(alma_inst)
    print("Explosion done.")
    print("="*80)
    return r

def generate_epoch(num_steps, network, alma_inst):
        for idx in range(num_steps):
            prb = alma.prebuf(alma_inst)
            res_tasks = prb[0]
            #print("idx={}\tnum(res_tasks)={}".format(idx, len(prb[0])), end=" ")
            if len(res_tasks) > 0:
                #res_lits = prb[1]
                res_lits = res_task_lits(prb[2])
                res_task_input = [x[:2] for x in res_tasks]
                #network.train_batch(res_task_input, res_lits)
                network.save_batch(res_task_input, res_lits)
                priorities = 1 - network.get_priorities(res_task_input)
                #print("len(priorirites)={}".format(len(priorities)), end=" ")
                alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
                alma.prb_to_res_task(alma_inst, 1.0)

#@profile
def train(explosion_steps=50, num_steps=500, numeric_bits=3, model_name="test1", use_gnn = False, num_trainings=1000, train_interval=500, kb='/home/justin/alma-2.0/glut_control/test1_kb.pl', net_clear=1000, gnn_nodes = 50):
    global alma_inst,res
    #hp = hpy()
    #hpy_before = hp.heap()
    if "test1_kb.pl" in kb:
        subjects = ['a', 'b', 'distanceAt', 'distanceBetweenBoundedBy']
    elif "january_preglut.pl" in kb:
        subjects = ["a{}".format(x) for x in range(explosion_steps)]

    if "test1_kb.pl" in kb and not use_gnn:
        for place in range(3):
            for cat_subj in ['a', 'b']:
                subjects.append("{}/{}".format(cat_subj, place))
            for num_subj in range(2**numeric_bits):
                subjects.append("{}/{}".format(num_subj, place))
    network = resolution_prebuffer.res_prebuffer(subjects, [], use_gnn=use_gnn, gnn_nodes = gnn_nodes)
    for b in range(num_trainings):
        print("Starting round ", b)
        if b > 0 and (b% 250 == 0):
            print("Saving checkpoint {}".format(b))
            network.model_save(model_name+"ckpt{}".format(b), numeric_bits)
        alma_inst,res = alma.init(1,kb, '0', 1, 1000, [], [])
        exp = explosion(explosion_steps, kb)
        res_tasks = exp[0]
        res_lits = res_task_lits(exp[2])
        #res_lits = exp[1]
        res_task_input = [ x[:2] for x in res_tasks]
        #network.train_batch(res_task_input, res_lits)
        print("Network has {} samples, {} of which are positive".format(len(network.ybuffer), network.ypos_count))
        print("Cleaning network...")
        network.clean()
        print("Network has {} samples, {} of which are positive".format(len(network.ybuffer), network.ypos_count))
        next_clear = net_clear
        for idx in range(num_steps):
            prb = alma.prebuf(alma_inst)
            res_tasks = prb[0]
            #print("idx={}\tnum(res_tasks)={}".format(idx, len(prb[0])), end=" ")
            if len(res_tasks) > 0:
                #res_lits = prb[1]
                res_lits = res_task_lits(prb[2])
                res_task_input = [x[:2] for x in res_tasks]
                #network.train_batch(res_task_input, res_lits)
                network.save_batch(res_task_input, res_lits)
                if idx >= next_clear:
                    print("Network has {} samples, {} of which are positive".format(len(network.ybuffer), network.ypos_count))
                    print("Cleaning network...")
                    network.clean()
                    print("Network has {} samples, {} of which are positive".format(len(network.ybuffer), network.ypos_count))
                    next_clear += net_clear
                priorities = 1 - network.get_priorities(res_task_input)
                #print("len(priorirites)={}".format(len(priorities)), end=" ")
                alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
                alma.prb_to_res_task(alma_inst, 1.0)
            #print()
            #alma.add(alma_inst, "distanceAt(a, {}, {}).".format(idx, idx))
            if idx > 0 and idx % train_interval == 0:
                print("Network has {} samples, {} of which are positive".format(len(network.ybuffer), network.ypos_count))
                if (network.ypos_count > (network.batch_size  / 2)) and (network.yneg_count > (network.batch_size  / 2)):
                    H = network.train_buffered_batch()
                    acc, loss = H.history['accuracy'][0], H.history['loss'][0]
                    #print("accuracy: {} \t loss: {}\ttacc: {}".format(acc, loss, H.history['tacc'][0]))
                    if acc > 0.8:
                        print("acc > 0.8")
                    if acc > 0.999 and loss < 1e-5:
                        print("Good model; ending early")
                        network.model_save(model_name, numeric_bits)
                        return
                else:
                    print("Continuing...")
                    continue
                hs = heap_size(alma_inst)
                print("Heap size is: ", hs)
                if hs < 100:
                    print("Refreshing the heap.")
                    _ = explosion(explosion_steps, kb)
                
            alma.astep(alma_inst)
        del alma_inst
        del prb
        del exp
        del res
        del res_lits
        del res_tasks
        del res_task_input


    network.model_save(model_name, numeric_bits)
    #hpy_after  = hp.heap()
    #hpy_diff = hpy_after - hpy_before
    # alma_inst, res = alma.init(1, kb, '0', 1, 1000, [], [])
    # exp = explosion(explosion_steps, kb)
    # for i in range(10):
    #     if heap_size(alma_inst) < 100:
    #         print("Refreshing the heap.")
    #         _ = explosion(explosion_steps, kb)
    #     test_network(network, num_steps, kb)
    return network

def test_network(network, num_steps, kb):
    global alma_inst
    while (network.ypos_count <= (network.batch_size  / 2)) or (network.yneg_count <= (network.batch_size  / 2)):
        generate_epoch(num_steps, network, alma_inst)
        print("pos -- {}  \t neg -- {}".format(network.ypos_count, network.yneg_count))
        alma.astep(alma_inst)
        if heap_size(alma_inst) < 100:
            print("Refreshing the heap.")
            _ = explosion(50, kb)
    H = network.test_buffered_batch()
    acc, loss = H.history['accuracy'][0], H.history['loss'][0]
    print("Testing:  accuracy: {} \t loss: {} \t tacc {}".format(acc, loss, H.history['tacc'][0]))


def test(network, network_priors, exp_size=10, num_steps=500, alma_heap_print_size=100, prb_print_size=30, numeric_bits=10, heap_print_freq=10, prb_threshold=-1, use_gnn = False, kb='/home/justin/alma-2.0/glut_control/test1_kb.pl', gnn_nodes=2000, initial_test=False):
    global alma_inst,res
    alma_inst,res = alma.init(1,kb, '0', 1, 1000, [], [])
    dbb_instances = []
    exp = explosion(exp_size, kb)
    res_tasks = exp[0]
    if len(res_tasks) == 0:
        return []
    res_lits = res_task_lits(exp[2])
    #subjects = ['a0', 'a1', 'b0', 'b1']
    # Compute initial subjects.  We want 'a','b' and first 8K integers in each of three places
    subjects = []
    if "test1_kb.pl" in kb and not use_gnn:
        for place in range(3):
            for cat_subj in ['a', 'b']:
                subjects.append("{}/{}".format(cat_subj, place))
            for num_subj in range(2 ** numeric_bits):
                subjects.append("{}/{}".format(num_subj, place))
    # for place in range(3):
    #     for cat_subj in ['a', 'b']:
    #         subjects.append("{}/{}".format(cat_subj, place))
    #     for num_subj in range(2**numeric_bits):
    #         subjects.append("{}/{}".format(num_subj, place))

    if "test1_kb.pl" in kb:
        subjects = ['a', 'b', 'distanceAt', 'distanceBetweenBoundedBy']
    elif "january_preglut.pl" in kb:
        subjects = ["a{}".format(x) for x in range(exp_size)]

    if initial_test:
        for _ in range(10):
            test_network(network, num_steps, kb)
        
    res_task_input = [ x[:2] for x in res_tasks]
    if network_priors:
        priorities = 1 - network.get_priorities(res_task_input)
    else:
        priorities = np.random.uniform(size=len(res_task_input))

    alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
    alma.prb_to_res_task(alma_inst, prb_threshold)

    #print("prb: ", alma.prebuf(alma_inst))
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
    parser = argparse.ArgumentParser(description="Run test with specified parameters.")
    parser.add_argument("explosion_steps", type=int, default=10)
    parser.add_argument("reasoning_steps", type=int, default=500)
    parser.add_argument("testing_reasoning_steps", type=int, default=1000)
    parser.add_argument("--heap_print_size", type=int, default=0)
    parser.add_argument("--prb_print_size", type=int, default=0)
    parser.add_argument("--numeric_bits", type=int, default=10)
    parser.add_argument("--prb_threshold", type=float, default=1.0)
    parser.add_argument("--use_network", action='store_true')
    parser.add_argument("--train", action='store', required=False, default="NONE")
    parser.add_argument("--num_trainings", type=int, default=100)
    parser.add_argument("--train_interval", type=int, default=500)
    parser.add_argument("--reload", action='store', required=False, default="NONE")
    parser.add_argument("--gnn_nodes", type=int, default=50)
    parser.add_argument("--kb", action='store', required=False, default="test1_kb.pl")
    parser.add_argument("--gnn", action='store_true')
    parser.add_argument("--cpu_only", action='store_true')

    network = None
    args = parser.parse_args()
    print("Run with args:", args)
    #use_net = True if args.use_network == "True" else False
    use_net = args.use_network
    assert(type(use_net) == type(True))

    if args.cpu_only:
        # TODO:  This may need to be done before any tensorflow imports
        os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
        
    if args.train != "NONE":
        model_name = args.train
        assert(args.reload == "NONE")
        print("Using network: {} with expsteps {}   rsteps {}    model_name {}".format(use_net, args.explosion_steps, args.reasoning_steps, model_name))
        print('Training; model name is ', args.train)
        network = train(args.explosion_steps, args.reasoning_steps, args.numeric_bits, model_name, args.gnn, args.num_trainings, args.train_interval, args.kb, gnn_nodes=args.gnn_nodes)
    if args.reload != "NONE":
        assert(args.train == "NONE")
        model_name = args.reload
        print("Using network: {}, expsteps {}   rsteps {}    model_name {}".format(use_net, args.explosion_steps, args.reasoning_steps, model_name))
        network = resolution_prebuffer.rpf_load(model_name, True)

    

    #res = test(use_net, args.explosion_steps, args.reasoning_steps, args.heap_print_size, args.prb_print_size, args.numeric_bits, heap_print_freq=10, model_name = model_name, prb_threshold=0.25)
    print("-"*80)
    print("BEGIN TESTING")
    print("-"*80)
    res = test(network, use_net, args.explosion_steps, args.testing_reasoning_steps, args.heap_print_size, args.prb_print_size, args.numeric_bits,
               heap_print_freq=1, prb_threshold=args.prb_threshold, use_gnn=args.gnn, kb=args.kb, gnn_nodes=args.gnn_nodes, initial_test=False)
    print("Final result is", res)
    print("Final number is", len(res))


#main()
if __name__ == "__main__":
    main()

