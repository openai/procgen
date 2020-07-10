# Changelog

## 0.10.4

* Support `render_mode` to gym environment and update docs to remove references to the confusing `render=True` option.

## 0.10.3

* fix render option for gym environment

## 0.10.2

* fix interactive script

## 0.10.1

* build fixes
* save action during libenv_act

## 0.10.0

* add `set_state`, `get_state` methods to save/restore environment state
* new flags: `use_backgrounds`, `restrict_themes`, `use_monocrhome_assets`
* switch to use `gym3` instead of `libenv` + `Scalarize`, `gym` and `baselines.VecEnv` interfaces are still available with the same names, the `gym3` environment is called `ProcgenGym3Env`
* zero initialize more member variables
* changed `info` dict to have more clear keys, `prev_level_complete` tells you if the level was complete on the previous timestep, since the `info` dict corresponds to the current timestep, and the current timestep is never on a complete level due to automatic resetting.  Similarly, `prev_level_seed` is the level seed from the previous timestep.
* environment creation should be slightly faster

## 0.9.5

* zero initialize member variables from base classes

## 0.9.4

* add random agent script
* add example Dockerfile

## 0.9.3

* changed pyglet dependency to `pyglet~=1.4.8`
* fix issue with procgen thinking it was installed in development mode and attempting to build when installed from a pypi package
* make procgen more fork safe when `num_threads=0`

## 0.9.2

* fixed type bug in interactive script that would sometimes cause the script to not start

## 0.9.1

* initial release
