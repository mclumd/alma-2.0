import alma
import glut_control.alma_utils as alma_utils
import numpy as np
import time
import sys

kb = 'glut_control/qlearning3.pl'


for i in range(3):
    alma_inst,res = alma.init(1,kb, '0', 1, 1000, [], [])
    #for _ in range(30):
    for _ in range(30):
        prebuf = alma.prebuf(alma_inst)
        full_actions = prebuf[0]
        actions_no_priorities = [x[:2] for x in full_actions]
        priorities =  np.random.uniform(size=len(full_actions))
        alma.set_priors_prb(alma_inst, priorities.tolist())
        alma.single_prb_to_res_task(alma_inst, 1.0)   # Note the 1.0 is a threshold, not a priority
        for j in range(10):
            alma.astep(alma_inst)
            alma.single_prb_to_res_task(alma_inst, 1.0)   # Note the 1.0 is a threshold, not a priority
        alma_utils.prebuf_print(alma_inst, 10)
        alma_utils.pr_heap_print(alma_inst, 10)
        alma_utils.kb_print(alma_inst)

    alma.halt(alma_inst)
    del alma_inst
    del res
    del alma
    del sys.modules["alma"]
    print("Waiting for next round...")
    time.sleep(30)
    import alma
print("Done.")
