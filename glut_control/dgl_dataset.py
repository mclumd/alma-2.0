######################################################################
# ``DGLDataset`` Object Overview
# ------------------------------
#
# Your custom graph dataset should inherit the ``dgl.data.DGLDataset``
# class and implement the following methods:
#
# -  ``__getitem__(self, i)``: retrieve the ``i``-th example of the
#    dataset. An example often contains a single DGL graph, and
#    occasionally its label.
# -  ``__len__(self)``: the number of examples in the dataset.
# -  ``process(self)``: load and process raw data from disk.
#

import pandas as pd

import dgl
from dgl.data import DGLDataset
import torch
import numpy as np
import os

# Constructor takes an Xbuffer and ybuffer from res_prebuffer object (having saved batch(es))
# Built and tested with output from save_batch when use_gnn = True and use_tf = False
# save_batch calls vectorize, this built and tested on data from the vectorize_alg = gnn1 mode

class AlmaDataset(DGLDataset):
    def __init__(self, Xbuffer, ybuffer):
        self.X = Xbuffer
        self.Y = ybuffer
        self.dim_nfeats = len(self.X[0][1][0])
        self.gclasses = len(np.unique(self.Y)) # untested, but should work!

        # self.dim_nfeats = 11    # hardcoded for now, can work on this later
        # self.gclasses = 2       # hardcoded for now, can work on this later

        # The overridden process function is automatically called after construction
        # https://docs.dgl.ai/api/python/dgl.data.html
        super().__init__(name='synthetic')

    def process(self):
        self.graphs = []
        self.labels = []
        src = np.array([1])     # A placeholder for the array before we begin appending edge information
        dst = np.array([1])     # It will be sliced off before passing to the dgl.graph constructor, maybe there is a better way to do this? Don't know numpy that well
        label = None
        num_nodes = -1

        # Iterate over list of graphs
        # For each adjacency, store a bidirectional edge between lowercase x and y,                                                         # Potential different architectures include no backwards edges,
        # this ensures no node is isolated from message passing                                                                             # a standard edge feature to identify backwards edges, and a
        # Then store the graph label from y                                                                                                 # new "supernode" that has bidirectional edges connecting to
        # Last, grab num_nodes from size of adjacency matrix                                                                                # and from each existing node. 1 and 3 are common standards,
        i = 0                                                                                                                               # 2 is not but could be helpful nonetheless.
        for graph in self.X:
            y = 0
            for row in graph[0]:
                x = 0
                for column in row:
                    if column == 1.0:
                        src = np.append(src, [x])
                        src = np.append(src, [y])
                        dst = np.append(dst, [y])
                        dst = np.append(dst, [x])
                    x += 1
                y += 1
            label = self.Y[i]
            num_nodes = len(graph[0][0])
            # Create a graph and add it to the list of graphs and labels.
            g = dgl.graph((src[1:], dst[1:]), num_nodes=num_nodes)
            # Grab features for g from X[i][1]
            # Convert to tensor via torch to avoid the "numpy.ndarray has no attribute 'device'" error
            # https://discuss.dgl.ai/t/attributeerror-numpy-ndarray-object-has-no-attribute-device/241
            g.ndata['feat'] = torch.LongTensor(graph[1])
            self.graphs.append(dgl.add_self_loop(g))        #stops the zero-in-degree issue with GCN
            self.labels.append(label)
            i += 1

        # Convert the label list to tensor for saving.
        self.labels = torch.LongTensor(self.labels)


    def __getitem__(self, i):
        return self.graphs[i], self.labels[i]

    def __len__(self):
        return len(self.graphs)


# Constructor takes a list of AlmaDatasets and mashes them together into one big dataset
# Probably don't need this anymore, but not bad to have on hand

class BigAlmaDataset(DGLDataset):
    def __init__(self, data_list):
        self.data_list = data_list
        self.dim_nfeats = data_list[0].dim_nfeats
        self.gclasses = data_list[0].gclasses
        super().__init__(name='synthetic')

    def process(self):
        self.graphs = []
        self.labels = []
        for set in self.data_list:
            for i in range(len(set)):
                g, l = set[i]
                self.graphs.append(g)
                self.labels.append(l)
        self.labels = torch.LongTensor(self.labels)


    def __getitem__(self, i):
        return self.graphs[i], self.labels[i]

    def __len__(self):
        return len(self.graphs)