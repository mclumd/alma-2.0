import dgl
import torch.nn as nn
from dgl.nn import GraphConv
import torch
import torch.nn.functional as F
import dgl.data
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
        self.conv2 = GraphConv(h_feats, num_classes)

    def forward(self, g, in_feat):
        h = self.conv1(g, in_feat)
        h = F.relu(h)
        h = self.conv2(g, h)
        g.ndata['h'] = h
        return dgl.mean_nodes(g, 'h')

# class ACN(nn.Module):
#     def __init__(self, in_feats, h_feats, num_classes):
#         super(GCN, self).__init__()


# https://pytorch.org/tutorials/beginner/saving_loading_models.html
# pytorch comes with save/load!

def save_gcn_model(model, model_name):
    torch.save(model, model_name + ".pt")

def load_gcn_model(model_name, in_feats, h_feats, num_classes):
    model = torch.load(model_name + ".pt")
    model.eval()




# *********************** #
# Pickling Still Untested #
# *********************** #

# def save_gnn_model(model, model_name):
#     pickle.dump(model, open(model_name + ".p", "wb"))
#
#
# def load_gnn_model(model_name):
#     model = pickle.load(open(model_name + ".p", "rb"))
#     return model
