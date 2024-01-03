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

def train(env, max_num_episode:int, lr:float, crop:bool, random_policy: bool, policy: bool, save_frequency: int):
    
    episode_count = 0
    successful_episode = 0
    cumul_reward = 0
    reward_history = []
    if crop:
        N_features = 35*64*3
    else:
        N_features = 64*64*3
    
    game_type = env.options['env_name'].split('_')[1]

    if game_type == 'move':
        action1 = np.array([1])
        action2 = np.array([7])
        success_reward_val = 0
    
    elif game_type == 'attack':
        action1 = np.array([4])
        action2 = np.array([9])
        success_reward_val = 1
    

    T = int(env.options['env_name'].split('_')[-1])
    W = np.random.normal(loc=0, scale = 0.001, size= (N_features))
    W_saved = np.zeros((int(max_num_episode//save_frequency)+1, N_features))
    while episode_count <= max_num_episode:
        if episode_count % save_frequency == 0:
            W_saved[int(episode_count//save_frequency)]  = W
        reward, ob, first = env.observe()
        if first[0]:
            if episode_count!=0:
                cumul_reward += reward[0] == success_reward_val
                reward_history.append(cumul_reward.item())
            if reward[0] == success_reward_val and episode_count!=0: ## If the last episode was a success, do a weight update:
                u = np.mean(y*X, axis=1).T
                W += lr * u
                successful_episode += 1
                
            step = 0
            episode_count += 1
            y = np.zeros([T])
            X = np.zeros([N_features, T])
        
        if crop:
            state = ob['rgb'][:,:35,:,:].flatten()
        else:
            state = ob['rgb'].flatten()

        if random_policy:
            action = 2*(np.random.rand() > 0.5) - 1
        elif policy:
            s = sigmoid(np.dot(W,state)/len(state))
            action = 2*((np.random.rand()<s) - 1/2)
        else:
            action = np.sign(np.dot(W,state))
        if action == -1:
            env.act(action1) ## move left / no shoot
        elif action == +1 :
            env.act(action2) ## move right / shoot
        y[step] = action
        X[:,step] = state
        step += 1

    return cumul_reward, reward_history, W_saved

if __name__ == '__main__':
    
    parser = argparse.ArgumentParser(
        description="Run a training for primitive game"
    )

    parser.add_argument(
        "--logdir",
        type = str,
        help="Path to log the experiment"
    )

    parser.add_argument(
        "--env-name",
        type = str,
        choices=["bossfight_move_100", "bossfight_move_200", "bossfight_move_400", "bossfight_attack_100", "bossfight_attack_200", "bossfight_attack_400"],
        help="game environment type"
    )
    parser.add_argument(
        "--max-num-episode",
        type = int,
        help = 'number of training episodes'
    )
    
    parser.add_argument(
        "--lr",
        type = float,
        help = 'learning_rate'
    )
    parser.add_argument(
        "--save-frequency",
        type = int,
        help = 'Frequency of saving weights'
    )
    parser.add_argument(
        "--crop",
        action="store_true",
        default=False,
        help = 'Crop input environment rgb pixels or not'
    )
    parser.add_argument(
        "--random-policy",
        action="store_true",
        default=False,
        help = 'Use random policy while training or not'
    )
    parser.add_argument(
        "--policy",
        action="store_true",
        default=False,
        help = 'Use sigmoidal policy or not'
    )

    args = parser.parse_args()

    env = ProcgenGym3Env(num=1, env_name = args.env_name, use_backgrounds=False, restrict_themes=True)
    cumul_reward, reward_history, W_saved = train(env = env, max_num_episode=args.max_num_episode, lr=args.lr, crop=args.crop, random_policy=args.random_policy, policy = args.policy, save_frequency=args.save_frequency)

    args_dict = vars(parser.parse_args())

    log_time = datetime.now().strftime("%Y%m%d%H%M%S.%f")
    log_folder = os.path.join(args.logdir, log_time)
    if not os.path.isdir(log_folder):
        os.makedirs(log_folder)

    with open(os.path.join(log_folder, 'args_train.json'), 'w') as f:
        json.dump(args_dict, f)
    
    result_dict = {'cumul_reward': cumul_reward, 'reward_history': reward_history, 'W_saved': W_saved}
    jl.dump(result_dict, os.path.join(log_folder, 'result_train.jl'))