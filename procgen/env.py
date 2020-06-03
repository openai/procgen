import os
import random
from typing import Sequence, Optional, List

import gym3
from gym3.libenv import CEnv
import numpy as np
from .build import build

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

MAX_STATE_SIZE = 2 ** 20

ENV_NAMES = [
    "bigfish",
    "bossfight",
    "caveflyer",
    "chaser",
    "climber",
    "coinrun",
    "dodgeball",
    "fruitbot",
    "heist",
    "jumper",
    "leaper",
    "maze",
    "miner",
    "ninja",
    "plunder",
    "starpilot",
]

EXPLORATION_LEVEL_SEEDS = {
    "coinrun": 1949448038,
    "caveflyer": 1259048185,
    "leaper": 1318677581,
    "jumper": 1434825276,
    "maze": 158988835,
    "heist": 876640971,
    "climber": 1561126160,
    "ninja": 1123500215,
}

# should match DistributionMode in game.h, except for 'exploration' which is handled by Python
DISTRIBUTION_MODE_DICT = {
    "easy": 0,
    "hard": 1,
    "extreme": 2,
    "memory": 10,
    "exploration": 20,
}


def create_random_seed():
    rand_seed = random.SystemRandom().randint(0, 2 ** 31 - 1)
    try:
        # force MPI processes to definitely choose different random seeds
        from mpi4py import MPI

        rand_seed = rand_seed - (rand_seed % MPI.COMM_WORLD.size) + MPI.COMM_WORLD.rank
    except ModuleNotFoundError:
        pass
    return rand_seed


class BaseProcgenEnv(CEnv):
    """
    Base procedurally generated environment
    """

    def __init__(
        self,
        num,
        env_name,
        options,
        debug=False,
        rand_seed=None,
        num_levels=0,
        start_level=0,
        use_sequential_levels=False,
        debug_mode=0,
        resource_root=None,
        num_threads=4,
        render_mode=None,
    ):
        if resource_root is None:
            resource_root = os.path.join(SCRIPT_DIR, "data", "assets") + os.sep
            assert os.path.exists(resource_root)

        lib_dir = os.path.join(SCRIPT_DIR, "data", "prebuilt")
        if os.path.exists(lib_dir):
            assert any([os.path.exists(os.path.join(lib_dir, name)) for name in ["libenv.so", "libenv.dylib", "env.dll"]]), "package is installed, but the prebuilt environment library is missing"
            assert not debug, "debug has no effect for pre-compiled library"
        else:
            # only compile if we don't find a pre-built binary
            lib_dir = build(debug=debug)
        
        self.combos = self.get_combos()

        if render_mode is None:
            render_human = False
        elif render_mode == "rgb_array":
            render_human = True
        else:
            raise Exception(f"invalid render mode {render_mode}")

        if rand_seed is None:
            rand_seed = create_random_seed()

        options.update(
            {
                "env_name": env_name,
                "num_levels": num_levels,
                "start_level": start_level,
                "num_actions": len(self.combos),
                "use_sequential_levels": bool(use_sequential_levels),
                "debug_mode": debug_mode,
                "rand_seed": rand_seed,
                "num_threads": num_threads,
                "render_human": render_human,
                # these will only be used the first time an environment is created in a process
                "resource_root": resource_root,
            }
        )

        self.options = options

        super().__init__(
            lib_dir=lib_dir,
            num=num,
            options=options,
            c_func_defs=[
                "int get_state(libenv_env *, int, char *, int);",
                "void set_state(libenv_env *, int, char *, int);",
            ],
        )
        # don't use the dict space for actions
        self.ac_space = self.ac_space["action"]

    def get_state(self):
        length = MAX_STATE_SIZE
        buf = self._ffi.new(f"char[{length}]")
        result = []
        for env_idx in range(self.num):
            n = self.call_c_func("get_state", env_idx, buf, length)
            result.append(bytes(self._ffi.buffer(buf, n)))
        return result

    def set_state(self, states):
        assert len(states) == self.num
        for env_idx in range(self.num):
            state = states[env_idx]
            self.call_c_func("set_state", env_idx, state, len(state))

    def get_combos(self):
        return [
            ("LEFT", "DOWN"),
            ("LEFT",),
            ("LEFT", "UP"),
            ("DOWN",),
            (),
            ("UP",),
            ("RIGHT", "DOWN"),
            ("RIGHT",),
            ("RIGHT", "UP"),
            ("D",),
            ("A",),
            ("W",),
            ("S",),
            ("Q",),
            ("E",),
        ]

    def keys_to_act(self, keys_list: Sequence[Sequence[str]]) -> List[Optional[np.ndarray]]:
        """
        Convert list of keys being pressed to actions, used in interactive mode
        """
        result = []
        for keys in keys_list:
            action = None
            max_len = -1
            for i, combo in enumerate(self.get_combos()):
                pressed = True
                for key in combo:
                    if key not in keys:
                        pressed = False

                if pressed and (max_len < len(combo)):
                    action = i
                    max_len = len(combo)

            if action is not None:
                action = np.array([action])
            result.append(action)
        return result

    def act(self, ac):
        # tensorflow may return int64 actions (https://github.com/openai/gym/blob/master/gym/spaces/discrete.py#L13)
        # so always cast actions to int32
        return super().act({"action": ac.astype(np.int32)})


class ProcgenGym3Env(BaseProcgenEnv):
    """
    gym3 interface for Procgen
    """
    def __init__(
        self,
        num,
        env_name,
        center_agent=True,
        use_backgrounds=True,
        use_monochrome_assets=False,
        restrict_themes=False,
        use_generated_assets=False,
        paint_vel_info=False,
        distribution_mode="hard",
        **kwargs,
    ):
        assert (
            distribution_mode in DISTRIBUTION_MODE_DICT
        ), f'"{distribution_mode}" is not a valid distribution mode.'

        if distribution_mode == "exploration":
            assert (
                env_name in EXPLORATION_LEVEL_SEEDS
            ), f"{env_name} does not support exploration mode"

            distribution_mode = DISTRIBUTION_MODE_DICT["hard"]
            assert "num_levels" not in kwargs, "exploration mode overrides num_levels"
            kwargs["num_levels"] = 1
            assert "start_level" not in kwargs, "exploration mode overrides start_level"
            kwargs["start_level"] = EXPLORATION_LEVEL_SEEDS[env_name]
        else:
            distribution_mode = DISTRIBUTION_MODE_DICT[distribution_mode]

        options = {
                "center_agent": bool(center_agent),
                "use_generated_assets": bool(use_generated_assets),
                "use_monochrome_assets": bool(use_monochrome_assets),
                "restrict_themes": bool(restrict_themes),
                "use_backgrounds": bool(use_backgrounds),
                "paint_vel_info": bool(paint_vel_info),
                "distribution_mode": distribution_mode,
            }
        super().__init__(num, env_name, options, **kwargs)


def ProcgenEnv(num_envs, env_name, **kwargs):
    """
    Baselines VecEnv interface for Procgen
    """
    return gym3.ToBaselinesVecEnv(ProcgenGym3Env(num=num_envs, env_name=env_name, **kwargs))