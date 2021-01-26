"""
New tests for DARPA.   
   1.  Repeat original tests but do less training on the neural network.  In particular, don't allow neural net to train on entire dataset.
   2.  Use Don's axioms.  
   3.  Use scenario from search task with lots of objects.
"""

import pickle
import numpy as np
import matplotlib.pyplot as plt
import time
import subprocess
import seaborn as sb
import math
import pandas as pd


""" 
Read in pickle file and show heatmaps.  Some notes:
 1.  For any particular value of esteps, there are at most 2*esteps formulas of the form distanceBeteweenBoundedBy() that could be derived.   This will be useful for calculating percentages.
"""
def analyze(pkl_filename):
    with open(pkl_filename, "rb") as run_save:
        results = pickle.load(run_save)
        
    max_esteps = results['max_esteps']
    estep_skip = results['estep_skip']
    max_rsteps = results['max_rsteps']
    rstep_skip = results['rstep_skip']
    D = results['dbb_results']
    R = results['num_dbb']

    num_esteps = max_esteps // estep_skip
    assert(num_esteps == R.shape[0])
    raw_res_false = R[:, :, 0]
    raw_res_true = R[:, :, 1]
    scaled_res_false = np.zeros( (R.shape[0], R.shape[1]))
    scaled_res_true = np.zeros( (R.shape[0], R.shape[1]))
    scaled_res_diff = np.zeros( (R.shape[0], R.shape[1]))
    for eidx in range(num_esteps):
        max_resolutions = 2*eidx*estep_skip
        scaled_res_false[eidx] = raw_res_false[eidx][:] / max_resolutions
        scaled_res_true[eidx] = raw_res_true[eidx][:] / max_resolutions
    scaled_res_false = np.nan_to_num(scaled_res_false)
    scaled_res_true = np.nan_to_num(scaled_res_true)
    scaled_res_diff = scaled_res_true - scaled_res_false
    np.set_printoptions(precision=2)
    print(scaled_res_false)
    print(scaled_res_true)
    print(scaled_res_diff)


    # Plot random priors
    plt.clf()
    fig, ax = plt.subplots(figsize=(15, 15))
    sb.heatmap(scaled_res_false, cmap='Blues', annot=True)
    ax.xaxis.tick_bottom()
    xticks_labels = [str(rstep) for rstep in range(0, max_rsteps, rstep_skip)]
    print(xticks_labels)
    plt.xticks(np.arange(len(xticks_labels)) + .5, labels=xticks_labels, rotation=90)# axis labels
    plt.xlabel('Reasoning steps')


    #ax.yaxis.tick_top()
    yticks_labels = [str(estep) for estep in range(0, max_esteps, estep_skip)]
    plt.yticks(np.arange(len(yticks_labels)) + .5, labels=yticks_labels)# axis labels
    plt.ylabel('Number of original statements')# title
    title = 'Proportion of Drawable Conclusions with Random Prioirties\n'.upper()
    plt.title(title, loc='left')
    plt.show()


    # Plot with ML
    plt.clf()
    fig, ax = plt.subplots(figsize=(15, 15))
    sb.heatmap(scaled_res_true, cmap='Blues', annot=True)
    ax.xaxis.tick_bottom()
    xticks_labels = [str(rstep) for rstep in range(0, max_rsteps, rstep_skip)]
    print(xticks_labels)
    plt.xticks(np.arange(len(xticks_labels)) + .5, labels=xticks_labels, rotation=90)# axis labels
    plt.xlabel('Reasoning steps')


    #ax.yaxis.tick_top()
    yticks_labels = [str(estep) for estep in range(0, max_esteps, estep_skip)]
    plt.yticks(np.arange(len(yticks_labels)) + .5, labels=yticks_labels)# axis labels
    plt.ylabel('Number of original statements')# title
    title = 'Proportion of Drawable Conclusions with Learned Priorities\n'.upper()
    plt.title(title, loc='left')
    plt.show()


    
    # Plot differences
    plt.clf()
    fig, ax = plt.subplots(figsize=(15, 15))
    sb.heatmap(scaled_res_diff, cmap='Reds', annot=True)
    ax.xaxis.tick_bottom()
    xticks_labels = [str(rstep) for rstep in range(0, max_rsteps, rstep_skip)]
    print(xticks_labels)
    plt.xticks(np.arange(len(xticks_labels)) + .5, labels=xticks_labels, rotation=90)# axis labels
    plt.xlabel('Reasoning steps')


    #ax.yaxis.tick_top()
    yticks_labels = [str(estep) for estep in range(0, max_esteps, estep_skip)]
    plt.yticks(np.arange(len(yticks_labels)) + .5, labels=yticks_labels)# axis labels
    plt.ylabel('Number of original statements')# title
    title = 'Difference in Proportion of Drawable Conclusions, Learned - Random\n'.upper()
    plt.title(title, loc='left')
    plt.show()


def data_collect(esteps_max=100, esteps_step=5, rsteps_max=20000, rsteps_step=500):
    total_dict = {}
    num_esteps = esteps_max // esteps_step
    num_rsteps = rsteps_max // rsteps_step
    results = np.zeros((num_esteps, num_rsteps, 4))
    for exp_steps in range(0,esteps_max, esteps_step):
        for reasoning_steps in range(0, rsteps_max, rsteps_step):
            for condition in [False, True]:
                if exp_steps == 0 or reasoning_steps == 0:
                    tr = []
                    tr_num = 0
                    tr_time = 0
                else:
                    print("Running with condition={}, exp_steps={}, reasoning_steps={}\n".format(condition, exp_steps, reasoning_steps))
                    tr_str, tr_num, tr_time = test_subprocess(condition, exp_steps, reasoning_steps)
                total_dict[exp_steps, reasoning_steps, condition] = tr
                results[ exp_steps // esteps_step, reasoning_steps // rsteps_step, 1 if condition else 0] = tr_num
                results[ exp_steps // esteps_step, reasoning_steps // rsteps_step, 3 if condition else 2] = tr_time
    return total_dict, results

def test_subprocess_old(condition, exp_steps, reasoning_steps):
    start = time.time()
    num_bits = int(math.ceil(math.log2(reasoning_steps)))
    model_name = "halloween{}-{}-{}".format(exp_steps, reasoning_steps, num_bits)
    if condition:
        cmd = subprocess.run("python ./test1.py {} {} {} 0 0 {} --retrain {} --num_rounds 1000".format(condition, exp_steps, reasoning_steps, num_bits, model_name), shell=True, stdout=subprocess.PIPE)
    else:
        cmd = subprocess.run("python ./test1.py {} {} {} 0 0 {}".format(condition, exp_steps, reasoning_steps, num_bits), shell=True, stdout=subprocess.PIPE)
    print("cmd=", cmd)
    #print("cmd_stdout=", cmd.stdout)
    stdout = cmd.stdout.decode('UTF-8')
    for outline in stdout.split('\n'):
        if str("Final result is ") in str(outline):
            tr_str = str(outline[16:])
        if str("Final number is ") in str(outline):
            tr_num = int(outline[16:])
    total_time = time.time() - start
    print("Finished at:", time.time())
    return tr_str, tr_num, total_time

#main()
#train(20, 500)
#print("results:", test(False, 50, 7000, alma_heap_print_size=10))
def test_models():
    max_esteps=50
    estep_skip = 5
    max_rsteps=10000
    rstep_skip= 500

    D, R = data_collect(max_esteps, estep_skip, max_rsteps, rstep_skip)

    res_dict = {
        'max_esteps': max_esteps,
        'estep_skip': estep_skip,
        'max_rsteps': max_rsteps,
        'rstep_skip': rstep_skip,
        'dbb_results': D,
        'num_dbb': R}

    with open("test1031_results.pkl", "wb") as run_save:
        pickle.dump(res_dict, run_save)

def train_models():
    max_esteps=50
    estep_skip = 5
    max_rsteps=100000
    rstep_skip= 500

    for esteps in range(estep_skip, max_esteps, estep_skip):
        num_bits = int(math.ceil(math.log2(esteps)))
        model_name = "halloween{}-{}".format(esteps, num_bits)
        subprocess.run("python ./test1.py True {} 10000  0 0 {} --retrain {}".format(esteps, num_bits, model_name), shell=True)

    
#train_models()
#analyze("test1_results.pkl")
#analyze("test2_results.pkl")


def test_subprocess(condition, prb_threshold):
    exp_steps = 50
    reasoning_steps = 3000
    num_bits = 6
    model_name = "january_preglut50-6"

    if condition:
        cmd = subprocess.run("python ./test1.py {} {} {} 0 0 {} {} --reload {}".format(condition, exp_steps, reasoning_steps, num_bits, prb_threshold, model_name), shell=True, stdout=subprocess.PIPE)
    else:
        cmd = subprocess.run("python ./test1.py {} {} {} 0 0 {} {} --reload {}".format(condition, exp_steps, reasoning_steps, num_bits, prb_threshold, model_name), shell=True, stdout=subprocess.PIPE)
    print("cmd=", cmd)

    stdout = cmd.stdout.decode('UTF-8')

    heap_sizes = np.zeros(3000) - 1
    dbb_vals = np.zeros(3000) - 1
    for outline in stdout.split('\n'):
        if str("Final result is ") in str(outline):
            tr_str = str(outline[16:])
        if str("Final number is ") in str(outline):
            tr_num = int(outline[16:])
        if str("HEAP size") in str(outline):
            i, s = outline.split(':')
            i = int(i.split(' ')[-1])
            s = int(s)
            print("Found {}; processed as s={}, i={}".format(outline, s, i))
            heap_sizes[i] = s
        if str("DBB") in str(outline):
            i, s = outline.split(':')
            i = int(i.split(' ')[-1])
            s = int(s)
            dbb_vals[i] = s

    return tr_str, tr_num, heap_sizes, dbb_vals



     

def collect_data():
    _, _, heap_sizes, dbb_vals = test_subprocess(False, 1.0)
    file1 = open('jpg_data_False_1.0.pkl', 'wb')
    pickle.dump( (heap_sizes, dbb_vals), file1)

    _, _, heap_sizes, dbb_vals = test_subprocess(True, 1.0)
    file2 = open('jpg_data_True_1.0.pkl', 'wb')
    pickle.dump( (heap_sizes, dbb_vals), file2)

    _, _, heap_sizes, dbb_vals = test_subprocess(False, 0.25)
    file3 = open('jpg_data_False_0.25.pkl', 'wb')
    pickle.dump( (heap_sizes, dbb_vals), file3)

    _, _, heap_sizes, dbb_vals = test_subprocess(True, 0.25)
    file4 = open('jpg_data_True_0.25.pkl', 'wb')
    pickle.dump( (heap_sizes, dbb_vals), file4)

def produce_graphs():
    df = {}
    for condition in [False, True]:
        for threshold in [1.0, 0.25]:
            plt.clf()
            f, ax = plt.subplots(1,1)
            fname = "jpg_data_{}_{}.pkl".format(condition, threshold)
            print("Using fname {}".format(fname))
            H, D = pickle.load(open(fname, "rb"))
            plt.xlabel("Reasoning steps")
            plt.ylabel("Heap Size")
            plt.title("Number of Potential Resolutions Over Time")
            ax.set_ylim(top=4250)
            #plt.bar(list(range(len(H))), H)
            #sb.barplot(list(range(len(H))), H, palette='Blues_d')
            ax.plot(list(range(len(H))), H, color='red')
            #sb.distplot(list(range(len(H))), H, color='red')
            ax.legend()
            plt.savefig("hw_img_h_{}_{}.png".format(condition, threshold), bbox_inches='tight')


            plt.clf()
            f, ax = plt.subplots(1,1)
            plt.xlabel("Reasoning Steps")
            plt.ylabel("Number of Conclusions Drawn")
            plt.title("Number of Conclusions Drawn Over Time")
            ax.set_ylim(top=100)
            ax.plot(D)
            ax.legend()
            plt.savefig("jpg_img_d_{}_{}.png".format(condition, threshold), bbox_inches='tight')


            plt.clf()
            f, ax = plt.subplots(1,1)
            x = list(range(len(H)))
            ax.set_ylim(top=150)
            ax.plot(x, H/30, label="Potential Resolutions (Div 30)" )
            ax.plot(x, D, label="Actualized Inferences")
            if condition:
                subtitle = " Using Learned Priorities"
            else:
                subtitle = " Using Random Priorities"
            subtitle = subtitle + " and Threshold {}".format(threshold)
                
            plt.title("Potential and Actualized Inferences over Time" + subtitle)
            ax.legend()
            plt.savefig("jpg_img_agg_{}_{}.png".format(condition, threshold), bbox_inches='tight')
            
collect_data()
produce_graphs()            

#  Idea:  No heat map.
#  1.  Using a 1.0 threshold, look at heap size over time; also measure number of conclusions drawn after n steps
#  1.  Using a 0.2 threshold, look at heap size over time; also measure number of conclusions drawn after n steps
