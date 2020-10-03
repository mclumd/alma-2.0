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
import pickle

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
        obs_fmla_a = "distanceAt(a, {}, {}).".format(random.randint(0, 10), i)
        obs_fmla_b = "distanceAt(b, {}, {}).".format(random.randint(0, 10), i)
        alma.add(alma_inst, obs_fmla_a)
        alma.add(alma_inst, obs_fmla_b)
        r = alma.prebuf(alma_inst)
        alma.astep(alma_inst)
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

def test(network_priors=False, exp_size=10, num_steps=500, alma_heap_print_size=100):
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

def data_collect(esteps_max=100, esteps_step=5, rsteps_max=20000, rsteps_step=500):
    total_dict = {}
    num_esteps = esteps_max // esteps_step
    num_rsteps = rsteps_max // rsteps_step
    results = np.zeros((num_esteps, num_rsteps, 2))
    for exp_steps in range(0,esteps_max, esteps_step):
        for reasoning_steps in range(0, rsteps_max, rsteps_step):
            for condition in [False, True]:
                if exp_steps == 0 or reasoning_steps == 0:
                    tr = []
                else:
                    print("Running with condition={}, exp_steps={}, reasoning_steps={}\n".format(condition, exp_steps, reasoning_steps))
                    tr = test(condition, exp_steps, reasoning_steps, alma_heap_print_size=10)
                total_dict[exp_steps, reasoning_steps, condition] = tr
                results[ exp_steps // esteps_step, reasoning_steps // rsteps_step, 1 if condition else 0] = len(tr)
    return total_dict, results

#main()
#train(20, 500)
#print("results:", test(False, 50, 7000, alma_heap_print_size=10))
max_esteps=100
estep_skip = 5
max_rsteps=10000
rstep_skip= 250
D, R = data_collect(max_esteps, estep_skip, max_rsteps, rstep_skip)

res_dict = {
    'max_esteps': max_esteps,
    'estep_skip': estep_skip,
    'max_rsteps': max_rsteps,
    'rstep_skip': rstep_skip,
    'dbb_results': D,
    'num_dbb': R}

with open("test1_results.pkl", "wb") as run_save:
    pickle.dump(res_dict, run_save)


