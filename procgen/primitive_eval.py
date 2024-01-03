import argparse 
from datetime import datetime
import os
import json
import numpy as np
import joblib as jl

from procgen import ProcgenGym3Env

def sigmoid(x):
    if x<0:
        return np.exp(x)/(1+np.exp(x))
    else:
        return 1. /(1+np.exp(-x))
    
def eval(env, W:np.ndarray, num_eval_episode:int, crop:bool, policy:bool):
    
    game_type = env.options['env_name'].split('_')[1]

    if game_type == 'move':
        action1 = np.array([1])
        action2 = np.array([7])
        success_reward_val = 0
    
    elif game_type == 'attack':
        action1 = np.array([4])
        action2 = np.array([9])
        success_reward_val = 1

    episode_count = 0
    successful_episode = 1

    while episode_count <= num_eval_episode:
        reward, ob, first = env.observe()
        if first[0]:
            if reward[0] == success_reward_val and episode_count!=0: ## If the last episode was a success, do a weight update:
                successful_episode += 1

            episode_count += 1
        
        if crop:
            state = ob['rgb'][:,:35,:,:].flatten()
        else:
            state = ob['rgb'].flatten()

        if policy:
            s = sigmoid(np.dot(W,state)/len(state))
            action = 2*((np.random.rand()<s) - 1/2)
        else:
            action = np.sign(np.dot(W,state))
        if action == -1:
            env.act(action1) ## move left / no shoot
        elif action == +1 :
            env.act(action2) ## move right / shoot

    return successful_episode

if __name__ == '__main__':
    
    parser = argparse.ArgumentParser(
        description="Run a training for primitive game"
    )

    parser.add_argument(
        "--logpath",
        type = str,
        help="Path of directory where logs of the experiments are saved"
    )

    parser.add_argument(
        "--num-eval-episode",
        type = int,
        help="Number of episodes runs to evaluate"
    )


    args = parser.parse_args()

    args_path = os.path.join(args.logpath, 'args_train.json')
    result_path = os.path.join(args.logpath, 'result_train.jl')

    assert os.path.exists(args_path) and os.path.exists(result_path)

    with open(args_path, 'r') as f:
        train_args_dict=json.load(f)
    
    result_dict=jl.load(result_path)

    W_saved = result_dict['W_saved']

    history = []

    for W in W_saved:
        env = ProcgenGym3Env(num=1, env_name = train_args_dict['env_name'], use_backgrounds=False, restrict_themes=True)
        successful_episode=eval(env=env, W=W, num_eval_episode=args.num_eval_episode, crop=train_args_dict['crop'], policy = train_args_dict['policy'])
        history.append(successful_episode)
    
    eval_args_dict = vars(parser.parse_args())


    with open(os.path.join(args.logpath, 'args_eval.json'), 'w') as f:
        json.dump(eval_args_dict, f)
    
    eval_result_dict = {'history': history}
    jl.dump(eval_result_dict, os.path.join(args.logpath, 'result_eval.jl'))