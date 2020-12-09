"""  Functions for producting vector and graph representations. """
import alma_functions
import numpy as np



class node_represention:
    def __init__(self, subjects_dict):
        self.subjects_dict = subjects_dict
        self.num_subjects = len(subjects_dict)
        self.subjects_dict['if'] = self.num_subjects + 0
        self.subjects_dict['and'] = self.num_subjects + 1
        self.subjects_dict['or'] = self.num_subjects + 2
        self.subjects_dict['func'] = self.num_subjects + 3
        self.subjects_dict['var'] = self.num_subjects + 4
        self.subjects_dict['NUMERIC'] = self.num_subjects + 5
        self.num_subjects = len(subjects_dict)

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
                    print("Unknown function name {}".format(name))
                    x[0] = 1
        return x

class graph_representation:
    def __init__(self, subjects_dict, max_nodes):
        self.node_rep = node_represention(subjects_dict)
        self.num_nodes = max_nodes
        self.feature_len = self.node_rep.num_subjects

    def gf_helper(self, sentence_tree, num_nodes, feature_len, A, X, root_idx):
        """
        Node types:
        'func' func_name [parameter list]
        'var' name number
        'if'   [antecedent] [consequent]
        'and' [term-list]
        'or'  [term-list]  (assuming)
        """
        term_type = sentence_tree[0]
        X.append(self.node_rep.gnn_feature(sentence_tree))
        if term_type == 'func':
            func_params = sentence_tree[2]
            num_params = len(func_params)
            for i in range(num_params):
                A[root_idx, root_idx + i + 1] = 1
                self.gf_helper(func_params[i], num_nodes, feature_len, A, X, root_idx+i+1)
        elif term_type == 'var':
            pass
            #X.append(self.node_rep.gnn_feature(sentence_tree[1]))
            # Nothing for A since this is a leaf node
        elif term_type == 'if':
            #X.append(self.node_rep.gnn_feature('if'))
            A[root_idx, root_idx + 1] = 1
            A[root_idx, root_idx + 2] = 1
            self.gf_helper(sentence_tree[1], num_nodes, feature_len, A, X, root_idx+1)
            self.gf_helper(sentence_tree[2], num_nodes, feature_len, A, X, root_idx+2)
        elif term_type == 'and':
            #X.append(self.node_rep.gnn_feature('and'))
            term_list = sentence_tree[1]
            for i, term in enumerate(term_list):
                A[root_idx, root_idx + i + 1] = 1
                self.gf_helper(term, num_nodes, feature_len, A, X, root_idx+i+1)
        elif term_type == 'or':
            #X.append(self.node_rep.gnn_feature('and'))
            term_list = sentence_tree[1]
            for i, term in enumerate(term_list):
                A[root_idx, root_idx + i + 1] = 1
                self.gf_helper(term, num_nodes, feature_len, A, X, root_idx+i+1)
        else:
            print("Unknown term type '{}'".format(term_type))
            raise(Exception)


    def graph_features(self, sentence_tree):
    
        A = np.zeros( (self.num_nodes, self.num_nodes) )
        #X = np.zeros( (self.num_nodes, self.feature_len) )
        X = []
        self.gf_helper(sentence_tree, self.num_nodes, self.feature_len, A, X, 0)
        return A, np.array(X)

    def inputs_to_graphs(self, inp0, inp1):
        graph0, feat0 = self.graph_features(inp0)
        graph1, feat1 = self.graph_features(inp1)
        #return ( np.concatenate(graph0, graph1), np.concatenate(feat0, feat1))
        return ( (graph0, graph1), (feat0, feat1))        


        
        

def gnn_feature(tree_root):
    return np.zeros(16)




def unifies(inputs):
    # for inp in inputs:
    #     print("inp=", inp)
    #     print("x={}      y={}".format(inp[0], inp[1]))
    #     print("Unification: ", (un.unify(un.parse_term(inp[0]), un.parse_term(inp[1]), {})))
    if self.use_tf:
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


def vectorize_bow2(inputs):
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


def main():
    test_input0= ['func', 'distanceAt', [['func', 'a', []], ['func', '9', []], ['func', '0', []]]]
    test_input1 = ['if', ['and', [['func', 'distanceAt', [['var', 'Item2', 0], ['var', 'D2', 3], ['var', 'T', 2]]], ['func', 'distanceAt', [['func', 'b', []], ['var', 'D1', 1], ['var', 'T', 2]]]]], ['func', 'distanceBetweenBoundedBy', [['var', 'Item2', 0], ['func', 'b', []], ['var', 'D1', 1], ['var', 'T', 2]]]]
    gr = graph_representation(dict([(x, i+1) for i, x in enumerate(['a', 'b', 'distanceAt', 'distanceBetweenBoundedBy'])]), 10)
    gr.inputs_to_graphs(test_input0, test_input1)
main()
