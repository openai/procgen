import numpy as np
import pytest
from .env import ENV_NAMES
from procgen import ProcgenEnv


@pytest.mark.parametrize("env_name", ["coinrun", "starpilot"])
def test_seeding(env_name):
    num_envs = 1

    def make_venv(level_num):
        venv = ProcgenEnv(
            num_envs=num_envs, env_name=env_name, num_levels=1, start_level=level_num
        )
        return venv

    venv1 = make_venv(0)
    venv2 = make_venv(0)
    venv3 = make_venv(1)

    venv1.reset()
    venv2.reset()
    venv3.reset()

    obs1, _, _, _ = venv1.step(np.zeros(num_envs))
    obs2, _, _, _ = venv2.step(np.zeros(num_envs))
    obs3, _, _, _ = venv3.step(np.zeros(num_envs))

    assert np.array_equal(obs1["rgb"], obs2["rgb"])
    assert not np.array_equal(obs1["rgb"], obs3["rgb"])


@pytest.mark.parametrize("env_name", ["coinrun", "starpilot"])
def test_determinism(env_name):
    def collect_observations():
        rng = np.random.RandomState(0)
        venv = ProcgenEnv(num_envs=2, env_name=env_name, rand_seed=23)
        obs = venv.reset()
        obses = [obs["rgb"]]
        for _ in range(128):
            obs, _rew, _done, _info = venv.step(
                rng.randint(
                    low=0,
                    high=venv.action_space.n,
                    size=(venv.num_envs,),
                    dtype=np.int32,
                )
            )
            obses.append(obs["rgb"])
        return np.array(obses)

    obs1 = collect_observations()
    obs2 = collect_observations()
    assert np.array_equal(obs1, obs2)


@pytest.mark.parametrize("env_name", ENV_NAMES)
@pytest.mark.parametrize("num_envs", [1, 2, 16])
def test_multi_speed(env_name, num_envs, benchmark):
    venv = ProcgenEnv(num_envs=num_envs, env_name=env_name)

    venv.reset()
    actions = np.zeros([venv.num_envs])

    def rollout(max_steps):
        step_count = 0
        while step_count < max_steps:
            _obs, _rews, _dones, _infos = venv.step(actions)
            step_count += 1

    benchmark(lambda: rollout(1000))

    venv.close()