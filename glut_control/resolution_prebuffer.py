"""
Reinforcement learning functionality.

Currently, the following are planned:
  a) A neural network that takes two alma_functions as parameters and outputs a probability that these will not unify.
  b) 
  c) 
"""

import tensorflow as tf
from tensorflow import keras
from tensorflow.keras.models import Model
from tensorflow.keras.optimizers import Adam
from tensorflow.keras.losses import BinaryCrossentropy
from tensorflow.keras.metrics import BinaryAccuracy
from tensorflow.keras.layers import Dense, Dropout
#from spektral.layers import GCNConv, GlobalSumPool
#from spektral.data.graph import Graph
#from spektral.data import DisjointLoader

#import spektral
#import spektral.models

import numpy as np
#from rl_dataset import potential_inference_data, simple_graph_dataset
import sys
import pickle
import alma_functions as aw
from sklearn.utils import shuffle
from vectorization import graph_representation, unifies, vectorize_bow1, vectorize_bow2

def forward_model(use_tf = False, num_subjects=0):
    # TODO:  fix use_tf code to return everything if this ever gets used.
    if use_tf:
        import tensorflow as tf
        model  = tf.keras.models.Sequential([
            tf.keras.Input(shape=( (  2*num_subjects,)) ),    # One input for each term
            tf.keras.layers.Dense(32, activation='relu'),
            tf.keras.layers.Dropout(0.2),
            tf.keras.layers.Dense(1),
            tf.keras.layers.Softmax()
        ])
        loss_object = tf.keras.losses.BinaryCrossentropy(from_logits=True)
        optimizer = tf.keras.optimizers.Adam()
        train_loss = tf.keras.metrics.Mean(name='train_loss')
        train_accuracy = tf.keras.metrics.SparseCategoricalAccuracy(name='train_accuracy')
        test_loss = tf.keras.metrics.Mean(name='test_loss')
        test_accuracy = tf.keras.metrics.SparseCategoricalAccuracy(name='test_accuracy')
        print("Model summary:", self.model.summary())
    else:
        model = keras.models.Sequential()
        model.add(keras.Input(shape=( (  2*num_subjects,)) ))    # One input for each term
        model.add(keras.layers.Dense(8, activation='tanh'))
        model.add(keras.layers.Dense(8, activation='tanh'))
        model.add(keras.layers.Dense(1, activation='sigmoid'))
        model.compile(loss='binary_crossentropy', optimizer='adam', metrics=['accuracy', 'binary_crossentropy'])
    return model

class history_struct:
    def __init__(self, d):
        self.history = d
        
class gnn_model(Model):
    def __init__(self, max_nodes=20):
        super().__init__()
        # TODO:  See if tanh works better as a hidden activation function
        self.graph_net = spektral.models.general_gnn.GeneralGNN(1, activation='sigmoid', hidden=16, message_passing=4,
                                                                 pre_process=2, post_process=2, connectivity='cat', batch_norm=True,
                                                                 dropout=0.2, aggregate='sum', pool='sum', hidden_activation='sigmoid')
        #self.graph_net = spektral.models.gcn.GCN(1)
        self.graph_net.compile('adam', 'binary_crossentropy')
        self.optimizer = Adam(learning_rate=0.00025, clipnorm=1.0)
        #self.loss_fn = BinaryCrossentropy()
        self.loss_fn = keras.losses.Huber()
        self.acc_fn = BinaryAccuracy()

    def call(self, inputs):
        return self.graph_net(inputs)

    # Training function
    #@tf.function(input_signature=loader_tr.tf_signature(), experimental_relax_shapes=True)
    def train_on_batch(self, inputs, target):
        with tf.GradientTape() as tape:
            predictions = self.graph_net(inputs, training=True)
            loss = self.loss_fn(target, predictions) + sum(self.graph_net.losses)
            acc = self.acc_fn(target, predictions)
            preds_01 = np.array([ 0.0 if p <= 0.5 else 1 for p in predictions])
            #tacc = np.square(preds_01 - target.T).mean()
            tacc = (1 - abs(preds_01 - target.T)).mean()
            gradients = tape.gradient(loss, self.graph_net.trainable_variables)
            self.optimizer.apply_gradients(zip(gradients, self.graph_net.trainable_variables))
        return loss, acc, tacc

    def fit(self, X, y, batch_size=16, verbose=True):
        dataset = simple_graph_dataset(X,y)
        loader = DisjointLoader(dataset, batch_size = batch_size)
        preds = []
        #results = []
        total_loss = 0
        total_acc = 0
        total_tacc = 0
        num_epochs = len(X) // batch_size
        if len(X) % batch_size != 0:
            num_epochs += 1    # Extra batch for remainder
        for i in range(num_epochs):
            b = loader.__next__()
            #b[0] looks like features, b[1] looks like graph, b[2] is targets, b[3] is some kind of large index array
            inputs = (b[0], b[1], b[3])
            target = b[2]
            loss, acc, tacc = self.train_on_batch(inputs, target )
            total_loss += loss
            total_acc += acc
            total_tacc += tacc
            #results.append((loss, acc))
            #preds.append(batch_preds.numpy())
        return history_struct({'accuracy': [(total_acc / num_epochs)],
                               'loss': [(total_loss / num_epochs)],
                               'tacc': [(total_tacc / num_epochs)]
                               })


class gnn_model_zero():
    def __init__(self, max_nodes=20, num_features=20):
        #gnn_model.__init__(self, max_nodes)
        input_size = (2*max_nodes)**2 + (2*max_nodes*num_features)
        self.input_size = input_size
        self.max_nodes = max_nodes
        self.num_features = num_features
        # Function API version; don't use this for now
        # input = keras.Input(shape=(input_size,))
        # x1 = keras.layers.Dense(8, activation='tanh')(input)
        # x2 = keras.layers.Dense(8, activation='tanh')(x1)
        # out = keras.layers.Dense(1, activation='sigmoid')(x2)
        # self.graph_net = out

        self.model = keras.models.Sequential()
        self.model.add(keras.Input(shape=( (  input_size,)) ))    # One input for each term
        self.model.add(keras.layers.Dense(8, activation='tanh'))
        self.model.add(keras.layers.Dense(8, activation='tanh'))
        self.model.add(keras.layers.Dense(1, activation='sigmoid'))
        self.model.compile(loss='mse', optimizer='adam', metrics=['accuracy', 'mse'])

        self.optimizer = Adam(learning_rate=0.00025, clipnorm=1.0)
        self.loss_fn = keras.losses.Huber()
        self.acc_fn = BinaryAccuracy()

    
    def flatten_input(self, X):
        """
        Inputs come in as pairs X[0], X[1] representing the graph and node features, respectively.
        X[0] is (2*max_nodes)x(2*max_nodes)
        X[1] is ((2*max_nodes)x(num_features)

        """
        return np.hstack([X[0], X[1]]).flatten()
    
    def fit(self, X, y, batch_size, verbose=True, callbacks=[]):
        X = np.array([self.flatten_input(Xi) for Xi in X])   # TODO:  Make this more efficient
        y = np.array(y)
        num_epochs = len(X) // batch_size
        if len(X) % batch_size != 0:
            num_epochs += 1    # Extra batch for remainder
        for i in range(num_epochs):
            Xbatch = X[i*batch_size:(i+1)*batch_size]
            ybatch = y[i*batch_size:(i+1)*batch_size]
            #loss, acc, tacc = self.model.fit(Xbatch, ybatch)
            H = self.model.fit(Xbatch, ybatch, callbacks=callbacks)
        return H

    def predict(self, X):
        X = np.array([self.flatten_input(Xi) for Xi in X])
        preds = self.model.predict(X)
        return preds
    
    def save(self, id_str):
        self.model.save("rl_model_{}".format(id_str))

    def load(self, id_str):
        self.model.load_model("rl_model_{}".format(id_str))


class res_prebuffer:
    def __init__(self, subjects, words, use_tf=False, debug=True, use_gnn=False, gnn_nodes = 2000, bow_algorithm='bow1'):
        self.use_tf = use_tf

        self.subjects = subjects
        self.words = words
        self.num_subjects = len(subjects)
        self.num_words = len(words)
        self.subjects_dict = {}
        for idx, subj in enumerate(self.subjects):
            self.subjects_dict[subj] = idx+1
        if bow_algorithm == 'bow2':
            self.subjects_dict["numerical0"] = self.num_subjects+1
            self.subjects_dict["numerical1"] = self.num_subjects+2
            self.subjects_dict["numerical2"] = self.num_subjects+3


        self.words_dict = {}
        for idx, word in enumerate(self.words):
            self.words_dict[word] = idx+1
        self.batch_size = 32
        self.debug=debug
        if self.debug:
            self.saved_prbs = []
            self.saved_inputs = []

        self.Xbuffer = []
        self.ybuffer = []
        self.yneg_count = 0
        self.ypos_count = 0
        print("Subjects dictionary:", self.subjects_dict)

        self.use_gnn = use_gnn
        self.max_gnn_nodes = gnn_nodes
        self.vectorize_alg = 'gnn1' if use_gnn else bow_algorithm
        if use_gnn:
            #self.graph_buffer = potential_inference_data()
            self.graph_buffer = None
            self.graph_rep = graph_representation(self.subjects_dict, self.max_gnn_nodes)

        self.model = forward_model(use_tf, len(self.subjects_dict)+1) if not use_gnn else gnn_model_zero(self.max_gnn_nodes,self.graph_rep.feature_len)
    



    def fmla_to_graph(self, inputs):
        A, X = [], []
        for inp0, inp1 in inputs:
            graph_struct, graph_features = inputs_to_graphs(inp0, inp1)
            A.append(graph_struct)
            X.append(graph_features)

    # Use word2vec for the embeddings
    def vectorize_w2vec(self, inputs):
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


    def vectorize(self, inputs):
        if self.vectorize_alg == 'bow1':
            return vectorize_bow1(inputs, self.subjects_dict)
        elif self.vectorize_alg == 'bow2':
            return vectorize_bow2(inputs, self.subjects_dict)
        elif self.vectorize_alg == 'gnn1':
            res = []
            for inp0, inp1 in inputs:
                #print(inp0, inp1)
                res.append(self.graph_rep.inputs_to_graphs(inp0, inp1))
            #print('done')
            #test = np.array(res)
            return res
            #  after debugging use this
            #  return np.array([ self.graph_rep.inputs_to_graphs(inp0, inp1) for inp0, inp1 in inputs ])
        elif self.vectorize_alg == 'word2vec':
            return self.vectorize_w2vec(inputs)
        


    # Save to some buffer; use for training when we can have reasonably balanced samples.
    # The buffers will consist of numpy arrays
    
    def save_batch(self, inputs, prb_strings):
        X = self.vectorize(inputs)
        y0 = unifies(prb_strings)

        if self.use_tf:
            y = tf.reshape(y0, (-1, 1))
        else:
            y = np.reshape(y0, (-1, 1))
        for i in range(len(X)):
            self.Xbuffer.append(X[i])
            self.ybuffer.append(y0[i])
            # if self.use_gnn:
            #     self.graph_buffer.add(inputs[i], y0[i])
            if self.debug or self.use_gnn:
                self.saved_inputs.append(inputs[i])
                self.saved_prbs.append(prb_strings[i])

        ypos = sum(y0)
        yneg = len(y0) - ypos
        self.yneg_count += yneg
        self.ypos_count += ypos

    """  Get rid of excess samples; this should help prevent memory leaks. """
    def clean(self):
        if self.debug:
            self.Xbuffer, self.ybuffer, self.saved_prbs, self.saved_inputs = shuffle(self.Xbuffer, self.ybuffer, self.saved_prbs, self.saved_inputs)
        else:
            self.Xbuffer, self.ybuffer = shuffle(self.Xbuffer, self.ybuffer)

        pos_floor  = self.batch_size
        neg_floor = self.batch_size
        while (self.ypos_count > pos_floor) and (self.yneg_count > neg_floor):
            x, y = self.Xbuffer.pop(), self.ybuffer.pop()
            if self.debug:
               self.saved_prbs.pop(), self.saved_inputs.pop()
            if y == 0:
                self.yneg_count -= 1
            else:
                self.ypos_count -= 1
            

    # Get a batch (X,y) of num_samples training pairs, 
    # Here X will be a (num_samples, input_size) input array
    # and y will be a (num_samples, 1) training vector.
    def get_training_batch(self, num_samples, num_pos):
        if self.debug:
            self.Xbuffer, self.ybuffer, self.saved_prbs, self.saved_inputs = shuffle(self.Xbuffer, self.ybuffer, self.saved_prbs, self.saved_inputs)
        else:
            self.Xbuffer, self.ybuffer = shuffle(self.Xbuffer, self.ybuffer)

        total_samples = 0
        total_pos = 0
        Xres, yres = [], []
        num_neg = num_samples - num_pos
        while (total_samples < num_samples):
            #print("Total samples: {} \t Xbuf_len: {}  \t ybuf_len: {}\t\t\t pos: {} \t neg: {}\n".format(total_samples, len(self.Xbuffer), len(self.ybuffer), total_pos, total_samples - total_pos))
            x, y = self.Xbuffer.pop(), self.ybuffer.pop()
            if self.debug:
                dbg_prb, dbg_input = self.saved_prbs.pop(), self.saved_inputs.pop()
            total_neg = total_samples - total_pos
            needed_pos = num_pos - total_pos
            needed_neg  = num_neg - total_neg
            if y == 0:
                self.yneg_count -= 1
                # Only add a negative sample if there's still enough
                # room for the remaining positives
                if (total_samples + needed_pos) < num_samples:
                    Xres.append(x)
                    yres.append(y)
                    total_samples += 1
            else:
                self.ypos_count -= 1
                # Only add a positive sample if there's still enough
                # room for the remaining negatives
                if (total_samples + needed_neg) < num_samples:
                    Xres.append(x)
                    yres.append(y)
                    total_samples += 1
                    total_pos += 1
                if self.debug:
                    pass
                    #print("Positive sample: \t {} -- {}\n".format(dbg_prb[0], dbg_prb[1]))

        if self.debug:
            print("-"*80)
        if self.use_gnn:
            return Xres, yres
        else:
            return np.array(Xres), np.array(yres)

    def train_buffered_batch(self, sample_ratio=0.5):
        X, y0 = self.get_training_batch(self.batch_size, int(self.batch_size*sample_ratio))
        if self.use_tf:
            y = tf.reshape(y0, (-1, 1))
        else:
            y = y0

        if self.use_tf:
            with tf.GradientTape() as tape:
                predb = self.model(X)
                loss = self.loss_object(y, predb)
            gradients = tape.gradient(loss, self.model.trainable_variables)
            self.optimizer.apply_gradients(zip(gradients, self.model.trainable_variables))
            print("y, pred:", y.numpy().T, pred.numpy().T)
            print("loss: {} \t acc: {}\n".format(self.train_loss(loss), self.train_accuracy(y, pred), end='\r'))
            return pred
        else:
            # TODO:  make sure this continues training rather than reinitializing
            return self.model.fit(X, y, batch_size=self.batch_size, verbose=True)

    def test_buffered_batch(self, sample_ratio=0.5):
        X, y0 = self.get_training_batch(self.batch_size, int(self.batch_size*sample_ratio))
        if self.use_tf:
            y = tf.reshape(y0, (-1, 1))
        else:
            y = y0

        preds = self.get_priorities(X, True)
        preds_01 = np.array([ 0.0 if p < 0.5 else 1 for p in preds])
        #accuracy = np.count_nonzero(preds_01 != y) / len(preds)
        accuracy = self.model.acc_fn(y, preds)
        tacc = (1 - abs(preds_01 - y)).mean()
        loss = np.square(preds - y).mean()
        return history_struct({'accuracy': [accuracy],
                               'loss': [loss],
                               'tacc':  [tacc]})

    #@tf.function
    def train_batch(self, inputs, prb_strings):
        #print("inputs=", inputs)
        X = self.vectorize(inputs)
        #print("X=", X)
        y0 = self.unifies(prb_strings)
        if self.use_tf:
            y = tf.reshape(y0, (-1, 1))
        else:
            y = np.reshape(y0, (-1, 1))
            #print("y=", y)
        preds = []
        if self.use_tf:
            num_batches = X.shape[0] // self.batch_size
            for b in range(num_batches):
                Xb = X[b*self.batch_size : (b+1)*self.batch_size]
                yb = y[b*self.batch_size : (b+1)*self.batch_size]
                with tf.GradientTape() as tape:
                    predb = self.model(Xb)
                    loss = self.loss_object(yb, predb)
                    preds.append(predb)
                gradients = tape.gradient(loss, self.model.trainable_variables)
                self.optimizer.apply_gradients(zip(gradients, self.model.trainable_variables))
                print("yb, predb:", yb.numpy().T, predb.numpy().T)
                print("loss: {} \t acc: {}\n".format(self.train_loss(loss), self.train_accuracy(yb, predb), end='\r'))
            return preds
        else:
            ones = sum(y0)
            zeros = len(y0) - ones
            if ones*zeros > 0:
                weights = {
                    0: ones,
                    1: zeros
                }
                self.model.fit(X, y, batch_size=self.batch_size, class_weight=weights)
            else:
                self.model.fit(X, y, batch_size=self.batch_size)

    def get_priorities(self, inputs, already_vectorized=False):
        X = inputs if already_vectorized else self.vectorize(inputs)
        if self.use_gnn and False:   # TODO:  remove False; check for gnn_zero
            #Xgraph = [ Graph(x,a) for a,x in X]
            # Want inputs to be a pair X, A where
            # X is (num_batches, num_nodes, num_features) and
            # A is (num_batches, num_nodes, num_nodes)
            # Want targets to be (num_batches,) I think.
            # Xlist = []
            # Alist = []
            # for a,x in X:
            #     Alist.append(a)
            #     Xlist.append(x)
            # inputs = (  np.array(Xlist), np.array(Alist))
            # t = np.zeros(len(Xlist))  # don't really matter
            #preds = self.model((inputs,t))
            dataset = simple_graph_dataset(X)
            loader = DisjointLoader(dataset, batch_size = self.batch_size)
            preds = []
            num_epochs = len(X) // self.batch_size
            if len(X) % self.batch_size != 0:
                num_epochs += 1    # Extra batch for remainder
            for i in range(num_epochs):
                b = loader.__next__()
                # b is now a triple:  b[0] is a batch of X, b[1] is a batch of A (as a SparseTensor),
                # b[2] is is some kind of index array
                inputs = (b[0], b[1], b[2])
                batch_preds = self.model.graph_net(inputs, training=True)
                preds.append(batch_preds.numpy())
            result = np.concatenate(preds).reshape(-1)
            return result
        else:
            preds = self.model.predict(X)
        return preds

    def model_save(self, id_str, numeric_bits):
        pkl_file = open("rl_model_{}.pkl".format(id_str), "wb")
        pickle.dump((self.subjects, self.words, self.num_subjects, self.num_words, self.subjects_dict, self.words_dict, self.batch_size, self.use_tf, numeric_bits, self.use_gnn, self.max_gnn_nodes, self.vectorize_alg), pkl_file)
        if self.use_gnn and False:
            self.model.graph_net.save("rl_model_{}".format(id_str))
            print("GNN trainable weights:")
            print(self.model.graph_net.trainable_weights)
        else:
            self.model.save("rl_model_{}".format(id_str))

    def model_load(self, id_str):
        if self.use_gnn:
            # print("Model loading not yet implemented.")
            # raise NotImplementedError
            # TODO:  test the following code
            pkl_file = open("rl_model_{}.pkl".format(id_str), "rb")
            (self.subjects, self.words, self.num_subjects, self.num_words, self.subjects_dict, self.words_dict, self.batch_size, self.use_tf, numeric_bits, self.use_gnn, self.max_gnn_nodes,self.vectorize_alg ) = pickle.load(pkl_file)
            #self.model.graph_net = keras.models.load_model("rl_model_{}".format(id_str))   #TODO:  Handle both types of gnn
            self.model.load(id_str)
        else:
            self.model = keras.models.load_model("rl_model_{}".format(id_str))

    def unify_likelihood(self, inputs):
        X = self.vectorize(inputs)
        return self.model.predict(X)
    

def rpf_load(id_str, debug=False):
    pkl_file = open("rl_model_{}.pkl".format(id_str), "rb")
    subjects, words, num_subjects, num_words, subjects_dict, words_dict, batch_size, use_tf, num_bits, use_gnn, gnn_nodes, vectorize_alg = pickle.load(pkl_file)
    rpf = res_prebuffer(subjects, words, use_tf, debug, use_gnn, gnn_nodes, vectorize_alg)
    rpf.num_subjects = num_subjects
    rpf.num_words = num_words
    rpf.subjects_dict = subjects_dict
    rpf.words_dict = words_dict
    rpf.batch_size = batch_size
    rpf.model_load(id_str)
    rpf.num_bits = num_bits
    return rpf
