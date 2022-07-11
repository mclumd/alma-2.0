import alma
import random

def explosion(size, kb, alma_inst):
    """
    The one axiom we work with:
       if(and(distanceAt(Item1, D1, T), distanceAt(Item2, D2, T)), distanceBetweenBoundedBy(D1, Item1, Item2, T)).
    """
    ilist = list(range(size))
    if size>0:
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
        alma.astep(alma_inst)
    r = alma.prebuf(alma_inst)
    #print("Explosion done.")
    #print("="*80)
    return r

def res_task_lits(lit_str):
    L = lit_str.split('\n')[:-1]
    return [ x.split('\t') for x in L]

def kb_to_subjects_list(kb, use_gnn=False):
    subjects = None
    if "test1_kb.pl" in kb:
        subjects = ['a', 'b', 'distanceAt', 'distanceBetweenBoundedBy']
    elif "january_preglut.pl" in kb:
        subjects = ["a{}".format(x) for x in range(explosion_steps)]

    if "test1_kb.pl" in kb and not use_gnn:
        for place in range(3):
            for cat_subj in ['a', 'b']:
                subjects.append("{}/{}".format(cat_subj, place))
            for num_subj in range(2**numeric_bits):
                subjects.append("{}/{}".format(num_subj, place))

    if "qlearning1.pl" in kb:
        subjects = ['f', 'g', 'a']
    if "qlearning2.pl" in kb:
        subjects = ['l', 'f', 'g', 'a']
    if "qlearning3.pl" in kb:
        subjects = ['loc', 'left', 'right', 'up', 'down', 'a']
    if "qlearning4.pl" in kb:
        subjects = ['loc', 'left', 'right', 'a']
    if subjects == None:
        print("Unknown vocabulary for {}".format(kb))
    else:
        subjects += ["now"]

    return subjects
        
