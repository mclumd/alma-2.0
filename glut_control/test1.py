"""
Define a simple knowledge base that gives combinatorial explosion.
Train a neural network to control the explosion.
Collect data and report results.

"""


import random
import os
import rl_functions
import numpy as np
import itertools
import pickle
import gc
from importlib import reload
import alma
import argparse

#os.environ["LD_LIBRARY_PATH"] = "/home/justin/alma-2.0/"
test_params = {
    'explosion_size': 1000,
    'alma_heap_print_size': 100
}
alma_inst,res = alma.init(1,'/home/justin/alma-2.0/glut_control/test1_kb.pl', '0', 1, 1000, ['a', 'b'], [0.5]*6)

def res_task_lits(lit_str):
    L = lit_str.split('\n')[:-1]
    return [ x.split('\t') for x in L]

def explosion(size=1000):
    """
    The one axiom we work with:
       if(and(distanceAt(Item1, D1, T), distanceAt(Item2, D2, T)), distanceBetweenBoundedBy(D1, Item1, Item2, T)).
    """
    global alma_inst,res
    for i in range(size):
        print("i=", i)
        obs_fmla_a = "distanceAt(a, {}, {}).".format(random.randint(0, 100), i)
        obs_fmla_b = "distanceAt(b, {}, {}).".format(random.randint(0, 100), i)
        alma.add(alma_inst, obs_fmla_a)
        alma.add(alma_inst, obs_fmla_b)
        r = alma.prebuf(alma_inst)
        alma.astep(alma_inst)
    print("Explosion done.")
    print("="*80)
    return r

def train(explosion_steps=50, num_steps=500):
    subjects = ['a0', 'a1', 'b0', 'b1']
    network = rl_functions.res_prefilter(subjects, [])
    for b in range(2):
        exp = explosion(explosion_steps)
        res_tasks = exp[0]
        res_lits = res_task_lits(exp[2])
        #res_lits = exp[1]
        res_task_input = [ x[:2] for x in res_tasks]
        network.train_batch(res_task_input, res_lits)
        for idx in range(num_steps):
            prb = alma.prebuf(alma_inst)
            res_tasks = prb[0]
            if len(res_tasks) > 0:
                #res_lits = prb[1]
                res_lits = res_task_lits(prb[2])
                res_task_input = [x[:2] for x in res_tasks]
                network.train_batch(res_task_input, res_lits)
                priorities = 1 - network.get_priorities(res_task_input)
                alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
                alma.prb_to_res_task(alma_inst)
            #alma.add(alma_inst, "distanceAt(a, {}, {}).".format(idx, idx))
            alma.astep(alma_inst)

    network.model_save('test1')

def test(network_priors, exp_size=10, num_steps=500, alma_heap_print_size=100):
    global alma_inst,res
    dbb_instances = []
    exp = explosion(exp_size)
    res_tasks = exp[0]
    if len(res_tasks) == 0:
        return []
    res_lits = res_task_lits(exp[2])
    subjects = ['a0', 'a1', 'b0', 'b1']
    res_task_input = [ x[:2] for x in res_tasks]
    if network_priors:
        network = rl_functions.rpf_load('test1')
        priorities = 1 - network.get_priorities(res_task_input)
    else:
        priorities = np.random.uniform(size=len(res_task_input))

    alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
    alma.prb_to_res_task(alma_inst)

    print("prb: ", alma.prebuf(alma_inst))
    kb = alma.kbprint(alma_inst)[0]
    print("kb: ")
    for s in kb.split('\n'):
        print(s)
    for idx in range(num_steps):
        prb = alma.prebuf(alma_inst)[0]
        if (idx % 10 == 0):
            print("Step: ", idx)
            print("prb size: ", len(prb))
            print("KB:")
            for fmla in alma.kbprint(alma_inst)[0].split('\n'):
                print(fmla)
                if ': distanceBetweenBoundedBy' in fmla:
                    dbb_instances.append(fmla)

            rth = alma.res_task_buf(alma_inst)
            print("Heap:")
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
            alma.prb_to_res_task(alma_inst)
        #alma.add(alma_inst, "distanceAt(a, {}, {}).".format(idx, idx))
        alma.astep(alma_inst)
    return dbb_instances

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run test with specified parameters.")
    parser.add_argument("use_network", type=str)
    parser.add_argument("explosion_steps", type=int, default=10)
    parser.add_argument("reasoning_steps", type=int, default=500)

    args = parser.parse_args()
    use_net = True if args.use_network == "True" else False
    assert(type(use_net) == type(True))
    print("Read network {}, expsteps {}   rsteps {}".format(use_net, args.explosion_steps, args.reasoning_steps))
    res = test(use_net, args.explosion_steps, args.reasoning_steps, 0)
    print("Final result is", res)
    print("Final number is", len(res))

            
    

