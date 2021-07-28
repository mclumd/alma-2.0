import dgl
import torch.nn as nn
from dgl.nn import GraphConv
from dgl.nn import GatedGraphConv
from dgl.nn.pytorch import GATConv

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

    def readout(self, g, in_feat):
        h = self.conv1(g, in_feat)
        h = F.relu(h)
        h = self.conv2(g, h)
        h = F.relu(h)
        h = self.conv3(g, h)
        g.ndata['r'] = h
        return dgl.mean_nodes(g, 'r')
        

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


class MultiHeadGATLayer(nn.Module):
    def __init__(self, g, in_dim, out_dim, num_heads, merge='cat'):
        super(MultiHeadGATLayer, self).__init__()
        self.heads = nn.ModuleList()
        for i in range(num_heads):
            self.heads.append(GATLayer(g, in_dim, out_dim))
        self.merge = merge

    def forward(self, h):
        head_outs = [attn_head(h) for attn_head in self.heads]
        if self.merge == 'cat':
            # concat on the output feature dimension (dim=1)
            return torch.cat(head_outs, dim=1)
        else:
            # merge using average
            return torch.mean(torch.stack(head_outs))  

class simpleGAT(nn.Module):
    def  __init__(self, in_feats, h_feats, num_classes, out_dim, num_heads):
        super().__init__()
        #self.layer1 = MultiHeadGATLayer(in_dim, hidden_dim, num_heads)
        # Be aware that the input dimension is hidden_dim*num_heads since
        # multiple head outputs are concatenated together. Also, only
        # one attention head in the output layer.
        #self.layer2 = MultiHeadGATLayer(hidden_dim * num_heads, out_dim, 1)
        self.layer1 = GATConv(in_feats, h_feats, num_heads)
        #self.layer2 = GATConv(num_heads*h_feats, h_feats, num_heads)
        self.layer3 = nn.Linear(h_feats*num_heads, num_classes)
        

    def forward(self, g, feat):
        h = self.layer1(g, feat)
        h = F.elu(h)
        #h = self.layer2(g, h)
        #h = F.elu(h)
        h = self.layer3(h)
        return h

    def readout(self,h):
        h = self.layer1(h)
        h = F.elu(h)
        #h = self.layer2(h)
        #h = F.elu(h)
        return h
# https://pytorch.org/tutorials/beginner/saving_loading_models.html
# pytorch comes with save/load!

def save_gnn_model(model, model_name):
    torch.save(model, "gcndir/" + model_name + ".pt")

def load_gnn_model(model_name):
    model = torch.load("gcndir/" + model_name + ".pt")
    # model.eval()
    return model
