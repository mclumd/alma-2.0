"""
Define, train and test the encoder of the transformer model.
This will basically be a BERT model that we train from scratch.
"""


import transformers
import numpy as np
import os
import pickle

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

def preprocess_datafiles(input_file_list, output_file):
    with open(output_file, "w") as of:
        for input_file in input_file_list:
            with open(input_file, "rb") as f:
                print("Processing file: {}".format(input_file))
                data = pickle.load(f)
                for traj in data:
                    for kb in traj['kb']:
                        form = "<kb> {} </kb>".format("<sep>".join(kb))
                        print("Found: ", form)
                        of.write(form + "\n")

import glob
preprocess_datafiles(glob.glob("/tmp/off*pkl"), "/tmp/off_data.txt")

