"""
Define, train and test the encoder of the transformer model.
This will basically be a BERT model that we train from scratch.
"""


import transformers
import numpy as np
import os
import pickle

import torch
from tokenizers import ByteLevelBPETokenizer
from tokenizers.processors import BertProcessing
from tokenizers.processors import RobertaProcessing
from torch.utils.data import Dataset
from transformers import BertConfig, BertForPreTraining,  Trainer, TrainingArguments
from transformers import RobertaConfig, RobertaForMaskedLM, RobertaTokenizer
from transformers import DataCollatorForLanguageModeling
from transformers import pipeline

class QL4Dataset(Dataset):
    def __init__(self, src_files, train: bool = False, objective: str = None):
        if train:
            assert(objective in ["mlm", "nsp"])
        self.tokenizer = RobertaTokenizer(
            "./simple_rl1-vocab.json",
            "./simple_rl1-merges.txt"
        )
        if objective == "nsp":
            self.get_nsp_examples(src_files)
        else:
            self.get_mlm_examples(src_files)



    def get_nsp_examples(self, src_files):
        self.examples = []
        for src_file in src_files:
            print("🔥", src_file)
            lines = src_file.read_text(encoding="utf-8").splitlines()
            for line in lines:
                if "<traj>" in line:
                    # New trajectory
                    kb_num = 0
                elif "</traj>" in line:
                    # End of trajectory
                    pass
                else:
                    assert( line[:4] == "<kb>" and line[-4:] == "</kb>")
                    kb = line[4:-4]
                    if kb_num == 0:
                        kb0 = kb
                    else:
                        kb1 = kb
                        self.examples += [self.tokenize_sp(kb0, kb1)]
                        kb0 = kb1[:]

    def tokenize_sp(self, kb0, kb1):
        return self.tokenizer.encode(kb0) + self.tokenizer.encode(kb1)  # This will need some tweaking

    def get_mlm_examples(self, src_files):
        self.examples = []
        for src_file in src_files:
            print("🔥", src_file)
            lines = open(src_file, "rt", encoding="utf-8").readlines()
            for line in lines:
                line = line.strip("\n")
                if "traj" in line:
                    continue
                else:
                    assert( line[:4] == "<kb>" and line[-5:] == "</kb>")
                    kb = line[4:-5][:510]
                    self.examples += [self.tokenizer(kb)]

    def __len__(self):
        return len(self.examples)

    def __getitem__(self, i):
        # We’ll pad at the batch level.
        #return torch.tensor(self.examples[i])
        return self.examples[i]



def train_tokenizer(datafiles):
    tokenizer = ByteLevelBPETokenizer()
    # Customize training
    tokenizer.train(files=datafiles, vocab_size=52_000, min_frequency=2, special_tokens=[
        "<s>", "</s>",
        "<kb>", "</kb>",
        "<traj>", "</traj>",
        "<unk>",
        "<mask>",
    ])
    tokenizer.save_model(".", "simple_rl1")
    return tokenizer

def train_encoder(args):
    if os.path.exists("simple_rl1-vocab.json"):
        tokenizer = RobertaTokenizer(
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

def test_string(string, model, tokenizer):
    from scipy.special import softmax
    ids=tokenizer.encode_plus(string)
    
    output = model(torch.tensor(ids['input_ids']).unsqueeze(0))
    logits = output['logits']
    nlogits = logits.detach().numpy()
    slogits = softmax(nlogits, axis=1)[0]
    L = np.argmax(slogits, axis=1)
    print("L=", L)
    return tokenizer.decode(L)



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
    if not os.path.exists("/tmp/off_data.txt"):
        preprocess_datafiles(src_files, "/tmp/off_data.txt")
        tokenizer = train_tokenizer("/tmp/off_data.txt")
    #train_encoder(["/tmp/off_data.txt"])
    train_dataset = QL4Dataset(["/tmp/off_data.txt"], train=True, objective="mlm")
    print("Train set size: ", len(train_dataset))
    data_collator = DataCollatorForLanguageModeling(tokenizer=train_dataset.tokenizer,
                                                    mlm=True, mlm_probability=0.15)
    

    model_config = RobertaConfig(
      vocab_size=train_dataset.tokenizer.vocab_size
    )
    model = RobertaForMaskedLM(model_config)
    model.cuda()

    training_args = TrainingArguments(
      output_dir='./results',          # output directory
      num_train_epochs=10,              # total number of training epochs
      per_device_train_batch_size=16,  # batch size per device during training
      per_device_eval_batch_size=64,   # batch size for evaluation
      warmup_steps=500,                # number of warmup steps for learning rate scheduler
      weight_decay=0.01,               # strength of weight decay
      logging_dir='./logs',            # directory for storing logs
      logging_steps=10,
    )


    trainer = Trainer(
        model=model,                         # the instantiated 🤗 Transformers model to be trained
        args=training_args,                  # training arguments, defined above
        train_dataset=train_dataset,
        data_collator=data_collator
        # ,         # training dataset
        #  eval_dataset=val_dataset             # evaluation dataset
    )

    trainer.train()
    rmodel = RobertaForMaskedLM.from_pretrained("results/checkpoint-5000")

if __name__ == "__main__":
    main()
