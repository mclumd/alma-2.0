import alma

def prebuf_print(alma_inst):
    prb = alma.prebuf(alma_inst)[0]
    print("prb size: ", len(prb))
    for fmla in prb:
        print(fmla)

def pr_heap_print(alma_inst, alma_heap_print_size=100):                
    rth = alma.res_task_buf(alma_inst)
    print("Heap:")
    print("HEAP size: {} ".format(len(rth[1].split('\n')[:-1])))
    for i, fmla in enumerate(rth[1].split('\n')[:-1]):
        pri = rth[0][i][-1]
        print("i={}:\t{}\tpri={}".format(i, fmla, pri))
        if i >  alma_heap_print_size:
            break
        
def kb_print(alma_inst):
    kb = alma.kbprint(alma_inst)[0]
    print("kb: ")
    for s in kb.split('\n'):
        print(s)
