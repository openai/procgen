#!/usr/bin/env python
import argparse

from .interactive_base import Interactive
from procgen import ProcgenEnv
from .env import ENV_NAMES
from .scalarize import Scalarize


class ProcgenInteractive(Interactive):
    """
    Interactive version of Procgen environments for humans to use
    """

    def __init__(self, vision, **kwargs):
        self._vision = vision
        venv = ProcgenEnv(num_envs=1, **kwargs)
        self.combos = list(venv.unwrapped.combos)
        self.last_keys = []
        env = Scalarize(venv)
        super().__init__(env=env, sync=False, tps=15, display_info=True)

    def get_image(self, obs, env):
        if self._vision == "human":
            return env.render(mode="rgb_array")
        else:
            return obs["rgb"]

    def keys_to_act(self, keys):
        action = None
        max_len = -1

        if "RETURN" in keys and "RETURN" not in self.last_keys:
            action = -1
        else:
            for i, combo in enumerate(self.combos):
                pressed = True
                for key in combo:
                    if key not in keys:
                        pressed = False

                if pressed and (max_len < len(combo)):
                    action = i
                    max_len = len(combo)

        self.last_keys = list(keys)

        return action


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--vision", choices=["agent", "human"], default="human")
    parser.add_argument("--record-dir", help="directory to record movies to")
    parser.add_argument("--distribution-mode", default="hard", help="which distribution mode to use for the level generation")
    parser.add_argument("--env-name", default="coinrun", help="name of game to create", choices=ENV_NAMES)
    parser.add_argument("--level-seed", type=int, help="select an individual level to use")
    args = parser.parse_args()

    kwargs = {"distribution_mode": args.distribution_mode}
    if args.level_seed is not None:
        kwargs["start_level"] = args.level_seed
        kwargs["num_levels"] = 1
    ia = ProcgenInteractive(args.vision, env_name=args.env_name, **kwargs)
    ia.run(record_dir=args.record_dir)


if __name__ == "__main__":
    main()
