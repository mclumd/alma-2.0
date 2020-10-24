import pickle
import numpy as np
import matplotlib.pyplot as plt


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
    scaled_res_diff = scaled_res_true - scaled_res_false
    plt.imshow(scaled_res_diff)
    plt.show()

#analyze("test1_results.pkl")
