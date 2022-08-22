import os
#os.environ["CUDA_VISIBLE_DEVICES"] = "-1"


import transformers
import numpy as np


import pickle
import random

import torch
import torch.nn as nn
from tokenizers import ByteLevelBPETokenizer, BertWordPieceTokenizer, AddedToken
from tokenizers.processors import BertProcessing
from tokenizers.processors import RobertaProcessing
from torch.utils.data import Dataset
from transformers import BertConfig, BertForPreTraining,  Trainer, TrainingArguments, BertTokenizerFast, BertTokenizer
from transformers import RobertaConfig, RobertaForMaskedLM, RobertaTokenizer
from transformers import DataCollatorForLanguageModeling, TextDatasetForNextSentencePrediction, TextDataset
from transformers import pipeline


class myBertPooler(nn.Module):
    def __init__(self, hidden_size):
        super().__init__()
        self.dense = nn.Linear(hidden_size, hidden_size)
        self.activation = nn.Tanh()

    def forward(self, hidden_states):
        # We "pool" the model by simply taking the hidden state corresponding
        # to the first token.
        first_token_tensor = hidden_states[:, 0]
        pooled_output = self.dense(first_token_tensor)
        pooled_output = self.activation(pooled_output)
        return pooled_output

def load_base(base_model_dir: str, tokenizer_prefix: str) -> tuple:
    # Load and decapitate base model
    rmodel = RobertaForMaskedLM.from_pretrained(base_model_dir)
    rmodel = rmodel.roberta # Don't want the LM head


    # Load tokenizer
    tokenizer = RobertaTokenizer(tokenizer_prefix+"-vocab.json",
                                 tokenizer_prefix+"-merges.txt",
                                 model_max_length=512)

    tokenizer.add_special_tokens( {
        'mask_token': '<mask>',
        'sep_token': '</s>',
        'pad_token': '<pad>',
        'cls_token': '<s>',
        'unk_token': '<unk>'
    })
    return rmodel, tokenizer

class trans_dqn(nn.Module):
    """  This will essentialy wrap the BERT model with a head that outputs a Q-value """

    
    def __init__(self, debugging=False,base_model="rl_transformer/base_model/2hidden_layers_4attention_heads/checkpoint-2400000", tokenizer_prefix="rl_transformer/simple_rl1", hidden_size=512, device="cpu", max_reward=1.0):
        super().__init__()
        self.max_reward = max_reward
        self.debugging = debugging
        self.base_model_dir = base_model
        self.tokenizer_prefix = tokenizer_prefix
        self.hidden_size=hidden_size
        self.device = device
        self.base_model, self.tokenizer = load_base(base_model, tokenizer_prefix)
        self.base_model.to(device)
        self.base_model.train()
        last_layer = self.base_model.encoder.layer[-1]
        base_output_size = last_layer.output.dense.out_features

        self.pooler = myBertPooler(base_output_size)

        self.qhead0 = torch.nn.Linear(in_features = base_output_size,
                                      out_features = hidden_size)
        self.activation0 = torch.nn.ReLU()
        self.LayerNorm0 = torch.nn.LayerNorm(hidden_size)
        self.qhead1 = torch.nn.Linear(in_features = hidden_size,
                                      out_features = hidden_size)
        self.activation1 = torch.nn.ReLU()
        self.LayerNorm1 = torch.nn.LayerNorm(hidden_size)
        self.qvalue = torch.nn.Linear(in_features = hidden_size,
                                      out_features = 1)
        self.q_activation = torch.nn.Sigmoid()

        self.head_layers = [self.pooler,
                            self.qhead0, self.activation0, self.LayerNorm0,
                            self.qhead1, self.activation1, self.LayerNorm1,
                            self.qvalue, self.q_activation]
                            

    def trainable_parameters(self, add_pooler=True):
        """ Return paramters not from base model """
        res = list(self.pooler.parameters()) if add_pooler else []
        
        for layer in self.head_layers:
            res += list(layer.parameters())
        
        return res

    def train(self):
        for p in self.trainable_parameters():
            p.requires_grad = True

    def forward(self, inp):
        base_out = self.base_model(**self.tokenizer(inp, return_tensors="pt", padding=True, truncation=True).to(self.device))
        x0 = self.pooler(base_out.last_hidden_state)
        y0 = self.activation0(self.qhead0(x0))
        l0 = self.LayerNorm0(y0)
        y1 = self.activation1(self.qhead1(l0))
        l1 = self.LayerNorm1(y1)
        q = self.q_activation(self.qvalue(l1))
        return self.max_reward*q
    
    def save(self, folder, id_str):
        #self.model.save("rl_model_{}".format(id_str))
        torch.save(self.state_dict(), os.path.join(folder, "rl_model_{}".format(id_str)))

    def load(self, folder, id_str):
        #self.model.load_model("rl_model_{}".format(id_str))
        self.load_state_dict(torch.load(os.path.join(folder, "rl_model_{}".format(id_str))))



