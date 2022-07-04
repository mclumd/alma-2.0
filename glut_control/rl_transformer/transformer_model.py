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
as a context to the decoder.  The decoder is fed each individual action 


We will encode the knowledge base s_1, ..., s_n as a single long sequence   
   s_11, s_12, ...., s_1(k_1) <SEP> s_21, s_22, ...., s_2(k_2) <SEP> ... <SEP>s_n1, s_n2, ...., s_n(k_n) <EOS>
and use a BERT model to define the embedding.

For 

"""


