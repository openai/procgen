from gym.envs.registration import register
from gym import ObservationWrapper
from .env import ENV_NAMES, ProcgenEnv
from .scalarize import Scalarize


class RemoveDictObs(ObservationWrapper):
    def __init__(self, env, key):
        self.key = key
        super().__init__(env=env)
        self.observation_space = env.observation_space.spaces[self.key]

    def observation(self, obs):
        return obs[self.key]


def make_env(**kwargs):
    venv = ProcgenEnv(num_envs=1, num_threads=0, **kwargs)
    env = Scalarize(venv)
    env = RemoveDictObs(env, key="rgb")
    return env


def register_environments():
    for env_name in ENV_NAMES:
        register(
            id=f'procgen-{env_name}-v0',
            entry_point='procgen.gym_registration:make_env',
            kwargs={"env_name": env_name},
        )