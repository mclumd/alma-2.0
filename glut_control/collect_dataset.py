"""
Collect datasets for offline processing.  For now tied strictly to test1_kb.pl
Parameters:
   num
   num_observations:    number of observations
   reaoning_steps:      number of reasoning steps
   outfile:             output filename
   training_percent:    percentage of potential inferences to use for training set.  

Outputs a pkl file with:
   training_set_ig:      training set as incidence graphs
   training_set_str:     strings of training set unification terms
   training_set_tree:    tree representation of training set potential inferences
   training_set_y:       1 for unifies, 0 for doesn't

And similarly for the testing set.
"""


import argparse
import numpy as np

import alma_utils
import resolution_prebuffer
import alma
from utils import explosion, res_task_lits
import sys
import pickle
from sklearn.utils import shuffle

from utils import kb_to_subjects_list
import tqdm

def get_dataset(network, training_percent):
    X = network.Xbuffer
    y = network.ybuffer
    saved_inputs = network.saved_inputs
    saved_prbs = network.saved_prbs
    
    subjects_dict = network.subjects_dict

    Xbuffer, ybuffer, infStr, inputs = shuffle(network.Xbuffer, network.ybuffer, network.saved_prbs, network.saved_inputs)
    X,Y, S, I = [], [], [], []
    pos_count, neg_count, total = [0]*3

    seen, rev_seen = set(), set()


    for i in range(len(Xbuffer)):
        x, y, s, i = [l.pop(0) for l in [Xbuffer, ybuffer, infStr, inputs]]
        strPair = tuple(s[:2])
        if (strPair in seen) or (strPair in rev_seen):
            print("Ignoring duplicate ", strPair)
        else:
            seen.add(strPair)
            rev_seen.add(  (strPair[1], strPair[0]) )
            X.append(x); Y.append(y); S.append(s); I.append(i)
            total += 1
            if y == 1:
                pos_count += 1
            else:
                neg_count += 1

    X, Y, S, I  = shuffle(X, Y, S, I)
    total = pos_count + neg_count
    train_total = int(total*training_percent)
    trainX, testX = X[:train_total], X[train_total:]
    trainY, testY = Y[:train_total], Y[train_total:]
    trainInfStr, testInfStr = S[:train_total], S[train_total:]
    trainInputs, testInputs = I[:train_total], I[train_total:]
    train_pos = sum(trainY)
    train_neg = len(trainY) - train_pos
    test_pos = sum(testY)
    test_neg = len(testY) - test_pos
    total_pos = train_pos + test_pos
    total_neg = train_neg + test_neg


    return {'training_set_ig':      trainX,
            'training_set_y':       trainY,
            'training_set_str':     trainInfStr,
            'training_set_tree':    trainInputs,
            'testing_set_ig':      testX,
            'testing_set_y':       testY,
            'testing_set_str':     testInfStr,
            'testing_set_tree':    testInputs,
            'train_pos': train_pos,
            'test_pos': test_pos,
            'train_neg': train_neg,
            'test_neg': test_neg,
            'total_pos': total_pos,
            'total_neg': total_neg,
            'total': total}


def collect(reasoning_steps, num_observations, num_trajectories, outfile, subject_list,
            kb=None, gnn_nodes=-1, gnn=True, text_kb=False, classical_steps=False, width=1,
            verbose=False, training_percent=0.8):
    trajectories = []
    for tnum in tqdm.trange(num_trajectories):
        network = resolution_prebuffer.res_prebuffer(subject_list, [], debug=True, use_tf=True,   # Torch not working at time of writing
                                                     use_gnn=gnn, gnn_nodes=gnn_nodes)
        alma_inst, _ = alma.init(1,kb, '0', 1, 1000, [], [])
        exp = explosion(num_observations, kb, alma_inst)
        res_tasks = exp[0]
        res_lits = res_task_lits(exp[2])
        res_task_input = [x[:2] for x in res_tasks]
        if text_kb:  # random walk
            priorities = np.random.uniform(size = len(res_task_input))
            kb_over_time = []
            kb_over_time.append(alma_utils.current_kb_text(alma_inst))
            total = 1
            alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
            alma.prb_to_res_task(alma_inst, 1.0)
        elif not classical_steps:
            network.save_batch(res_task_input, res_lits)
            priorities = 1 - network.get_priorities(res_task_input)
            alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
            alma.prb_to_res_task(alma_inst, 1.0)
        # Get the next action
        actions = []
        if not classical_steps:
            actions.append(alma_utils.next_action(alma_inst))
        for step in range(reasoning_steps):
            print("Tnum: ", tnum, "Step: ", step)
            if classical_steps:
                alma_utils.classical_step(alma_inst)
                new_kb = alma_utils.current_kb_text(alma_inst)
                kb_over_time.append(new_kb)
                if verbose:
                    print("KB: ", new_kb)
                res_task_buffer = alma.res_task_buf(alma_inst)
                actions.append(res_task_buffer[0])
            else:
                for _ in range(width):
                    alma.astep(alma_inst)
                prb = alma.prebuf(alma_inst)
                res_tasks = prb[0]
                if len(res_tasks) > 0:
                    res_lits = res_task_lits(prb[2])
                    res_task_input = [x[:2] for x in res_tasks]
                    if text_kb:
                        priorities = np.random.uniform(size = len(res_task_input))
                        kb_over_time.append(alma_utils.current_kb_text(alma_inst))
                        total += 1
                    else:
                        network.save_batch(res_task_input, res_lits)
                        print("rti len is", len(res_task_input))
                        print("At reasoning step {}, network has {} samples, {} of which are positive".format(step, len(network.ybuffer), network.ypos_count))
                        priorities = 1 - network.get_priorities(res_task_input)
                    alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
                    alma.prb_to_res_task(alma_inst, 1.0)
                    actions.append(alma_utils.next_action(alma_inst))
        if classical_steps:
            text_actions = [ [ (alma_utils.alma_tree_to_str(x[0]), alma_utils.alma_tree_to_str(x[1])) for x in y] for y in actions ]
        else:
            text_actions = [  (alma_utils.alma_tree_to_str(x[0]), alma_utils.alma_tree_to_str(x[1])) for x in actions]
        if text_kb:
            trajectory = {
                'total': total,
                'kb': kb_over_time,
                'actions': text_actions
                }
        else:
            trajectory = get_dataset(network, training_proportion)
            print("Final result has {} positive, {} negative, {} total samples.   Writing to {}".format(trajectory['total_pos'],trajectory['total_neg'],trajectory['total'], outfile))
        trajectories.append(trajectory)
        alma.halt(alma_inst)
    
    with open(outfile, "wb") as pkl_file:
        pickle.dump(trajectories, pkl_file)

    
def main():
    print("Command line: ", ''.join(sys.argv))
    
    parser = argparse.ArgumentParser(description="Collect dataset for offline learning.")
    parser.add_argument("num_observations", type=int, default=10)
    parser.add_argument("reasoning_steps", type=int, default=500)
    parser.add_argument("num_trajectories", type=int, default=5)
    parser.add_argument("outfile", type=str)
    parser.add_argument("--text_kb", action='store_true')
    parser.add_argument("--training_percent", type=float, default=0.8)
    parser.add_argument("--gnn_nodes", type=int, default=-1)
    parser.add_argument("--kb", action='store', required=False, default="test1_kb.pl")
    parser.add_argument("--dgl_gnn", action='store_true')
    parser.add_argument("--classical_steps", action='store_true')
    parser.add_argument("--width", type=int, default=1)
    parser.add_argument("--verbose", action='store_true')

    args = parser.parse_args()
    subject_list = kb_to_subjects_list(args.kb, True)
    if args.classical_steps:
        args.text_kb = True
    collect(args.reasoning_steps, args.num_observations, args.num_trajectories,
            args.outfile, subject_list,
            kb = args.kb, gnn_nodes = args.gnn_nodes, gnn=args.gnn_nodes != -1,
            training_percent=args.training_percent,
            text_kb = args.text_kb, classical_steps=args.classical_steps, width=args.width, verbose=args.verbose)







if __name__ == "__main__":
    main()
