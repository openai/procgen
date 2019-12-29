import os
import random

from .libenv import CVecEnv
import numpy as np
from .build import build

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

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


class BaseProcgenEnv(CVecEnv):
    """
    Base procedurally generated environment
    """

    def __init__(
        self,
        num_envs,
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
                # these will only be used the first time an environment is created in a process
                "resource_root": resource_root,
            }
        )

        self.options = options

        super().__init__(
            lib_dir=lib_dir, num_envs=num_envs, debug=debug, options=options
        )

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

    def step_async(self, actions):
        # tensorflow may return int64 actions (https://github.com/openai/gym/blob/master/gym/spaces/discrete.py#L13)
        # so always cast actions to int32
        return super().step_async(actions.astype(np.int32))


class ProcgenEnv(BaseProcgenEnv):
    def __init__(
        self,
        num_envs,
        env_name,
        center_agent=True,
        options=None,
        use_generated_assets=False,
        paint_vel_info=False,
        distribution_mode="hard",
        **kwargs,
    ):
        if options is None:
            options = {}
        else:
            options = dict(options)

        assert (
            distribution_mode in DISTRIBUTION_MODE_DICT
        ), f'"{distribution_mode}" is not a valid distribution mode.'

        if distribution_mode == "exploration":
            assert env_name in EXPLORATION_LEVEL_SEEDS, f"{env_name} does not support exploration mode"

            distribution_mode = DISTRIBUTION_MODE_DICT["hard"]
            assert "num_levels" not in kwargs, "exploration mode overrides num_levels"
            kwargs["num_levels"] = 1
            assert "start_level" not in kwargs, "exploration mode overrides start_level"
            kwargs["start_level"] = EXPLORATION_LEVEL_SEEDS[env_name]
        else:
            distribution_mode = DISTRIBUTION_MODE_DICT[distribution_mode]

        options.update(
            {
                "center_agent": bool(center_agent),
                "use_generated_assets": bool(use_generated_assets),
                "paint_vel_info": bool(paint_vel_info),
                "distribution_mode": distribution_mode,
            }
        )
        super().__init__(num_envs, env_name, options, **kwargs)
