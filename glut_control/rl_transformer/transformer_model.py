import torch
from torch import nn
import transformers




""" Transformer model for prioritizing actions.  Intuitively, the
input is a state and a list of actions; the output is a vector
representing approximate Q-values for each action. 

In more detail, our first pass at the architecture will be as follows.
We conceive of the model as a kind of sequence2value model which
approximates Q(a; k) where a is a sequence of actions and k is the
knowledge base.  We will model this with an encoder-decoder
architecture, where the encoder encodes the knowledge base which is fed in 
as a context to the decoder.  

The decoder is fed each individual action and uses the encoded context vector to output an estimated Q-value.  
 """



class transformer_model(nn.Module):
    def __init__(self, input_size = 512, num_heads=4, debugging=False):
        self.debugging = debugging
        self.input_size = input_size

        self.input_size = input_size
        self.max_nodes = max_nodes
        self.num_features = num_features


        self.state_input_size = (max_nodes)**2 + (max_nodes*num_features) # We'll feed in KB formulae one at a time
        # Define the model
        self.encoder =



        else:
            self.emb_input = self.model_input

        if self.tensorflow_backend:
            self.dense1 = keras.layers.Dense(8, activation='tanh')(self.emb_input)
            self.dense2 = keras.layers.Dense(8, activation='tanh')(self.dense1)
            self.output = keras.layers.Dense(1, activation='sigmoid')(self.dense2)
        else:
            self.dense1 = torch.nn.linear(out_features = 8)(self.emb_input)
            #self.dense1 = keras.layers.Dense(8, activation='tanh')(self.emb_input)
            self.dense2 = torch.nn.linear(out_features=8, activation='tanh')(self.dense1)
            self.output = keras.layers.Dense(out_features1, activation='sigmoid')(self.dense2)

        if False:
            self.model = keras.models.Sequential()
            self.model.add(keras.Input(shape=( (  input_size,)) ))    # One input for each term
            self.model.add(keras.layers.Dense(8, activation='tanh'))
            self.model.add(keras.layers.Dense(8, activation='tanh'))
            self.model.add(keras.layers.Dense(1, activation='sigmoid'))

        if use_state:
            self.model = keras.Model(
                inputs = [self.state_input, self.model_input],
                outputs = [self.output]
            )
        else:
            self.model = keras.Model(
                inputs = [self.model_input],
                outputs = [self.output]
            )
        #self.model.compile(loss='mse', optimizer='adam', metrics=['mse'], run_eagerly=self.debugging)
        self.model.compile(loss='mse', optimizer='adam', metrics=['mse'], run_eagerly=False)

        self.optimizer = Adam(learning_rate=0.00025, clipnorm=1.0)
        self.loss_fn = keras.losses.Huber()
        self.acc_fn = BinaryAccuracy()
        if self.debugging:
            self.train_loss =  tf.keras.metrics.Mean('train_loss', dtype=tf.float32)
            self.train_summary_writer = tf.summary.create_file_writer(tb_log_dir)

    def tb_summary(self, x):
        tf.summary.histogram('x', x)
        return x


    def flatten_input(self, X):
        """
        Inputs come in as pairs X[0], X[1] representing the graph and node features, respectively.
        X[0] is (2*max_nodes)x(2*max_nodes)
        X[1] is ((2*max_nodes)x(num_features)

        """
        return np.hstack([X[0], X[1]]).flatten()

    #@profile
    def fit(self, inputs, y, batch_size, verbose=True, callbacks=[]):
        if self.use_state:
            # Coming in, we have inputs = (states, actions) where:
            #            actions is a list of batch_size pairs (graph, features)
            #            states is a list of batch_size, each element of which is
            #                   a variable-length list of (graph, features) pairs
            # What we want to feed into the network should flatten the pairs:
            #    stateX should be a tensor of rank (batch_size, ???, graph_size + feature_size)
            #    actions should be a tensor of rank (batch_size, graph_size + feature_size)
            states = inputs[0]
            actions = inputs[1]
            stateX = [np.array([self.flatten_input(Xi) for Xi in kb]) for kb in states]
        else:
            actions = inputs
        actionX = np.array([self.flatten_input(Xi) for Xi in actions])   # TODO:  Make this more efficient
        y = np.array(y)
        if self.use_state:
            #Iterate through the batch manually so that we can use variable length states.
            #TODO:  Find a way to process a batch at a time -- this might involve using ragged tensors
            #       or padding.
            for j in range(len(actions)):
                state, action = stateX[j], actionX[j]
                ybatch = y[j]
                H = self.model.fit({"action_input": np.expand_dims(action, axis=0),
                                    "state_input": np.expand_dims(state, axis=0)},
                                    ybatch, callbacks=callbacks, epochs=1)
        else:
            num_batches = len(actions) // batch_size
            if len(actionX) % batch_size != 0:
                num_batches += 1    # Extra batch for remainder
            for i in range(num_batches):
                actionXbatch = actionX[i*batch_size:(i+1)*batch_size]
                ybatch = y[i*batch_size:(i+1)*batch_size]
                #loss, acc, tacc = self.model.fit(Xbatch, ybatch)
                H = self.model.fit(actionXbatch, ybatch, callbacks=callbacks)

            return H

    def predict(self, X):
        if self.use_state:
            states = X[0]
            actions = X[1]
            actionX = np.array([self.flatten_input(Xi) for Xi in actions])   # TODO:  Make this more efficient
            stateX = [np.array([self.flatten_input(Xi) for Xi in kb]) for kb in states]
            preds = np.array([self.model.predict({"action_input": np.expand_dims(action,axis=0), "state_input": np.expand_dims(state, axis=0)}) for action, state in zip(actionX, stateX)]).reshape(-1,1)
        else:
            X = np.array([self.flatten_input(Xi) for Xi in X])
            preds = self.model.predict(X)
        return preds

    def save(self, id_str):
        self.model.save("rl_model_{}".format(id_str))

    def load(self, id_str):
        self.model.load_model("rl_model_{}".format(id_str))
