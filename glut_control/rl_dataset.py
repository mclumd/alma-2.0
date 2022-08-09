#from spektral.data import Dataset, Graph
from sklearn.utils import shuffle
import numpy as np
import os
import copy

"""

"""
# class simple_graph_dataset(Dataset):
#     def __init__(self, ax_list, y=None, **kwargs):
#         if y==None:
#             self.graph_list = [Graph(x, a) for a, x in ax_list]
#         else:
#             self.graph_list = [Graph(x, a,np.array([y]) ) for (a, x),y in zip(ax_list, y)]
#         #self.path = "/tmp/sgd"
#         super().__init__(**kwargs)
#
#     def download(self):
#         if not os.path.exists(self.path):
#             os.mkdir(self.path)
#
#     def read(self):
#         return self.graph_list

"""
Create a dataset and run it through a GeneralGNN.  Surprisingly difficult!
"""
def test_with_gen():
    import numpy as np
    from spektral.models.general_gnn import GeneralGNN
    from spektral.data import DisjointLoader

    graph_net = GeneralGNN(1, activation=None, hidden=256,
                           message_passing=4, pre_process=2, post_process=2,
                           connectivity='cat', batch_norm=True, dropout=0.0, aggregate='sum',
                           hidden_activation='prelu', pool='sum')
    a = np.array([ [1,0,1], [1,1,0], [1,1,1]])
    x = np.array([ [1,0], [1,1], [0,1]])
    A = [a, a]
    X = [x, x]
    ds = simple_graph_dataset(A, X)
    loader1 = DisjointLoader(ds, batch_size=2)
    batch = loader1.__next__()
    print(graph_net(batch))
    # for batch in loader1:
    #     print(graph_net(batch))
        
    
"""
Eventually, this should be a good way to do the experience replay.
"""
# class potential_inference_data(Dataset):
#     def __init__(self, saved_inputs, Xbuffer, ybuffer, **kwargs):
#         self.nodes=100
#         self.feats=20
#         self.saved_inputs = saved_inputs
#         self.Xbuffer = Xbuffer
#         self.ybuffer = ybuffer
#         super().__init__(**kwargs)
#
#     def download(self):
#         os.mkdir(self.path)
#
#     def read(self):
#         return []
#
#
#     def add(self, input, reward):
#         pass
    
        
class replay_batch:
    def __init__(self, s0, a, r, s1, pa):
        self.states0 = s0
        self.actions = a
        self.rewards = r
        self.states1 = s1
        self.potential_actions = pa

class experience_replay_buffer:
    def __init__(self):
        self.states0, self.actions, self.rewards, self.states1, self.potential_actions = [], [], [], [], []

    def num_entries(self):
        return len(self.actions)

    def append(self, states0, actions, rewards, states1, potential_actions):
        if actions != []:
            self.states0.append(states0)
            self.actions.append(actions)
            self.rewards.append(rewards)
            self.states1.append(states1)
            self.potential_actions.append(potential_actions)

    def copy(self):
        new_rb = experience_replay_buffer()
        new_rb.states0 = copy.deepcopy(self.states0)
        new_rb.actions = copy.deepcopy(self.actions)
        new_rb.rewards = copy.deepcopy(self.rewards)
        new_rb.states1 = copy.deepcopy(self.states1)
        return new_rb

    def extend(self, states0, actions, rewards, states1, potential_actions):
        assert(len(states0) == len(actions))
        assert(len(states0) == len(rewards))
        assert(len(states0) == len(states1))
        self.states0.extend(states0)
        self.actions.extend(actions)
        self.rewards.extend(rewards)
        self.states1.extend(states1)
        self.potential_actions.extend(potential_actions)
        
    def get_batch(self, size):
        self.states0, self.actions, self.rewards, self.states1, self.potential_actions = shuffle(self.states0, self.actions, self.rewards, self.states1, self.potential_actions)
        res_s0, res_a, res_r, res_s1, res_pa = [], [], [], [], []
        for _ in range(size):
            res_s0.append(self.states0.pop())
            res_a.append(self.actions.pop())
            res_r.append(self.rewards.pop())
            res_s1.append(self.states1.pop())
            res_pa.append(self.potential_actions.pop())
        return replay_batch(res_s0, res_a, res_r, res_s1)
