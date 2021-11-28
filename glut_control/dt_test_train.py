

import os, sys, argparse
import wandb
import pickle
import numpy as np
import torch
from transformers import GPT2Tokenizer
import dt_dataset
import tqdm

# Find decision_transformer
cwd = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(cwd, "decision-transformer/gym/"))

from decision_transformer.evaluation.evaluate_episodes import evaluate_episode, evaluate_episode_rtg
from decision_transformer.models.decision_transformer import DecisionTransformer
from decision_transformer.training.seq_trainer import SequenceTrainer




def train(variant, dataset="offline_datasets/dection_trans_data_qlearning2.1.pkl"):
    max_ep_len = 20
    env_targets = [3600, 1800]  # evaluation conditioning targets
    scale = 1000.  # normalization for rewards/returns

    tokenizer = GPT2Tokenizer.from_pretrained("gpt2")

    # The following was taken from a blog entry; not sure if it can be made to work
    # with the DecisionTransfofmer class of if it's actually necessary.  It *would* be
    # nice to limit the vocabulary size!
    special_tokens_dict = {'bos_token': '<BOS>', 'eos_token': '<EOS>',
                           'sep_token': '<SEP>'}
    num_added_toks = tokenizer.add_special_tokens(special_tokens_dict)
    #model.resize_token_embeddings(len(tokenizer))

    state_dim = 2048
    act_dim = 1024
    max_length= 128
    warmup_steps = variant['warmup_steps']
    with open(dataset, "rb") as dfile:
        D = pickle.load(dfile)
        data = dt_dataset.dt_dataset(D, state_dim=state_dim, act_dim=act_dim, max_ep_len=max_ep_len)

    model = DecisionTransformer(
            state_dim=state_dim,
            act_dim=act_dim,
            max_length=max_length,
            max_ep_len=max_ep_len,
            hidden_size=variant['embed_dim'],
            n_layer=variant['n_layer'],
            n_head=variant['n_head'],
            n_inner=4*variant['embed_dim'],
            activation_function=variant['activation_function'],
            n_positions=1024,
            resid_pdrop=variant['dropout'],
            attn_pdrop=variant['dropout'],
        )

    
    model = model.to(device=variant['device'])
    optimizer = torch.optim.AdamW(
        model.parameters(),
        lr=variant['learning_rate'],
        weight_decay=variant['weight_decay'],
    )
    scheduler = torch.optim.lr_scheduler.LambdaLR(
        optimizer,
        lambda steps: min((steps+1)/warmup_steps, 1)
    )


    trainer = SequenceTrainer(
            model=model,
            optimizer=optimizer,
            batch_size=variant['batch_size'],
            get_batch=data.get_batch,
            scheduler=scheduler,
            loss_fn=lambda s_hat, a_hat, r_hat, s, a, r: torch.mean((a_hat - a)**2),
            #eval_fns=[eval_episodes(tar) for tar in env_targets],
        )

    #if log_to_wandb:
    if False:   #investigate using this later
        wandb.init(
            name=exp_prefix,
            group=group_name,
            project='decision-transformer',
            config=variant
        )
        # wandb.watch(model)  # wandb has some bug

    for iter in tqdm.trange(variant['max_iters']):
        outputs = trainer.train_iteration(num_steps=variant['num_steps_per_iter'], iter_num=iter+1, print_logs=True)
        if log_to_wandb:
            wandb.log(outputs)

    torch.save({
            'epoch': 1,
            'model_state_dict': model.state_dict(),
            'optimizer_state_dict': optimizer.state_dict(),
            'loss': 0.4,
            }, "dt_checkpoint.pt")        


def test():
    pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--env', type=str, default='hopper')
    parser.add_argument('--dataset', type=str, default='medium')  # medium, medium-replay, medium-expert, expert
    parser.add_argument('--mode', type=str, default='normal')  # normal for standard setting, delayed for sparse
    parser.add_argument('--K', type=int, default=20)
    parser.add_argument('--pct_traj', type=float, default=1.)
    parser.add_argument('--batch_size', type=int, default=64)
    parser.add_argument('--model_type', type=str, default='dt')  # dt for decision transformer, bc for behavior cloning
    parser.add_argument('--embed_dim', type=int, default=128)
    parser.add_argument('--n_layer', type=int, default=3)
    parser.add_argument('--n_head', type=int, default=1)
    parser.add_argument('--activation_function', type=str, default='relu')
    parser.add_argument('--dropout', type=float, default=0.1)
    parser.add_argument('--learning_rate', '-lr', type=float, default=1e-4)
    parser.add_argument('--weight_decay', '-wd', type=float, default=1e-4)
    parser.add_argument('--warmup_steps', type=int, default=10000)
    parser.add_argument('--num_eval_episodes', type=int, default=100)
    parser.add_argument('--max_iters', type=int, default=10)
    parser.add_argument('--num_steps_per_iter', type=int, default=10000)
    parser.add_argument('--device', type=str, default='cuda')
    parser.add_argument('--log_to_wandb', '-w', type=bool, default=False)
    
    args = parser.parse_args()

    train(variant=vars(args))
