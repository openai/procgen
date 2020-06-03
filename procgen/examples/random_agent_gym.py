"""
Example random agent script using the gym API to demonstrate that procgen works
"""

import gym
env = gym.make('procgen:procgen-coinrun-v0')
obs = env.reset()
step = 0
while True:
    obs, rew, done, info = env.step(env.action_space.sample())
    print(f"step {step} reward {rew} done {done}")
    step += 1
    if done:
        break