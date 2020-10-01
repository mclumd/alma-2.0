"""
Define a simple knowledge base that gives combinatorial explosion.
Train a neural network to control the explosion.
Collect data and report results.

"""

import alma
import random
import os
import rl_functions
import numpy as np
import itertools

#os.environ["LD_LIBRARY_PATH"] = "/home/justin/alma-2.0/"

alma_inst,res = alma.init(1,'/home/justin/alma-2.0/glut_control/test1_kb.pl', '0', 1, 1000, ['a', 'b', 'c', 'd', 'e', 'f'], [0.5]*6)
alma.add(alma_inst, "distanceAt(a, 100, 1).")
alma.add(alma_inst, "distanceAt(b, 200, 1).")
for step in range(10):
    alma.astep(alma_inst)
    alma.prb_to_res_task(alma_inst)
    print("KB:")
    for fmla in alma.kbprint(alma_inst)[0].split('\n'):
        print(fmla)

    rth = alma.res_task_buf(alma_inst)
    print("Heap:")
    if len(rth) > 0:
        for i, fmla in enumerate(rth[1].split('\n')[:-1]):
            pri = rth[0][i][-1]
            print("i={}:\t{}\tpri={}".format(i, fmla, pri))
            if i > 100:
                break
    print("-"*80)

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
        obs_fmla_a = "distanceAt(a, {}, {}).".format(random.randint(0, 10), i)
        obs_fmla_b = "distanceAt(b, {}, {}).".format(random.randint(0, 10), i)
        alma.add(alma_inst, obs_fmla_a)
        alma.add(alma_inst, obs_fmla_b)
        r = alma.prebuf(alma_inst)
        #print("r: ", r)
        alma.astep(alma_inst)
    return r

def train():
    exp = explosion()
    res_tasks = exp[0]
    res_lits = res_task_lits(exp[1])
    subjects = ['a', 'b']
    network = rl_functions.res_prefilter(subjects, [])
    res_task_input = [ x[:2] for x in res_tasks]
    network.train_batch(res_task_input, res_lits)
    network.model_save('test1')

def test(random_priors=False, num_steps=1000):
    global alma_inst,res
    exp = explosion(100)
    res_tasks = exp[0]
    res_lits = res_task_lits(exp[1])
    subjects = ['a', 'b']
    res_task_input = [ x[:2] for x in res_tasks]
    if not random_priors:
        network = rl_functions.res_prefilter(subjects, [])
        network.model_load('test1')
        priorities = 1 - network.get_priorities(res_task_input)
    else:
        priorities = np.random.uniform(size=len(res_task_input))

    alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
    alma.prb_to_res_task(alma_inst)

    print("prb: ", alma.prebuf(alma_inst))
    print("kb: ", alma.kbprint(alma_inst))
    for idx in range(num_steps):
        prb = alma.prebuf(alma_inst)[0]
        if (idx % 10 == 0):
            print("Step: ", idx)
            print("prb size: ", len(prb))
            print("KB:")
            for fmla in alma.kbprint(alma_inst)[0].split('\n'):
                print(fmla)

            rth = alma.res_task_buf(alma_inst)
            print("Heap:")
            for i, fmla in enumerate(rth[1].split('\n')):
                pri = rth[0][i][-1]
                print("i={}:\t{}\tpri={}".format(i, fmla, pri))
                if i > 100:
                    break
            print("-"*80)

        if len(prb) > 0:
            res_task_input = [x[:2] for x in prb]
            priorities = np.random.uniform(size=len(res_task_input)) if random_priors else  1 - network.get_priorities(res_task_input)
            alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
            alma.prb_to_res_task(alma_inst)
        alma.add(alma_inst, "distanceAt(a, {}, {}).".format(idx, idx))
        alma.astep(alma_inst)

def test_without_rth():
    global alma_inst,res
    exp = explosion(100)
    res_tasks = exp[0]
    res_lits = res_task_lits(exp[1])
    subjects = ['a', 'b']
    #network = rl_functions.res_prefilter(subjects, [])
    res_task_input = [ x[:2] for x in res_tasks]
    priorities = np.random.uniform(size=len(res_task_input))
    alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
    alma.prb_to_res_task(alma_inst)

    print("prb: ", alma.prebuf(alma_inst))
    print("kb: ", alma.kbprint(alma_inst))
    for idx in range(1000):
        prb = alma.prebuf(alma_inst)[0]
        if (idx % 10 == 0):
            print("Step: ", idx)
            print("prb size: ", len(prb))
            print("KB:")
            for fmla in alma.kbprint(alma_inst)[0].split('\n'):
                print(fmla)

            rth = alma.res_task_buf(alma_inst)
            print("Heap:")
            for i, fmla in enumerate(rth[1].split('\n')):
                pri = rth[0][i][-1]
                print("i={}:\t{}\tpri={}".format(i, fmla, pri))
                if i > 100:
                    break
            print("-"*80)

        if len(prb) > 0:
            res_task_input = [x[:2] for x in prb]
            priorities = np.random.uniform(size=len(res_task_input))
            alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
            alma.prb_to_res_task(alma_inst)
        #alma.add(alma_inst, "distanceAt(a, {}, {}).".format(idx, idx))
        #alma.add(alma_inst, "distanceAt(b, {}, {}).".format(idx, idx))
        alma.astep(alma_inst)



