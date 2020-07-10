from gym.envs.registration import register
from gym3 import ToGymEnv, ViewerWrapper, ExtractDictObWrapper
from .env import ENV_NAMES, ProcgenGym3Env


def make_env(render_mode=None, render=False, **kwargs):
    # the render option is kept here for backwards compatibility
    # users should use `render_mode="human"` or `render_mode="rgb_array"`
    if render:
        render_mode = "human"

    use_viewer_wrapper = False
    kwargs["render_mode"] = render_mode
    if render_mode == "human":
        # procgen does not directly support rendering a window
        # instead it's handled by gym3's ViewerWrapper
        # procgen only supports a render_mode of "rgb_array"
        use_viewer_wrapper = True
        kwargs["render_mode"] = "rgb_array"

    env = ProcgenGym3Env(num=1, num_threads=0, **kwargs)
    env = ExtractDictObWrapper(env, key="rgb")
    if use_viewer_wrapper:
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