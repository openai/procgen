from gym.envs.registration import register
from gym3 import ToGymEnv, ViewerWrapper, ExtractDictObWrapper
from .env import ENV_NAMES, ProcgenGym3Env


def make_env(render=False, **kwargs):
    if render:
        kwargs["render_human"] = True
    env = ProcgenGym3Env(num=1, num_threads=0, **kwargs)
    env = ExtractDictObWrapper(env, key="rgb")
    if render:
        env = ViewerWrapper(env, tps=15, info_key="rgb")
    gym_env = ToGymEnv(env)
    return gym_env


def register_environments():
    for env_name in ENV_NAMES:
        register(
            id=f'procgen-{env_name}-v0',
            entry_point='procgen.gym_registration:make_env',
            kwargs={"env_name": env_name},
        )