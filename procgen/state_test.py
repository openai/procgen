import numpy as np
import pytest
from procgen import ProcgenGym3Env
from .env import ENV_NAMES
import gym3
import multiprocessing as mp


NUM_STEPS = 10000


def gather_rollouts(
    env_kwargs, actions, state=None, get_state=False, set_state_every_step=False
):
    env = ProcgenGym3Env(**env_kwargs)
    if state is not None:
        env.callmethod("set_state", state)
    result = [dict(ob=env.observe(), info=env.get_info())]
    if get_state:
        result[-1]["state"] = env.callmethod("get_state")
    if set_state_every_step:
        env.callmethod("set_state", result[-1]["state"])
    for act in actions:
        env.act(act)
        result.append(dict(ob=env.observe(), info=env.get_info()))
        if get_state:
            result[-1]["state"] = env.callmethod("get_state")
        if set_state_every_step:
            env.callmethod("set_state", result[-1]["state"])
    return result


def fn_wrapper(fn, result_queue, **kwargs):
    result = fn(**kwargs)
    result_queue.put(result)


def run_in_subproc(fn, **kwargs):
    ctx = mp.get_context("spawn")
    result_queue = ctx.Queue()
    p = ctx.Process(
        target=fn_wrapper, kwargs=dict(fn=fn, result_queue=result_queue, **kwargs)
    )
    p.start()
    result = result_queue.get()
    p.join()
    return result


def assert_rollouts_identical(a_rollout, b_rollout):
    assert len(a_rollout) == len(b_rollout)
    for a, b in zip(a_rollout, b_rollout):
        assert a["info"] == b["info"]
        a_rew, a_ob, a_first = a["ob"]
        b_rew, b_ob, b_first = b["ob"]
        assert np.array_equal(a_rew, b_rew)
        assert np.array_equal(a_first, b_first)
        assert sorted(a_ob.keys()) == sorted(b_ob.keys())
        for k in sorted(a_ob.keys()):
            assert np.array_equal(a_ob[k], b_ob[k])
        if "state" in a and "state" in b:
            assert a["state"] == b["state"]


@pytest.mark.skip(reason="slow")
@pytest.mark.parametrize("env_name", ENV_NAMES)
def test_state(env_name):
    run_state_test(env_name)


def run_state_test(env_name):
    env_kwargs = dict(num=2, env_name=env_name, rand_seed=0)
    env = ProcgenGym3Env(**env_kwargs)
    rng = np.random.RandomState(0)
    actions = [
        gym3.types_np.sample(env.ac_space, bshape=(env.num,), rng=rng)
        for _ in range(NUM_STEPS)
    ]
    ref_rollouts = run_in_subproc(
        gather_rollouts, env_kwargs=env_kwargs, actions=actions
    )
    assert len(ref_rollouts) == NUM_STEPS + 1

    # run the same thing a second time
    basic_rollouts = run_in_subproc(
        gather_rollouts, env_kwargs=env_kwargs, actions=actions
    )
    assert_rollouts_identical(ref_rollouts, basic_rollouts)

    # run but save states
    state_rollouts = run_in_subproc(
        gather_rollouts, env_kwargs=env_kwargs, actions=actions, get_state=True
    )
    assert_rollouts_identical(ref_rollouts, state_rollouts)

    # make sure states are the same
    state_rollouts_2 = run_in_subproc(
        gather_rollouts, env_kwargs=env_kwargs, actions=actions, get_state=True
    )
    assert_rollouts_identical(ref_rollouts, state_rollouts_2)
    assert_rollouts_identical(state_rollouts, state_rollouts_2)

    # save and restore at each timestep
    state_rollouts_3 = run_in_subproc(
        gather_rollouts,
        env_kwargs=env_kwargs,
        actions=actions,
        get_state=True,
        set_state_every_step=True,
    )
    assert_rollouts_identical(ref_rollouts, state_rollouts_3)
    assert_rollouts_identical(state_rollouts, state_rollouts_3)

    # restore a point in the middle of the rollout and make sure that the remainder of the data looks the same
    offset = NUM_STEPS // 2
    state_restore_rollouts = run_in_subproc(
        gather_rollouts,
        env_kwargs={**env_kwargs, "rand_seed": 1},
        actions=actions[offset:],
        state=state_rollouts[offset]["state"],
        get_state=True,
    )
    assert_rollouts_identical(ref_rollouts[offset:], state_restore_rollouts)
    assert_rollouts_identical(state_rollouts[offset:], state_restore_rollouts)
