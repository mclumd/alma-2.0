"""
Define, train and test the encoder of the transformer model.
This will basically be a BERT model that we train from scratch.
"""


import transformers
import numpy as np
import os

from tokenizers import ByteLevelBPETokenizer



def train_tokenizer(datafile):
    tokenizer = ByteLevelBPETokenizer()
    # Customize training
    tokenizer.train(files=datafile, vocab_size=52_000, min_frequency=2, special_tokens=[
        "<s>",
        "<pad>",
        "</s>",
        "<unk>",
        "<mask>",
    ])
    return tokenizer

def train_encoder(args):
    if os.path.exists("simple_rl1"):
        tokenizer.load_model(".", "simple_rl1")
    else:
        train_tokenizer(args.datafile)    
        # Save files to disk
        tokenizer.save_model(".", "simple_rl1")

