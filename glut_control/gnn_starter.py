"""
Start experimenting with graph representations.  
"""
_CPU_ONLY=True

import random
import os
import sys
import torch


if _CPU_ONLY:
    os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
else:
    print("Does torch find cuda: ", torch.cuda.is_available())

import rl_utils, resolution_prebuffer
import numpy as np
import itertools
import pickle

from importlib import reload
import alma
from alma_utils import *
import argparse
import random

import dgl_dataset
import dgl_network
from dgl_heuristic import dgl_heuristic

#from memory_profiler import profile

#os.environ["LD_LIBRARY_PATH"] = "/home/justin/alma-2.0/"
test_params = {
    'explosion_size': 1000,
    'alma_heap_print_size': 100
}
#alma_inst,res = alma.init(1,'test1_kb.pl', '0', 1, 1000, [], [])
# alma_inst,res = alma.init(1,'test2.pl', '0', 1, 1000, [], [])
# alma_inst,res = alma.init(1,'test3.pl', '0', 1, 1000, [], [])
# alma_inst,res = alma.init(1,'test4.pl', '0', 1, 1000, [], [])
# alma_inst,res = alma.init(1,'test5.pl', '0', 1, 1000, [], [])
# alma_inst,res = alma.init(1,'qlearning1.pl', '0', 1, 1000, [], [])
# alma_inst,res = alma.init(1,'qlearning2.pl', '0', 1, 1000, [], [])
# alma_inst,res = alma.init(1,'qlearning3.pl', '0', 1, 1000, [], [])
# alma_inst,res = alma.init(1,'ps_test_search.pl', '0', 1, 1000, [], [])

import gc
#gc.disable()
from tensorflow.keras import backend as K
def res_task_lits(lit_str):
    L = lit_str.split('\n')[:-1]
    return [ x.split('\t') for x in L]

def explosion(size, kb, alma_inst):
    """
    The one axiom we work with:
       if(and(distanceAt(Item1, D1, T), distanceAt(Item2, D2, T)), distanceBetweenBoundedBy(D1, Item1, Item2, T)).
    """
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
        elif "qlearning1.pl" in kb:
            # obs_fmla_a = "f({}).".format(i)
            obs_fmla_b = "g({}).".format(i)
            # alma.add(alma_inst, obs_fmla_a)
            alma.add(alma_inst, obs_fmla_b)
        # elif "ps_test_search.pl" in kb:
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
def train_loop(network, num_steps, explosion_steps, kb, train_interval, dgl_data, exhaustive):
    alma_inst,res = alma.init(1,kb, '0', 1, 1000, [], [])
    prb = explosion(explosion_steps, kb, alma_inst)
    print('prb_size', len(prb[0]))
    for idx in range(num_steps):
        res_tasks = prb[0]
        if len(res_tasks) > 0:
            res_lits = res_task_lits(prb[2])
            res_task_input = [x[:2] for x in res_tasks]
            network.save_batch(res_task_input, res_lits)
            # if not new_dgl_dataset:
            #     g_data = dgl_dataset.AlmaDataset(network.Xbuffer, network.ybuffer)
            #     dgl_data.append(g_data)

            print("rti len is", len(res_task_input))
            if len(res_task_input) > 1000:
                print("that's a lot!!")
            priorities = 1 - network.get_priorities(res_task_input)
            alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
            alma.prb_to_res_task(alma_inst, 1.0)
            del priorities

        if idx > 0 and idx % train_interval == 0:
            print("At reasoning step {}, network has {} samples, {} of which are positive".format(idx, len(network.ybuffer), network.ypos_count))
            if (network.ypos_count > (network.batch_size  / 2)) and (network.yneg_count > (network.batch_size  / 2)):

                # dgl_test(network.Xbuffer, network.ybuffer)
                # build up a list of DGLDatasets
                # g_data = dgl_dataset.AlmaDataset(network.Xbuffer, network.ybuffer)
                # dgl_data.append(g_data)

                H = network.train_buffered_batch()
                if H is not None:
                    acc, loss = H.history['accuracy'][0], H.history['loss'][0]
                    #print("accuracy: {} \t loss: {}\ttacc: {}".format(acc, loss, H.history['tacc'][0]))
                    if acc > 0.8:
                        print("acc > 0.8")
                    if acc > 0.999 and loss < 1e-5:
                        print("Good model; ending early")
                        network.model_save(model_name, numeric_bits)
                        return

                if exhaustive:   # If exhaustive, try to use up the replay buffer
                    while  (network.ypos_count > (network.batch_size  / 2)) and (network.yneg_count > (network.batch_size  / 2)):
                        print("another batch...", end=" ")
                        H = network.train_buffered_batch()
                        acc, loss = H.history['accuracy'][0], H.history['loss'][0]

            #else:
            #    print("Continuing...")
            #    continue
            #hs = heap_size(alma_inst)
            #if hs < 100:
            #    print("Refreshing the heap.")
            #    prb = explosion(explosion_steps, kb, alma_inst)
            #    continue

        alma.astep(alma_inst)
        prb = alma.prebuf(alma_inst)
    network.clean(True)
    del prb
    alma.halt(alma_inst)
    gc.collect()
    K.clear_session()

#@profile
def train(explosion_steps=50, num_steps=500, numeric_bits=3, model_name="test1", use_gnn = False, num_trainings=1000, train_interval=500, kb='test1_kb.pl', net_clear=1000, gnn_nodes = 50, exhaustive=False, dgl_gnn=False):
    global new_dgl_dataset
    #hp = hpy()
    #hpy_before = hp.heap()

    # ***************** #
    # BUILD DGL DATASET #
    # ***************** #

    dgl_data = []
    #new_dgl_dataset = False
    new_dgl_dataset = True
    if new_dgl_dataset:
        if "test1_kb.pl" in kb:
            subjects = ['a', 'b', 'distanceAt', 'distanceBetweenBoundedBy']
        elif "january_preglut.pl" in kb:
            subjects = ["a{}".format(x) for x in range(explosion_steps)]
        elif "qlearning1.pl" in kb:
            subjects = ['f', 'g', 'a']
        # elif "ps_test_search.pl" in kb:

        if "test1_kb.pl" in kb and not use_gnn:
            for place in range(3):
                for cat_subj in ['a', 'b']:
                    subjects.append("{}/{}".format(cat_subj, place))
                for num_subj in range(2 ** numeric_bits):
                    subjects.append("{}/{}".format(num_subj, place))

        network = resolution_prebuffer.res_prebuffer(subjects, [], use_gnn=use_gnn, gnn_nodes=gnn_nodes)

        alma_inst, res = alma.init(1, kb, '0', 1, 1000, [], [])
        exp = explosion(explosion_steps, kb, alma_inst)       # making a big but less redundant dataset
        res_tasks = exp[0]
        res_lits = res_task_lits(exp[2])
        res_task_input = [x[:2] for x in res_tasks]
        prb = alma.prebuf(alma_inst)
        res_tasks = prb[0]
        res_lits = res_task_lits(prb[2])
        res_task_input = [x[:2] for x in res_tasks]
        network.save_batch(res_task_input, res_lits)
        print("Network has {} samples, {} of which are positive".format(len(network.ybuffer), network.ypos_count))

        temp = network.ybuffer
        g_data = dgl_dataset.AlmaDataset(network.Xbuffer, network.ybuffer)
        dgl_data.append(g_data)

    # ****************** #
    # END DGL DATA BLOCK #
    # ****************** #


    if "test1_kb.pl" in kb:
        subjects = ['a', 'b', 'distanceAt', 'distanceBetweenBoundedBy']
    elif "january_preglut.pl" in kb:
        subjects = ["a{}".format(x) for x in range(explosion_steps)]
    elif "qlearning1.pl" in kb:
        subjects = ['f', 'g', 'a']
    # elif "ps_test_search.pl" in kb:

    if "test1_kb.pl" in kb and not use_gnn:
        for place in range(3):
            for cat_subj in ['a', 'b']:
                subjects.append("{}/{}".format(cat_subj, place))
            for num_subj in range(2**numeric_bits):
                subjects.append("{}/{}".format(num_subj, place))
    if dgl_gnn:
        network = dgl_heuristic(g_data.dim_nfeats, subjects, [], use_gnn=True, gnn_nodes = gnn_nodes)
    else:
        network = resolution_prebuffer.res_prebuffer(subjects, [], use_gnn=use_gnn, gnn_nodes = gnn_nodes)
    for b in range(num_trainings):
        print("Starting round ", b)
        if b > 0 and (b% 25 == 0):
            print("Saving checkpoint {}".format(b))
            network.model_save(model_name+"ckpt{}".format(b), numeric_bits)

        print("Network has {} samples, {} of which are positive".format(len(network.ybuffer), network.ypos_count))

        train_loop(network, num_steps, explosion_steps, kb, train_interval, dgl_data, exhaustive)


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

    # slightly hacky, return dgl_data here for the dgl train/test loops
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
    alma_inst,res = alma.init(1,kb, '0', 1, 1000, [], [])
    dbb_instances = []
    exp = explosion(exp_size, kb, alma_inst)
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
    elif "qlearning1.pl" in kb:
        subjects = ['f', 'g', 'a']
    # elif "ps_test_search.pl" in kb:

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
    print("Command line: ", ''.join(sys.argv))
    
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
    parser.add_argument("--exhaustive", action='store_true')
    parser.add_argument("--dgl_gnn", action='store_true')
    
    network = None
    args = parser.parse_args()
    print("Run with args:", args)
    # use_net = True if args.use_network == "True" else False
    use_net = args.use_network
    assert(type(use_net) == type(True))

    if args.cpu_only:
        # TODO:  This may need to be done before any tensorflow imports
        os.environ["CUDA_VISIBLE_DEVICES"] = "-1"

    if args.dgl_gnn:
        assert(args.gnn)

    if args.train != "NONE":
        model_name = args.train
        assert(args.reload == "NONE")
        print("Using network: {} with expsteps {}   rsteps {}    model_name {}".format(use_net, args.explosion_steps, args.reasoning_steps, model_name))
        print('Training; model name is ', args.train)
        network = train(args.explosion_steps, args.reasoning_steps, args.numeric_bits, model_name, args.gnn, args.num_trainings, args.train_interval, args.kb, gnn_nodes=args.gnn_nodes, exhaustive=args.exhaustive, dgl_gnn=args.dgl_gnn)
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

    return

    # ************** #
    # GNN TRAIN/TEST #
    # ************** #
    # print("-"*80)
    # print("Now training GCN:")
    # print("-" * 80)

    # gnn = gnn_train(dgl_data)
    # gnn = dgl_network.load_gnn_model("best_gcn_epoch80")

    print("-"*80)
    print("Now testing GCN:")
    print("-"*80)
    res = gnn_test(gnn, use_net, args.explosion_steps, args.testing_reasoning_steps, args.heap_print_size,
               args.prb_print_size, args.numeric_bits,
               heap_print_freq=1, prb_threshold=args.prb_threshold, use_gnn=args.gnn, kb=args.kb,
               gnn_nodes=args.gnn_nodes, initial_test=False)
    print("Final result is", res)
    print("Final number is", len(res))


import torch
import torch.nn as nn
import torch.nn.functional as F
import dgl.data
from dgl.dataloading import GraphDataLoader
from torch.utils.data.sampler import SubsetRandomSampler, SequentialSampler


# A lot of this is boilerplate from:
# https://docs.dgl.ai/tutorials/blitz/5_graph_classification.html#sphx-glr-tutorials-blitz-5-graph-classification-py

def gnn_train(data_list):
    dataset = dgl_dataset.BigAlmaDataset(data_list)

    num_examples = len(dataset)
    num_train = int(num_examples * 0.8)

    train_sampler = SequentialSampler(torch.arange(num_train))
    test_sampler = SequentialSampler(torch.arange(num_train, num_examples))

    train_dataloader = GraphDataLoader(
        dataset, sampler=train_sampler, batch_size=32, drop_last=False)
    test_dataloader = GraphDataLoader(
        dataset, sampler=test_sampler, batch_size=100000, drop_last=False)

    model = dgl_network.GCN(dataset.dim_nfeats, 512, dataset.gclasses)
    # model = dgl_network.GatedGCN(dataset.dim_nfeats, 16, dataset.gclasses)
    # model = dgl_network.load_gnn_model("best_gcn_epoch5")
    optimizer = torch.optim.Adam(model.parameters(), lr=0.01)
    model.train()

    for epoch in range(100000):
        minloss = math.inf
        print("="*80)
        print("GCN epoch ", epoch, ":")
        i = 0
        for batched_graph, labels in train_dataloader:
            # print("Batched Graph ", i)
            i += 1
            # if i > 250:
            #     break
            pred = model(batched_graph, batched_graph.ndata['feat'].float())
            tensor = torch.tensor((), dtype=torch.float32)
            mse_labels = tensor.new_zeros((len(labels), 2))
            for j in range(batched_graph.batch_size):
                mse_labels[j][1] = labels[j]
                if labels[j] == 1:
                    mse_labels[j][0] = 0
                else:
                    mse_labels[j][0] = 1
            loss = F.mse_loss(pred, mse_labels)
            # loss = F.cross_entropy(pred, labels)
            optimizer.zero_grad()
            # tloss = loss
            loss.backward()
            optimizer.step()

            num_correct = 0
            num_tests = 0
            for batched_graph_test, labels_test in test_dataloader:
                pred = model(batched_graph_test, batched_graph_test.ndata['feat'].float())
                tensor = torch.tensor((), dtype=torch.float32)
                mse_labels_test = tensor.new_zeros((len(labels_test), 2))
                for j in range(batched_graph_test.batch_size):
                    mse_labels_test[j][1] = labels_test[j]
                    if labels_test[j] == 1:
                        mse_labels_test[j][0] = 0
                    else:
                        mse_labels_test[j][0] = 1
                tloss = F.mse_loss(pred, mse_labels_test)
                # tloss = F.cross_entropy(pred, labels_test)
                t1, t2 = torch.max(pred, 1)
                num_correct += (pred.argmax(1) == labels_test).sum().item()
                num_tests += len(labels_test)

                # pred[x] = [confidence class == 0, confidence class == 1] for sample x
                # t2[x] == binary prediction for sample x, t1[x] == magnitude of confidence value for prediction made in t2 on sample x

                show_errors = False
                # show_errors = True
                if show_errors and i/len(train_dataloader) > .1 and num_tests < 10:
                    if (pred.argmax(1) == labels).sum().item() != 5:
                        for a in range(batched_graph_test.batch_size):
                            if pred.argmax(1)[a] != labels[a]:
                                print("*" * 80)
                                print("INCORRECT PREDICTION " * 4)
                                print("Predicted:", pred.argmax(1)[a])
                                print("Actual:", labels[a])
                                graph_list = dgl.unbatch(batched_graph_test)
                                print("-" * 80)
                                torch.set_printoptions(threshold=100_000)  # Want to see it all
                                print("src/dst lists: ", graph_list[a].adj(True, 'cpu', None, graph_list[a].etypes[0]))
                                print("features: ", graph_list[a].ndata['feat'])
                                print("-" * 80)
                                print("*" * 80)

            print('GCN accuracy at', "{:.2f}".format(i/len(train_dataloader)*100), "% of training", ':', num_correct / num_tests)
            print('Loss:', tloss)

            if tloss < minloss and num_correct / num_tests > .9:
                minloss = tloss
                print("saving model")
                dgl_network.save_gnn_model(model, "best_gcn_epoch" + str(epoch))

            if num_correct / num_tests > 0.97 and tloss < 0.001 and epoch > 5000000:
                print("good GCN, returning early")
                dgl_network.save_gnn_model(model, "returned_gcn")
                # model.eval()
                return model



        print("=" * 80)

    # num_correct = 0
    # num_tests = 0
    # for batched_graph, labels in test_dataloader:
    #     pred = model(batched_graph, batched_graph.ndata['feat'].float())
    #     num_correct += (pred.argmax(1) == labels).sum().item()
    #     num_tests += len(labels)

    # print('GCN accuracy:', num_correct / num_tests)

    dgl_network.save_gnn_model(model, "returned_gcn")
    # model.eval()
    return model


def gnn_test(network, network_priors, exp_size=10, num_steps=500, alma_heap_print_size=100, prb_print_size=30, numeric_bits=10, heap_print_freq=10, prb_threshold=1, use_gnn = False, kb='/home/justin/alma-2.0/glut_control/test1_kb.pl', gnn_nodes=2000, initial_test=False):
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
    elif "qlearning1.pl" in kb:
        subjects = ['f', 'g', 'a']
    # elif "ps_test_search.pl" in kb:


    # network.eval()
    res_task_input = [ x[:2] for x in res_tasks]
    if network_priors:
        temp_network = resolution_prebuffer.res_prebuffer(subjects, [], use_gnn=use_gnn, gnn_nodes=gnn_nodes)

        prb = alma.prebuf(alma_inst)
        res_tasks = prb[0]
        res_lits = res_task_lits(prb[2])
        res_task_input = [x[:2] for x in res_tasks]
        temp_network.save_batch(res_task_input, res_lits)
        X = temp_network.Xbuffer
        Y = temp_network.ybuffer
        dataset = dgl_dataset.AlmaDataset(X, Y)

        # X = temp_network.vectorize(res_task_input)
        # Y = np.zeros(len(X)) # filler data, labels don't matter
        # dataset = dgl_dataset.AlmaDataset(X, Y)

        test_sampler = SubsetRandomSampler(torch.arange(0, len(dataset)))
        test_dataloader = GraphDataLoader(
            dataset, sampler=test_sampler, batch_size=len(dataset), drop_last=False)

        for batched_graph, labels in test_dataloader:
            pred = network(batched_graph, batched_graph.ndata['feat'].float())
            t1, t2 = torch.max(pred, 1)
            #s = torch.softmax(pred, 1)
            priorities = (1-pred)[:,1].detach().numpy()
            continue
            # t2[x] == binary prediction for sample x, t1[x] == magnitude of confidence value for prediction made in t2 on sample x
            priorities = np.zeros(len(X))
            for i in range(len(priorities)):
                # priorities[i] = pred[i][1]                      # activation val
                # priorities[i] = 1 / (1 + np.exp(priorities[i]))  # sigmoid for cross entropy
                p0 = float(pred[i][0])
                p1 = float(pred[i][1])
                priorities[i] = 1 - (np.exp(p1)/(np.exp(p1) + np.exp(p0)))
    else:
        priorities = np.random.uniform(size=len(res_task_input))

    alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
    alma.prb_to_res_task(alma_inst, prb_threshold)

    #print("prb: ", alma.prebuf(alma_inst))
    kb = alma.kbprint(alma_inst)[0]
    print("kb: ")
    for s in kb.split('\n'):
        print(s)

    alma.astep(alma_inst)
    for idx in range(num_steps):
        prb = alma.prebuf(alma_inst)
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

        if len(prb[0]) > 0:
            res_task_input = [x[:2] for x in prb]

            if network_priors:
                temp_network = resolution_prebuffer.res_prebuffer(subjects, [], use_gnn=use_gnn, gnn_nodes=gnn_nodes)

                #prb = alma.prebuf(alma_inst)
                res_tasks = prb[0]
                res_lits = res_task_lits(prb[2])
                res_task_input = [x[:2] for x in res_tasks]
                temp_network.save_batch(res_task_input, res_lits)
                X = temp_network.Xbuffer
                Y = temp_network.ybuffer
                dataset = dgl_dataset.AlmaDataset(X, Y)

                # X = temp_network.vectorize(res_task_input)
                # Y = np.zeros(len(X))  # filler data, labels don't matter
                # dataset = dgl_dataset.AlmaDataset(X, Y)

                test_sampler = SubsetRandomSampler(torch.arange(0, len(dataset)))
                test_dataloader = GraphDataLoader(
                    dataset, sampler=test_sampler, batch_size=len(dataset), drop_last=False)
                # test_dataloader = GraphDataLoader(
                #     dataset, sampler=test_sampler, batch_size=1, drop_last=False)

                for batched_graph, labels in test_dataloader:
                    pred = network(batched_graph, batched_graph.ndata['feat'].float())
                    t1, t2 = torch.max(pred, 1)
                    # t2[x] == binary prediction for sample x, t1[x] == magnitude of confidence value for prediction made in t2 on sample x
                    priorities = np.zeros(len(X))
                    for i in range(len(priorities)):
                        priorities[i] = pred[i][1]  # activation val
                        # priorities[i] = 1 / (1 + np.exp(priorities[i]))  # sigmoid for cross entropy
                        p0 = float(pred[i][0])
                        p1 = float(pred[i][1])
                        priorities[i] = 1 - (np.exp(p1) / (np.exp(p1) + np.exp(p0)))

            else:
                priorities = np.random.uniform(size=len(res_task_input))

            alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
            alma.prb_to_res_task(alma_inst, prb_threshold)
        #alma.add(alma_inst, "distanceAt(a, {}, {}).".format(idx, idx))
        alma.astep(alma_inst)
    return dbb_instances





#main()
if __name__ == "__main__":
    #gnn = dgl_network.load_gnn_model("best_gcn_epoch2")
    #gnn_test(gnn, True, 20, 50, heap_print_freq=1, alma_heap_print_size=1000, use_gnn=True, gnn_nodes=50, initial_test=False)
    main()

