
#(setq python-shell-interpreter "/home/justin/software/Python-3.8.10-valgrind/bin/python3"
#      python-shell-interpreter-args "-i -X showrefcount -X showalloccount -X tracemalloc -X dev ")



import glut_control.alma_utils as alma_utils
#import numpy as np
import time
import sys
import gc
import ctypes
import tracemalloc


kb = 'glut_control/qlearning3.pl'
#gc.set_debug(gc.DEBUG_LEAK)
#gc.disable()
class PyObject(ctypes.Structure):
    _fields_ = [("refcnt", ctypes.c_long)]

def single_run(num_moves=30, steps_per_move=10, run_num=None):
    import alma
    alma_inst,res = alma.init(1,kb, '0', 1, 1000, [], [])
    #for _ in range(30):
    ref_list = []
    for move in range(num_moves):
        prebuf = alma.prebuf(alma_inst)
        paddr = id(prebuf)
        paddr0 = id(prebuf[0])
        #print("Prebuf references (start):  ",PyObject.from_address(paddr).refcnt)
        #print("Prebuf[0] references (start):  ",PyObject.from_address(paddr0).refcnt)

        #input("Press any key")
        full_actions = prebuf[0]
        actions_no_priorities = [x[:2] for x in full_actions]
        #priorities =  np.random.uniform(size=len(full_actions)).tolist()
        priorities = [0.5 for a in range(len(full_actions))]
        alma.set_priors_prb(alma_inst, priorities)
        alma.single_prb_to_res_task(alma_inst, 1.0)   # Note the 1.0 is a threshold, not a priority
        for j in range(steps_per_move):
            run_str = "" if run_num is None else str(run_num) 
            print("move {}     step {} \t {}".format(move, j, run_str))
            alma.astep(alma_inst)
            alma.single_prb_to_res_task(alma_inst, 1.0)   # Note the 1.0 is a threshold, not a priority
        alma_utils.prebuf_print(alma_inst, 10)
        alma_utils.pr_heap_print(alma_inst, 10)
        alma_utils.kb_print(alma_inst)
        # paddr = id(prebuf)
        # paddr0 = id(prebuf[0])
        # aaddr = id(a)
        #print("Prebuf references (end):  ",PyObject.from_address(paddr).refcnt)
         #print("Prebuf[0] references (end):  ",PyObject.from_address(paddr0).refcnt)
        #print("a references (end):  ",PyObject.from_address(aaddr).refcnt)
        if len(prebuf[0]) > 0 and False:
            element1 = prebuf[0][0]
            eaddr = id(element1)
            print("element1 references:  ",PyObject.from_address(eaddr).refcnt)
            print("element1", element1)
            for n in element1:
                naddr = id(n)
                print("{} (addr {}) references: {}".format(n, naddr, PyObject.from_address(naddr).refcnt))
                ref_list.append(naddr)
        print("")
        #input("Press any key")
    #del full_actions
    #del prebuf
    #gc.collect()
    alma.halt(alma_inst)
    # del alma_inst
    # del res
    #del alma
    #del sys.modules["alma"]
    print("Waiting for next round...")
    #return paddr, paddr0, aaddr, eaddr, ref_list


def test():
    for i in range(3):
        #paddr, paddr0, aaddr, eaddr, reflist = single_run(30, 10, i)
        single_run(30, 10, i)
        #print("Returned: ", paddr, paddr0, aaddr, eaddr, reflist)
        #print("Prebuf references (tot, 0, 1, 2)",PyObject.from_address(paddr).refcnt)
        #print("Prebuf[0] references:  ",PyObject.from_address(paddr0).refcnt)
        #print("a references (end):  ",PyObject.from_address(aaddr).refcnt)
        #print("element1 references:  ",PyObject.from_address(eaddr).refcnt)
        #for addr in reflist:
        #    print("id {} references: {}".format(addr, PyObject.from_address(addr).refcnt))
        input("Press ENTER")

    print("Done.")
    
if __name__ == "__main__":
    test()


    
