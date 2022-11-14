import alma
import math
import numpy as np


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

def kb_str(alma_inst):
    return alma.kbprint(alma_inst)[0]


def next_action(alma_inst):        
    res_task_buffer = alma.res_task_buf(alma_inst)
    next_action = res_task_buffer[0][0]
    return next_action

def full_kb_text(alma_inst):
    L = alma.kb_to_pyobject(alma_inst, 1)
    return [alma_tree_to_str(x) for x in L]

def current_kb_text(alma_inst, full_kb=False):
    full = 1 if full_kb else 0
    L = alma.kb_to_pyobject(alma_inst, full)
    return [alma_tree_to_str(x) for x in L]

def alma_tree_to_str(tree):
    if len(tree) == 0:
        return ""
    elif tree[0] == 'if':
        return alma_tree_to_str(tree[1]) + " --> " + alma_tree_to_str(tree[2])
    elif tree[0] == 'func':
        if len(tree[2]) > 0:
            return tree[1] + '(' + ''.join([alma_tree_to_str(term) + ', ' for term in tree[2]   ])[:-2] + ')'
        else:
            return tree[1]
    elif tree[0] == 'var':
        return tree[1]
    elif tree[0] == 'and':
        result = alma_tree_to_str(tree[1][0])
        for clause in tree[1][1:]:
            result += ' and ' +alma_tree_to_str(clause)
        return result
    elif tree[0] == 'or':
        result = alma_tree_to_str(tree[1][0])
        for clause in tree[1][1:]:
            result += ' or ' +alma_tree_to_str(clause)
        return result
    elif tree[0] == 'neg':
        return 'not(' + alma_tree_to_str(tree[1]) + ')'
        

def alma_collection_to_strings(collection):
    return [ alma_tree_to_str(tree) for tree in collection]

def actions_to_strings(acts):
    return [ [alma_tree_to_str(t0), alma_tree_to_str(t1)] for [t0, t1] in acts  ]

def kb_action_to_text(kb, action, use_now = False):
    if use_now:
        filtered_kb = [c for c in kb if "time" not in c and "wallnow" not in c and "agentname" not in c]
    else:
        filtered_kb = [c for c in kb if "time" not in c and "now" not in c and "agentname" not in c]
    kb_form = ".".join(filtered_kb)
    action = sorted(action)
    action_form = action[0] + ";" + action[1]
    form = kb_form + "</kb>" + action_form
    return form
    
def get_pred_names(alma_inst):
    L = alma.kb_to_pyobject(alma_inst, 1)
    K= [get_predicates(x) for x in L]
    M = [y for x in K for y in x]
    return sorted(list(set(M)))

def get_predicates(tree):
    # Predicates are 0-ary functions (this means constants count as well).
    # Right now "fif" objects come in as functions, this is probably wrong but
    # we'll treat them as a special case for now.
    if len(tree) == 0:
        return []
    elif tree[0] == 'if':
        return [get_predicates(tree[1])]+ [get_predicates(tree[2])]
    elif tree[0] == 'func' and tree[1] == 'fif':
        return [get_predicates(tree[2][0])]+ [get_predicates(tree[2][1])]
    elif tree[0] == 'func' and len(tree[2]) == 0:
            return tree[1]
    elif tree[0] == 'func' and tree[1] == 'not':
            return get_predicates(tree[2][0])
    elif tree[0] == 'var':
        return []
    elif tree[0] == 'and':
        return [get_predicates(clause) for clause in tree[1]]
    elif tree[0] == 'or':
        return [get_predicates(clause) for clause in tree[1]]
    elif tree[0] == 'neg':
        return get_predicates(tree[1])
    else:
        return []

def classical_step(alma_inst):
    """

    """
    alma.prb_to_res_task(alma_inst, 2.0)
    alma.step(alma_inst)
