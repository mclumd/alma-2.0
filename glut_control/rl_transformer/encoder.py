"""
Define, train and test the encoder of the transformer model.
This will basically be a BERT model that we train from scratch.
"""


import transformers
import numpy as np
import os
import pickle

from tokenizers import ByteLevelBPETokenizer
from tokenizers.processors import BertProcessing
from torch.utils.data import Dataset

class QL4Dataset(Dataset):
    def __init__(self, src_files, evaluate: bool = False):
        tokenizer = ByteLevelBPETokenizer(
            "./simple_rl1-vocab.json",
            "./simple_rl1-merges.txt"
        )
        tokenizer._tokenizer.post_processor = BertProcessing(
            ("</traj>", tokenizer.token_to_id("</traj>")),
            ("<traj>", tokenizer.token_to_id("<traj>")),
        )
        tokenizer.enable_truncation(max_length=512)
        # or use the RobertaTokenizer from `transformers` directly.

        self.examples = []

        for src_file in src_files:
            print("🔥", src_file)
            lines = src_file.read_text(encoding="utf-8").splitlines()
            self.examples += [x.ids for x in tokenizer.encode_batch(lines)]

    def __len__(self):
        return len(self.examples)

    def __getitem__(self, i):
        # We’ll pad at the batch level.
        return torch.tensor(self.examples[i])



def train_tokenizer(datafiles):
    tokenizer = ByteLevelBPETokenizer()
    # Customize training
    tokenizer.train(files=datafiles, vocab_size=52_000, min_frequency=2, special_tokens=[
        "<sep>",
        "<kb>", "</kb>",
        "<traj>", "</traj>",
        "<unk>",
        "<mask>",
    ])
    tokenizer.save_model(".",  "simple_rl1")
    return tokenizer

def train_encoder(args):
    if os.path.exists("simple_rl1-vocab.json"):
        tokenizer = ByteLevelBPETokenizer(
            "./simple_rl1-vocab.json",
            "./simple_rl1-merges.txt"
        )
    else:
        tokenizer = train_tokenizer(args.datafile)
    tokenizer._tokenizer.post_processor = BertProcessing(
        ("</s>", tokenizer.token_to_id("</s>")),
        ("<s>", tokenizer.token_to_id("<s>")),
    )
    tokenizer.enable_truncation(max_length=512)




def preprocess_datafiles(input_file_list, output_file):
    with open(output_file, "w") as of:
        for input_file in input_file_list:
            with open(input_file, "rb") as f:
                print("Processing file: {}".format(input_file))
                data = pickle.load(f)
                for traj in data:
                    of.write("<traj>\n")
                    for kb in traj['kb']:
                        form = "<kb>{}</kb>".format("<sep>".join([
                            c for c in kb if "time" not in c and "now" not in c and "agentname" not in c]))
                        print("Found: ", form)
                        of.write(form + "\n")
                    of.write("</traj>\n")

def main():
    import glob
    src_files = glob.glob("/tmp/off*pkl")
    preprocess_datafiles(src_files, "/tmp/off_data.txt")
    tokenizer = train_tokenizer("/tmp/off_data.txt")
    #train_encoder(["/tmp/off_data.txt"])
    train_set = QL4Dataset(["/tmp/off_data.txt"])
    

if __name__ == "__main__":
    main()
