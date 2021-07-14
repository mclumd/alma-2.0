import dgl
import torch.nn as nn
from dgl.nn import GraphConv
from dgl.nn import GatedGraphConv
import torch
import torch.nn.functional as F
import dgl.data
import numpy as np
import pickle


# Also boilerplate from dgl.ai

######################################################################
# Define Model
# ------------
#
# This tutorial will build a two-layer `Graph Convolutional Network
# (GCN) <http://tkipf.github.io/graph-convolutional-networks/>`__. Each of
# its layer computes new node representations by aggregating neighbor
# information. If you have gone through the
# :doc:`introduction <1_introduction>`, you will notice two
# differences:
#
# -  Since the task is to predict a single category for the *entire graph*
#    instead of for every node, you will need to aggregate the
#    representations of all the nodes and potentially the edges to form a
#    graph-level representation. Such process is more commonly referred as
#    a *readout*. A simple choice is to average the node features of a
#    graph with ``dgl.mean_nodes()``.
#
# -  The input graph to the model will be a batched graph yielded by the
#    ``GraphDataLoader``. The readout functions provided by DGL can handle
#    batched graphs so that they will return one representation for each
#    minibatch element.
#

# ********* #
# Basic GCN #
# ********* #

class GCN(nn.Module):
    def __init__(self, in_feats, h_feats, num_classes):
        # self.allow_zero_in_degree = True      # does not work as intended??? solved issue with dgl.add_self_loop(g) in dataset process
        super(GCN, self).__init__()
        self.conv1 = GraphConv(in_feats, h_feats)
        self.conv2 = GraphConv(h_feats, h_feats)
        self.conv3 = GraphConv(h_feats, h_feats)
        self.conv4 = GraphConv(h_feats, num_classes)

    def forward(self, g, in_feat):
        h = self.conv1(g, in_feat)
        h = F.relu(h)
        h = self.conv2(g, h)
        h = F.relu(h)
        h = self.conv3(g, h)
        h = F.relu(h)
        h = self.conv4(g, h)
        g.ndata['h'] = h
        return dgl.mean_nodes(g, 'h')

    def save(filename):
        torch.save(self, "gcndir/" + filename + ".pt")        

class GatedGCN(nn.Module):
    def __init__(self, in_feats, h_feats, num_classes):
        super(GatedGCN, self).__init__()
        self.conv1 = GatedGraphConv(in_feats, h_feats, 8, 1, bias=True)
        self.conv2 = GatedGraphConv(h_feats, num_classes, 8, 1, bias=True)

    def forward(self, g, in_feat):
        etypes = torch.tensor((), dtype=torch.int32)
        num_edges = g.num_edges()
        edges = np.zeros(num_edges)
        etypes = etypes.new_tensor(edges)

        h = self.conv1(g, in_feat, etypes)
        h = F.relu(h)
        h = self.conv2(g, h, etypes)
        g.ndata['h'] = h
        return dgl.mean_nodes(g, 'h')

# https://pytorch.org/tutorials/beginner/saving_loading_models.html
# pytorch comes with save/load!

def save_gnn_model(model, model_name):
    torch.save(model, "gcndir/" + model_name + ".pt")

def load_gnn_model(model_name):
    model = torch.load("gcndir/" + model_name + ".pt")
    # model.eval()
    return model
