"""  Functions for producting vector and graph representations. """
import alma_functions as aw
import numpy as np
import queue
import unif.unifier as un
import re


class node_representation:
    def __init__(self, subjects):
        if type(subjects) == type({}):
            self.subjects_dict = subjects
        elif type(subjects) == type([]):
            self.subjects_dict = dict([(x, i+1) for i, x in enumerate(subjects)])
        else:
            print("Unknown subjects initializer of type ", type(subjects))
            raise TypeError
            
        self.num_subjects = len(self.subjects_dict)
        self.subjects_dict['if'] = self.num_subjects + 1
        self.subjects_dict['and'] = self.num_subjects + 2
        self.subjects_dict['or'] = self.num_subjects + 3
        self.subjects_dict['func'] = self.num_subjects + 4
        self.subjects_dict['var'] = self.num_subjects + 5
        self.subjects_dict['NUMERIC'] = self.num_subjects + 6
        self.subjects_dict['MISC'] = 0
        self.num_subjects = len(self.subjects_dict)


    def gnn_feature(self, node):
        """
        Based on the node type:   
          A function gets a 1-hot representation of the function name
          A var gets a 1-hot representation of the variable name
          Logical connectives get 1- representations of connection
          The 0th position is for numerical values.
        """
        x = np.zeros( self.num_subjects)
        if node[0] in ['if', 'and', 'or', 'func', 'var']:
            x[self.subjects_dict[node[0]]] = 1
        if node[0] == 'func' or node[0] == 'var':
            name = node[1]
            if name.isnumeric():
                x[self.subjects_dict['NUMERIC']] = int(name)
            else:
                try:
                    x[self.subjects_dict[name] ] = 1
                except:
                    #print("Unknown function name {}".format(name))
                    x[0] = 1
        return x

class graph_representation:
    def __init__(self, subjects_dict, max_nodes):
        self.node_rep = node_representation(subjects_dict)
        self.num_nodes = max_nodes
        self.feature_len = self.node_rep.num_subjects
        self.blank_graph = np.zeros( (self.num_nodes, self.num_nodes))

    def gf_helper(self, sentence_tree, num_nodes, feature_len, A, X):
        """
        Node types:
        'func' func_name [parameter list]
        'var' name number
        'if'  [antecedent] [consequent]
        'and' [term-list]
        'or'  [term-list]  (assuming)

        root_idx indicates the index for the root of the tree
        child_offset indicates the number of indices after the root idx where the first child should begin.

        The recursive structures is as follows.   Suppose node idx has m children.
        Then the m children of idx will have indices 
          idx + child_offset, idx + child_offset + 1, ..., idx + child_offset + (m-1)
        These will then be followed by the indices for the children of the first child of idx, etc.

        The function returns a value total, which gives the number of children for the subtree rooted at root_idx.

        The algorithm is then based on a BFS-like processing of the vertices
           1.   Update the incidence matrix A to reflect each child relation.
           2.   Add each child to the queue
           3.   Deqeue a node and process
        """
        nodeq = queue.SimpleQueue()
        nodeq.put((sentence_tree, 0))
        new_node_idx = 1
        while not nodeq.empty():
            sentence_tree, root_idx = nodeq.get()
            term_type = sentence_tree[0]

            if root_idx >= self.num_nodes or new_node_idx >= self.num_nodes:
                #TODO:  We should, at least, fill the available space instead of just bailing.
                print("Graph representation overflowed available space.")
                return
            X[root_idx] = self.node_rep.gnn_feature(sentence_tree)
            if term_type == 'func':
                func_params = sentence_tree[2]
                num_children = len(func_params)
                for i in range(num_children):
                    A[root_idx, new_node_idx] = 1
                    nodeq.put((func_params[i], new_node_idx))
                    new_node_idx += 1
            elif term_type == 'var':
                pass
                #X.append(self.node_rep.gnn_feature(sentence_tree[1]))
                # Nothing for A since this is a leaf node
            elif term_type == 'if':
                #X.append(self.node_rep.gnn_feature('if'))
                A[root_idx, new_node_idx] = 1
                nodeq.put((sentence_tree[1], new_node_idx))
                new_node_idx += 1
                
                A[root_idx, new_node_idx] = 1
                nodeq.put((sentence_tree[2], new_node_idx))
                new_node_idx += 1

            elif term_type == 'and':
                #X.append(self.node_rep.gnn_feature('and'))
                term_list = sentence_tree[1]
                num_terms = len(term_list)
                for i, term in enumerate(term_list):
                    A[root_idx, new_node_idx] = 1
                    nodeq.put((term, new_node_idx))
                    new_node_idx += 1
            elif term_type == 'or':
                #X.append(self.node_rep.gnn_feature('and'))
                term_list = sentence_tree[1]
                for i, term in enumerate(term_list):
                    A[root_idx, new_node_idx] = 1
                    nodeq.put((term, new_node_idx))
                    new_node_idx += 1
            else:
                print("Unknown term type '{}'".format(term_type))
                raise(Exception)


    def graph_features(self, sentence_tree):
    
        A = np.zeros( (self.num_nodes, self.num_nodes) )
        X = np.zeros( (self.num_nodes, self.feature_len) )
        self.gf_helper(sentence_tree, self.num_nodes, self.feature_len, A, X)
        return A, X

    def inputs_to_graphs(self, inp0, inp1):
        graph0, feat0 = self.graph_features(inp0)
        graph1, feat1 = self.graph_features(inp1)
        #return ( np.concatenate(graph0, graph1), np.concatenate(feat0, feat1))
        # This will be a single input to spektral; so we want to create a graph representing a
        # forest of two trees with 2*num_nodes nodes
        return self.graph_join(graph0, graph1, feat0, feat1)
        return ( np.array([graph0, graph1]), np.array([feat0, feat1]))

    def graph_join(self, graph0, graph1, feat0, feat1):
    # This will be a single input to spektral; so we want to create a
    # graph representing a forest of two trees with 2*num_nodes nodes
        joined_graph = np.vstack([  np.hstack([graph0, self.blank_graph]),
                                    np.hstack([self.blank_graph, graph1])
                                    ])
        joined_features = np.vstack([feat0, feat1])
        return joined_graph, joined_features

def unifies(inputs, use_tf=False):
    # for inp in inputs:
    #     print("inp=", inp)
    #     print("x={}      y={}".format(inp[0], inp[1]))
    #     print("Unification: ", (un.unify(un.parse_term(inp[0]), un.parse_term(inp[1]), {})))
    if use_tf:
        return tf.cast([ (un.unify(un.parse_term(x), un.parse_term(y), {}) is not None) for x,y in inputs  ], tf.float32)
    else:
        return np.array([ (un.unify(un.parse_term(x), un.parse_term(y), {}) is not None) for x,y in inputs  ], dtype='float32')

    # For now, simple many-hot representation of a bag-of-words, just on the subjects
def vectorize_bow1(inputs, subjects_dict):
    num_subjects = len(subjects_dict)
    X = []
    for inp0, inp1 in inputs:
        #print("inp0: {} \t inp1: {}\n".format(inp0, inp1))
        #print("inp0 constants:", aw.alma_constants([inp0]))
        #print("inp1 constants:", aw.alma_constants([inp1]))
        x = np.zeros( (2, num_subjects + 1))
        for idx, inp in enumerate([inp0, inp1]):
            for subj in aw.alma_constants([inp], True):
                try:   # distanceAt(a, 5, 17)   -- my distance to a is 5 at time 17
                    x[idx, subjects_dict[subj]] = 1
                except KeyError:
                    # There are now two cases; a genuinely unknown term or something that represents a numerical value.
                    num_parse = re.match("(\d+)/(\d+)", subj)
                    if num_parse is None:
                        x[idx, 0] = 1   # position 0 is a catchall
                    else:
                        num, pos = num_parse.groups()
                        position = subjects_dict["numerical"+pos]
                        x[idx, position] = float(num)
        X.append(x.flatten())
    #print('Vectorized inputs: ', X)
    #return np.array(X)
    return np.array(X, dtype=np.float32)

# Similar to the above, but we don't assume that numerical values are explicitly encoded.
def vectorize_bow2(inputs, subjects_dict):
    num_subjects = len(subjects_dict)
    X = []
    for inp0, inp1 in inputs:
        #print("inp0: {} \t inp1: {}\n".format(inp0, inp1))
        #print("inp0 constants:", aw.alma_constants([inp0]))
        #print("inp1 constants:", aw.alma_constants([inp1]))
        x = np.zeros( (2, num_subjects + 1))
        for idx, inp in enumerate([inp0, inp1]):
            for subj in aw.alma_constants([inp], True):
                try:
                    x[idx, subjects_dict[subj]] = 1
                except KeyError:
                    x[idx, 0] = 1
        X.append(x.flatten())
    #print('Vectorized inputs: ', X)
    #return np.array(X)
    return np.array(X, dtype=np.float32)


# More complex bag of words.   Now, rather than a 1 hot encoding we encode subjects as follows:
# We now want subjects to come in without place identifiers.
# We fix a number of potential place values (say 4).
#
# Each place then gets 16 bits.  If the first bit is 1 the remaining 15 bits represent an integer
# between 0 and 32768.  Otherwise the 15 bits code the subject.

# This basically boils down to an explicit coding of 2^15 integers plus however many subjects are involved.

# So perhaps this is a better way to go; it would probably allow us to use libraries more easily.   


def vectorize_bow3(inputs):
    X = []
    for inp0, inp1 in inputs:
        #print("inp0: {} \t inp1: {}\n".format(inp0, inp1))
        #print("inp0 constants:", aw.alma_constants([inp0]))
        #print("inp1 constants:", aw.alma_constants([inp1]))
        x = np.zeros( (2, self.num_subjects + 1))
        for idx, inp in enumerate([inp0, inp1]):
            for subj in aw.alma_constants([inp], True):
                try:
                    x[idx, self.subjects_dict[subj]] = 1
                except KeyError:
                    x[idx, 0] = 1
        X.append(x.flatten())
    #print('Vectorized inputs: ', X)
    #return np.array(X)
    return np.array(X, dtype=np.float32)


def graph_rep_test():
    test_input0= ['func', 'distanceAt', [['func', 'a', []], ['func', '9', []], ['func', '0', []]]]
    test_input1 = ['if', ['and', [['func', 'distanceAt', [['var', 'Item2', 0], ['var', 'D2', 3], ['var', 'T', 2]]], ['func', 'distanceAt', [['func', 'b', []], ['var', 'D1', 1], ['var', 'T', 2]]]]], ['func', 'distanceBetweenBoundedBy', [['var', 'Item2', 0], ['func', 'b', []], ['var', 'D1', 1], ['var', 'T', 2]]]]
    # desired nodes for input1:  0: 'if', 1: 'and', 2: 'func dBB', 3: 'func dA', 4: 'func dA'
    gr = graph_representation(dict([(x, i+1) for i, x in enumerate(['a', 'b', 'distanceAt', 'distanceBetweenBoundedBy'])]), 30)
    gr.inputs_to_graphs(test_input0, test_input1)

graph_rep_test()
