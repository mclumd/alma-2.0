from resolution_prebuffer import res_prebuffer, history_struct
import dgl_network, dgl_dataset
import torch
import torch.nn as nn
import torch.nn.functional as F
import dgl.data
import numpy as np
from torch.utils.data.sampler import SubsetRandomSampler, SequentialSampler
from dgl.dataloading import GraphDataLoader

class dgl_heuristic(res_prebuffer):
    def __init__(self, input_size, pi_dataset=False, gat_network=False, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.model = dgl_network.simpleGAT(input_size, 512, 2, 16, 1) if gat_network else dgl_network.GCN(input_size, 512, 2)
        self.optimizer = torch.optim.Adam(self.model.parameters(), lr=0.01)
        self.dgl_gnn = True
        self.pi_dataset = pi_dataset
        self.gat_network = gat_network
        


    def train_buffered_batch(self, sample_ratio=0.5, given_batch=None):
        self.model.train()
        if given_batch is None:
            X, y, dbg = self.get_training_batch(self.batch_size, int(self.batch_size*sample_ratio))
        else:
            xp, xn = given_batch['posig'], given_batch['negig']
            X = xp + xn
            y = [0]*len(xp) + [1]*len(xn)
            
        
        dataset = dgl_dataset.PotentialInferenceDataSet(X, y)  if self.pi_dataset else dgl_dataset.AlmaDataset(X, y)

        # X = temp_network.vectorize(res_task_input)
        # Y = np.zeros(len(X)) # filler data, labels don't matter
        # dataset = dgl_dataset.AlmaDataset(X, Y)

        num_examples = len(dataset)
        #num_train = int(num_examples * 0.8)
        num_train = num_examples

        train_sampler = SequentialSampler(torch.arange(num_train))
        test_sampler = SequentialSampler(torch.arange(num_train, num_examples))
        train_dataloader = GraphDataLoader(
            dataset, sampler=train_sampler, batch_size=self.batch_size, drop_last=False)
        for batched_graph, labels in train_dataloader:
            # print("Batched Graph ", i)
            #i += 1
            # if i > 250:
            #     break
            pred = self.model(batched_graph, batched_graph.ndata['feat'].float() ) if self.gat_network else self.model(batched_graph, batched_graph.ndata['feat'].float())
            pred = torch.softmax(pred,1)
            tensor = torch.tensor((), dtype=torch.float32)
            mse_labels = tensor.new_zeros((len(labels), 2))
            for j in range(batched_graph.batch_size):
                mse_labels[j][1] = labels[j]
                if labels[j] == 1:
                    mse_labels[j][0] = 0
                else:
                    mse_labels[j][0] = 1
            #loss = F.mse_loss(pred, mse_labels)
            loss = F.cross_entropy(pred, labels)
            self.optimizer.zero_grad()
            # tloss = loss
            loss.backward()
            self.optimizer.step()
            print('loss: %.3f' % loss.item())
        return history_struct({'accuracy': [np.nan], 'loss': [loss.item()]})

    def get_priorities(self, inputs, already_vectorized=False):
        X = inputs if already_vectorized else self.vectorize(inputs)
        Y = np.zeros(len(X)) # filler data, labels don't matter
        dataset = dgl_dataset.PotentialInferenceDataSet(X, y)  if self.pi_dataset else dgl_dataset.AlmaDataset(X, y)
        sampler = SequentialSampler(torch.arange(len(X)))
        #sampler = SequentialSampler(torch.arange(1))
        test_dataloader = GraphDataLoader(
            dataset, sampler=sampler, batch_size=1, drop_last=False)
#            dataset, sampler=sampler, batch_size=len(X), drop_last=False)
        preds = []
        i=0
        self.model.eval()
        for batched_graph, labels in test_dataloader:
            print(i, '/', len(X))
            i+=1
            preds.append(torch.softmax(self.model(batched_graph, batched_graph.ndata['feat'].float()),1).detach().cpu().numpy())
        print('done')
        self.model.train()
        return np.array(preds).reshape(-1,2)[:,1]
