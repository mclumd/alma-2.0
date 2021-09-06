import alma
import math

def prebuf_print(alma_inst, print_size=math.inf):
    prb = alma.prebuf(alma_inst)[0]
    print("prb size: ", len(prb))
    i=0
    for fmla in prb:
        print(fmla)
        i += 1
        if i > print_size:
            return

def pr_heap_print(alma_inst, alma_heap_print_size=100):                
    rth = alma.res_task_buf(alma_inst)
    print("Heap:")
    print("HEAP size: {} ".format(len(rth[1].split('\n')[:-1])))
    for i, fmla in enumerate(rth[1].split('\n')[:-1]):
        pri = rth[0][i][-1]
        print("i={}:\t{}\tpri={}".format(i, fmla, pri))
        if i >  alma_heap_print_size:
            break

def heap_size(alma_inst):
    return alma.res_task_buf_size(alma_inst)

def heap_size_old(alma_inst):
    rth = alma.res_task_buf(alma_inst)
    hs = len(rth[1].split('\n')[:-1])
    del rth
    return hs

def kb_print(alma_inst):
    kb = alma.kbprint(alma_inst)[0]
    print("kb: ")
    for s in kb.split('\n'):
        print(s)


def alma_tree_to_str(tree):
    if len(tree) == 0:
        return ""
    elif tree[0] == 'if':
        return alma_tree_to_str(tree[1]) + " --> " + alma_tree_to_str(tree[2])
    elif tree[0] == 'func':
        return tree[1] + '(' + ''.join([alma_tree_to_str(term) for term in tree[2]   ]) + ')'
    elif tree[0] == 'var':
        return tree[1]
    elif tree[0] == 'a':
        return alma_tree_to_str(tree[1][0]) + ' and ' + alma_tree_to_str(tree[1][1])

def alma_collection_to_strings(collection):
    return [ alma_tree_to_str(tree) for tree in collection]

def actions_to_strings(acts):
    return [ [alma_tree_to_str(t0), alma_tree_to_str(t1)] for [t0, t1] in acts  ]
