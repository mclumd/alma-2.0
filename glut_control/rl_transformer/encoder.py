"""
Define, train and test the encoder of the transformer model.
This will basically be a BERT model that we train from scratch.
"""


import transformers
import numpy as np
import os
import pickle
import random

import torch
from tokenizers import ByteLevelBPETokenizer, BertWordPieceTokenizer, AddedToken
from tokenizers.processors import BertProcessing
from tokenizers.processors import RobertaProcessing
from torch.utils.data import Dataset
from transformers import BertConfig, BertForPreTraining,  Trainer, TrainingArguments, BertTokenizerFast, BertTokenizer
from transformers import RobertaConfig, RobertaForMaskedLM, RobertaTokenizer
from transformers import DataCollatorForLanguageModeling, TextDatasetForNextSentencePrediction, TextDataset
from transformers import pipeline

class QL4Dataset(Dataset):
    def __init__(self, src_files, tokenizer=None, train: bool = False, objective: str = None):
        if train:
            assert(objective in ["mlm", "nsp"])
        if tokenizer is None:
            self.tokenizer = RobertaTokenizer(
                "./simple_rl1-vocab.json",
                "./simple_rl1-merges.txt"
            )
        else:
            self.tokenizer = tokenizer
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
        truncated_lines, total_lines = 0, 0
        for src_file in src_files:
            print("🔥", src_file)
            lines = open(src_file, "rt", encoding="utf-8").readlines()
            for line in lines:
                line = line.strip("\n")
                if line == "":
                    continue
                else:
                    tokenized_line = self.tokenizer(line, max_length=510, truncation=True)
                    self.examples += [tokenized_line]
                    total_lines += 1
                    if len(tokenized_line) > 500:
                        print("🔥", line)
                        print("🔥", tokenized_line)
                        print("🔥", len(tokenized_line))
                        truncated_lines += 1
        print("Truncated {} lines out of {} total; or {} percent".format(truncated_lines, total_lines, truncated_lines*100/total_lines))


    def __len__(self):
        return len(self.examples)

    def __getitem__(self, i):
        # We’ll pad at the batch level.
        #return torch.tensor(self.examples[i])
        return self.examples[i]



def train_tokenizer(datafiles, roberta=False):
    if roberta:
        tokenizer = ByteLevelBPETokenizer()
        # Customize training
        tokenizer.train(files=datafiles, vocab_size=510, min_frequency=2, special_tokens=[
            "<s>", "<pad>", "</s>",
            "</kb>",
            "<unk>",
            "<mask>",
        ])
        tokenizer.save_model(".", "simple_rl1")
    else:
        tokenizer = BertWordPieceTokenizer(
            clean_text=True,
            strip_accents=True,
            lowercase=False,
        )
        tokenizer.train(
            datafiles,
            vocab_size=510,
            min_frequency=2,
            show_progress=True,
            special_tokens=[
                "<s>", "<pad>", "</s>",
                "</kb>",
                "<unk>",
                "<mask>"
            ],
            limit_alphabet=1000,
            wordpieces_prefix="##",
        )
        tokenizer.save_model(".", "bert_rl1")
        
    return tokenizer

def train_encoder(args):
    if os.path.exists("simple_rl1-vocab.json"):
        tokenizer = RobertaTokenizer(
            "./simple_rl1-vocab.json",
            "./simple_rl1-merges.txt"
        )
    else:
        tokenizer = train_tokenizer(args.datafile, args.roberta)
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

def preprocess_datafiles(input_file_list, output_file, val_file, val_threshold = 0.01):
    with open(val_file, "w") as vf:
        with open(output_file, "w") as of:
            for input_file in input_file_list:
                with open(input_file, "rb") as f:
                    print("Processing file: {}".format(input_file))
                    data = pickle.load(f)
                    for traj in data:
                        handle = vf if random.random() < val_threshold else of
                        #of.write("<cls>\n")
                        for kb, action in zip(traj['kb'], traj['actions']):
                            kb_form = "{}".format(".".join([
                                c for c in kb if "time" not in c and "now" not in c and "agentname" not in c]))
                            action_form = action[0] + ";" + action[1]
                            #form = kb_form + "</kb>" + action_form + "</sep>"
                            form = kb_form + "</kb>" + action_form
                            print("Found: ", form)
                            handle.write(form + "\n")
                        #of.write("</traj>\n")
                        handle.write("\n")
def train():
    import glob
    roberta = True
    
    src_files = glob.glob("/tmp/off*pkl")
    if not os.path.exists("/tmp/off_data_tds.txt"):
        preprocess_datafiles(src_files, "/tmp/off_data_tds.txt", "/tmp/off_data_tds_val.txt")
        tokenizer = train_tokenizer("/tmp/off_data_tds.txt", roberta)
    else:
        train_tokenizer("/tmp/off_data_tds.txt", roberta)

    if roberta:
        # SPECIAL_TOKENS = []
        # SPECIAL_TOKENS.append(AddedToken("<s>"))
        # SPECIAL_TOKENS.append(AddedToken("<pad>"))
        # SPECIAL_TOKENS.append(AddedToken("</s>"))
        # SPECIAL_TOKENS.append(AddedToken("</kb>"))

        # SPECIAL_TOKENS.append(AddedToken("<unk>"))
        # SPECIAL_TOKENS.append(AddedToken("<mask>", lstrip=False))

        tokenizer = RobertaTokenizer("./simple_rl1-vocab.json","./simple_rl1-merges.txt",model_max_length=512)

        #tokenizer = ByteLevelBPETokenizer("./simple_rl1-vocab.json","./simple_rl1-merges.txt")
        #tokenizer.add_special_tokens(SPECIAL_TOKENS)
        tokenizer.add_special_tokens( {
            'mask_token': '<mask>',
            'sep_token': '</s>',
            'pad_token': '<pad>',
            'cls_token': '<s>',
            'unk_token': '<unk>'
        })

        
        # tokenizer._tokenizer.post_processor = RobertaProcessing(
        #     sep=("</s>", tokenizer.token_to_id("</s>")),
        #     cls=("<s>", tokenizer.token_to_id("<s>")),
        # )
        # tokenizer.mask_token = "<mask>"
        # tokenizer.mask_token_id = tokenizer.token_to_id("<mask>")
        
        #tokenizer.enable_truncation(max_length=510)

        model_config = RobertaConfig(
            vocab_size=tokenizer.vocab_size,
            num_hidden_layers = 2, num_attention_heads=4
        )
        model = RobertaForMaskedLM(model_config)
        model.cuda()
        train_dataset = QL4Dataset(["/tmp/off_data_tds.txt"], tokenizer=tokenizer, train=True, objective="mlm")
        val_dataset = QL4Dataset(["/tmp/off_data_tds_val.txt"], tokenizer=tokenizer, train=False, objective="mlm")
    else:
        SPECIAL_TOKENS = []
        SPECIAL_TOKENS.append(AddedToken("<s>"))
        SPECIAL_TOKENS.append(AddedToken("<pad>"))
        SPECIAL_TOKENS.append(AddedToken("</s>"))
        SPECIAL_TOKENS.append(AddedToken("</kb>"))

        SPECIAL_TOKENS.append(AddedToken("<unk>"))
        SPECIAL_TOKENS.append(AddedToken("<mask>", lstrip=False))

        # Going to try working with BPE
        # tokenizer = BertTokenizer(
        #     vocab_file= "./bert_rl1-vocab.txt",
        #     do_lower_case = False,
        #     max_len=512
        # )

        tokenizer = ByteLevelBPETokenizer("./simple_rl1-vocab.json","./simple_rl1-merges.txt")
        tokenizer.add_special_tokens(SPECIAL_TOKENS)
        tokenizer._tokenizer.post_processor = RobertaProcessing(
            sep=("</s>", tokenizer.token_to_id("</s>")),
            cls=("<s>", tokenizer.token_to_id("<s>")),
        )
        model_config = BertConfig(
            vocab_size=tokenizer.get_vocab_size(True),
            num_hidden_layers = 2, num_attention_heads=4
        )
        print("model config", model_config)
        model = BertForPreTraining(model_config)
        print(model)
        model.cuda()

        #train_dataset = TextDatasetForNextSentencePrediction(tokenizer=tokenizer, file_path="/tmp/off_data_tds.txt", block_size=512)
        #val_dataset = TextDatasetForNextSentencePrediction(tokenizer=tokenizer, file_path="/tmp/off_data_tds_val.txt", block_size=512)
        train_dataset = TextDataset(tokenizer=tokenizer, file_path="/tmp/off_data_tds.txt",
                                                             block_size=512)
        val_dataset = TextDataset(tokenizer=tokenizer, file_path="/tmp/off_data_tds_val.txt",
                                                           block_size=512)

    #mlm_dataset = TextDatasetForMaskedLM(tokenizer=rtokenizer, file_path="/tmp/off_data_tds.txt", block_size=512)
    #train_dataset = QL4Dataset(["/tmp/off_data.txt"], train=True, objective="mlm")
    print("Train dataset set size: ", len(train_dataset))
    data_collator = DataCollatorForLanguageModeling(tokenizer=tokenizer,
                                                    mlm=True, mlm_probability=0.15,)
    training_args = TrainingArguments(
        output_dir='./results',          # output directory
        overwrite_output_dir=True,
        num_train_epochs=10,              # total number of training epochs
        per_device_train_batch_size=16,  # batch size per device during training
        per_device_eval_batch_size=16,   # batch size for evaluation  (was 64)
        warmup_steps=500,                # number of warmup steps for learning rate scheduler
        weight_decay=0.01,               # strength of weight decay
        logging_dir='./logs',            # directory for storing logs
        logging_steps=500,
        save_total_limit=2,
        prediction_loss_only=True,
        evaluation_strategy="steps",
        eval_steps=100
    )


    trainer = Trainer(
        model=model,                         # the instantiated 🤗 Transformers model to be trained
        args=training_args,                  # training arguments, defined above
        train_dataset=train_dataset,
        data_collator=data_collator,
        # ,         # training dataset
        eval_dataset=val_dataset,             # evaluation dataset
    )

    trainer.train()
    # trainer.save_model("rltrans1")
    # if roberta:
    #     rmodel = RobertaForMaskedLM.from_pretrained("./results/checkpoint-27000")
    #     tokenizer = RobertaTokenizer("./simple_rl1-vocab.json", "./simple_rl1-merges.txt")
    # else:
    #     pass

def test_encoding():
    tokenizer = RobertaTokenizer("./simple_rl1-vocab.json","./simple_rl1-merges.txt",model_max_length=512)
    rmodel = RobertaForMaskedLM.from_pretrained("./results/checkpoint-1634000")
    tokenizer.encode("<mask>")
if __name__ == "__main__":
    train()
    #test_encoding()

