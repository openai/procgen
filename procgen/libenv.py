import collections
import copy
import os
import platform
from typing import Dict, Union, List, Any, Optional, Tuple

import numpy as np
import gym
import gym.spaces
from cffi import FFI

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ACTION_KEY = "action"
STATE_NEEDS_RESET = "needs_reset"
STATE_WAIT_ACT = "wait_act"
STATE_WAIT_WAIT = "step_wait"


class CVecEnv:
    """
    An environment instance created by an EnvLib, uses the VecEnv interface.

    https://github.com/openai/baselines/blob/master/baselines/common/vec_env/__init__.py

    Args:
        num_envs: number of environments to create
        lib_dir: a folder containing either lib{name}.so (Linux), lib{name}.dylib (Mac), or {name}.dll (Windows)
        lib_name: name of the library (minus the lib part)
        c_func_defs: list of cdefs that are passed to FFI in order to define custom functions that can then be called with env.call_func()
        options: options to pass to the libenv_make() call for this environment
        debug: if set to True, check array data to make sure it matches the provided spaces
        reuse_arrays: reduce allocations by using the same numpy arrays for each reset(), step(), and render() call
    """

    def __init__(
        self,
        num_envs: int,
        lib_dir: str,
        lib_name: str = "env",
        c_func_defs: Optional[List[str]] = None,
        options: Optional[Dict] = None,
        debug: bool = False,
        reuse_arrays: bool = False,
    ) -> None:
        self._debug = debug
        self._reuse_arrays = reuse_arrays

        if options is None:
            options = {}
        options = copy.deepcopy(options)

        if platform.system() == "Linux":
            lib_filename = f"lib{lib_name}.so"
        elif platform.system() == "Darwin":
            lib_filename = f"lib{lib_name}.dylib"
        elif platform.system() == "Windows":
            lib_filename = f"{lib_name}.dll"
        else:
            raise Exception(f"unrecognized platform {platform.system()}")

        if c_func_defs is None:
            c_func_defs = []

        # load cdef for libenv.h
        libenv_cdef = ""
        with open(os.path.join(SCRIPT_DIR, "libenv.h")) as f:
            inside_cdef = False
            for line in f:
                if line.startswith("// BEGIN_CDEF"):
                    inside_cdef = True
                elif line.startswith("// END_CDEF"):
                    inside_cdef = False
                elif line.startswith("#if") or line.startswith("#endif"):
                    continue

                if inside_cdef:
                    line = line.replace("LIBENV_API", "")
                    libenv_cdef += line

        self._ffi = FFI()
        self._ffi.cdef(libenv_cdef)

        for cdef in c_func_defs:
            self._ffi.cdef(cdef)

        self._lib_path = os.path.join(lib_dir, lib_filename)
        assert os.path.exists(self._lib_path), f"lib not found at {self._lib_path}"
        # unclear if this is necessary, but nice to not have symbols conflict if possible
        dlopen_flags = (
            self._ffi.RTLD_NOW | self._ffi.RTLD_LOCAL  # pylint: disable=no-member
        )
        if platform.system() == "Linux":
            dlopen_flags |= self._ffi.RTLD_DEEPBIND  # pylint: disable=no-member
        self._c_lib = self._ffi.dlopen(name=self._lib_path, flags=dlopen_flags)
        # dlclose will be called automatically when the library goes out of scope
        # https://cffi.readthedocs.io/en/latest/cdef.html#ffi-dlopen-loading-libraries-in-abi-mode
        # on mac os x, the library may not always be unloaded when you expect
        # https://developer.apple.com/videos/play/wwdc2017/413/?time=1776
        # loading/unloading the library all the time can be slow
        # it may be useful to keep a reference to an environment (and thus the c_lib object)
        # to avoid this happening

        self._options = options
        self._state = STATE_NEEDS_RESET

        c_options, self._options_keepalives = self._convert_options(
            self._ffi, self._c_lib, options
        )

        self._c_env = self._c_lib.libenv_make(num_envs, c_options[0])

        self.reward_range = (float("-inf"), float("inf"))
        self.spec = None
        self.num_envs = num_envs
        self.observation_space = self._get_spaces(self._c_lib.LIBENV_SPACES_OBSERVATION)
        self._action_space = self._get_spaces(self._c_lib.LIBENV_SPACES_ACTION)
        self._info_space = self._get_spaces(self._c_lib.LIBENV_SPACES_INFO)
        self._render_space = self._get_spaces(self._c_lib.LIBENV_SPACES_RENDER)

        # allocate buffers
        self._observations, self._observation_buffers = self._allocate_dict_space(
            self.num_envs, self.observation_space
        )

        # we only use dict spaces for consistency, but action is always a single space
        # the private version is the dict space, while the public version is a single space
        assert len(self._action_space.spaces) == 1, "action space can only be 1 element"
        assert list(self._action_space.spaces.keys())[0] == ACTION_KEY
        self.action_space = self._action_space.spaces[ACTION_KEY]
        dict_actions, self._action_buffers = self._allocate_dict_space(
            self.num_envs, self._action_space
        )
        self._actions = dict_actions[ACTION_KEY]

        self._renders, self._renders_buffers = self._allocate_dict_space(
            self.num_envs, self._render_space
        )

        self.metadata = {"render.modes": list(self._render_space.spaces.keys())}

        self._infos, self._infos_buffers = self._allocate_dict_space(
            self.num_envs, self._info_space
        )

        self._rews, self._rews_buffer = self._allocate_array(
            self.num_envs, np.dtype("float32")
        )
        self._dones, self._dones_buffer = self._allocate_array(
            self.num_envs, np.dtype("bool")
        )
        assert np.dtype("bool").itemsize == 1

        c_step = self._ffi.new("struct libenv_step *")
        c_step.obs = self._observation_buffers
        # cast the pointer to the buffer to avoid a warning from cffi
        c_step.rews = self._ffi.cast(
            self._ffi.typeof(c_step.rews).cname, self._rews_buffer
        )
        c_step.dones = self._ffi.cast(
            self._ffi.typeof(c_step.dones).cname, self._dones_buffer
        )
        c_step.infos = self._infos_buffers
        self._c_step = c_step

        self.closed = False
        self.viewer = None

    def __repr__(self):
        return f"<CVecEnv lib_path={self._lib_path} options={self._options}>"

    def _numpy_aligned(self, shape, dtype, align=64):
        """
        Allocate an aligned numpy array, based on https://github.com/numpy/numpy/issues/5312#issuecomment-299533915
        """
        n_bytes = np.prod(shape) * dtype.itemsize
        arr = np.zeros(n_bytes + (align - 1), dtype=np.uint8)
        data_align = arr.ctypes.data % align
        offset = 0 if data_align == 0 else (align - data_align)
        view = arr[offset : offset + n_bytes].view(dtype)
        return view.reshape(shape)

    def _allocate_dict_space(
        self, num_envs: int, dict_space: gym.spaces.Dict
    ) -> Tuple[collections.OrderedDict, Any]:
        """
        Allocate arrays for a space, returns an OrderedDict of numpy arrays along with a backing bytearray
        """
        result = collections.OrderedDict()  # type: collections.OrderedDict
        length = len(dict_space.spaces) * num_envs
        buffers = self._ffi.new(f"void *[{length}]")
        for space_idx, (name, space) in enumerate(dict_space.spaces.items()):
            actual_shape = (num_envs,) + space.shape
            arr = self._numpy_aligned(shape=actual_shape, dtype=space.dtype)
            result[name] = arr
            for env_idx in range(num_envs):
                buffers[space_idx * num_envs + env_idx] = self._ffi.from_buffer(
                    arr.data[env_idx:]
                )
        return result, buffers

    def _allocate_array(self, num_envs: int, dtype: np.dtype) -> Tuple[np.ndarray, Any]:
        arr = self._numpy_aligned(shape=(num_envs,), dtype=dtype)
        return arr, self._ffi.from_buffer(arr.data)

    @staticmethod
    def _convert_options(ffi: Any, c_lib: Any, options: Dict) -> Any:
        """
        Convert a dictionary to libenv_options
        """
        keepalives = (
            []
        )  # add variables to here to keep them alive after this function returns
        c_options = ffi.new("struct libenv_options *")
        c_option_array = ffi.new("struct libenv_option[%d]" % len(options))

        for i, (k, v) in enumerate(options.items()):
            name = str(k).encode("utf8")
            assert (
                len(name) < c_lib.LIBENV_MAX_NAME_LEN - 1
            ), "length of options key is too long"
            if isinstance(v, bytes):
                c_data = ffi.new("char[]", v)
                dtype = c_lib.LIBENV_DTYPE_UINT8
                count = len(v)
            elif isinstance(v, str):
                c_data = ffi.new("char[]", v.encode("utf8"))
                dtype = c_lib.LIBENV_DTYPE_UINT8
                count = len(v)
            elif isinstance(v, bool):
                c_data = ffi.new("uint8_t*", v)
                dtype = c_lib.LIBENV_DTYPE_UINT8
                count = 1
            elif isinstance(v, int):
                assert -2 ** 31 < v < 2 ** 31
                c_data = ffi.new("int32_t*", v)
                dtype = c_lib.LIBENV_DTYPE_INT32
                count = 1
            elif isinstance(v, float):
                c_data = ffi.new("float*", v)
                dtype = c_lib.LIBENV_DTYPE_FLOAT32
                count = 1
            elif isinstance(v, np.ndarray):
                c_data = ffi.new("char[]", v.tobytes())
                if v.dtype == np.dtype("uint8"):
                    dtype = c_lib.LIBENV_DTYPE_UINT8
                elif v.dtype == np.dtype("int32"):
                    dtype = c_lib.LIBENV_DTYPE_INT32
                elif v.dtype == np.dtype("float32"):
                    dtype = c_lib.LIBENV_DTYPE_FLOAT32
                else:
                    assert False, f"unsupported type {v.dtype}"
                count = v.size
            else:
                assert False, f"unsupported value {v} for option {k}"

            c_option_array[i].name = name
            c_option_array[i].dtype = dtype
            c_option_array[i].count = count
            c_option_array[i].data = c_data
            keepalives.append(c_data)

        keepalives.append(c_option_array)
        c_options.items = c_option_array
        c_options.count = len(options)
        return c_options, keepalives

    @staticmethod
    def _check_arrays(
        arrays: Dict[str, np.ndarray], num_envs: int, dict_space: gym.spaces.Dict
    ) -> None:
        """
        Make sure each array in arrays matches the given dict_space
        """
        for name, space in dict_space.spaces.items():
            arr = arrays[name]
            assert isinstance(arr, np.ndarray)
            expected_shape = (num_envs,) + space.shape
            assert (
                arr.shape == expected_shape
            ), f"array is invalid shape expected={expected_shape} received={arr.shape}"
            assert (
                arr.dtype == space.dtype
            ), f"array has invalid dtype expected={space.dtype} received={arr.dtype}"
            if isinstance(space, gym.spaces.Box):
                # we only support single low/high values
                assert (
                    len(np.unique(space.low)) == 1 and len(np.unique(space.high)) == 1
                )
                low = np.min(space.low)
                high = np.max(space.high)
            elif isinstance(space, gym.spaces.Discrete):
                low = 0
                high = space.n - 1
            else:
                assert False, "unrecognized space type"
            assert np.min(arr) >= low, (
                '"%s" has values below space lower bound, %f < %f'
                % (name, np.min(arr), low)
            )
            assert np.max(arr) <= high, (
                '"%s" has values above space upper bound, %f > %f'
                % (name, np.max(arr), high)
            )

    def _maybe_copy_ndarray(self, obj: np.ndarray) -> np.ndarray:
        """
        Copy a single numpy array if reuse_arrays is False,
        otherwise just return the object
        """
        if self._reuse_arrays:
            return obj
        else:
            return obj.copy()

    def _maybe_copy_dict(self, obj: Dict[str, Any]) -> Dict[str, Any]:
        """
        Copy a list of dicts of numpy arrays if reuse_arrays is False,
        otherwise just return the object
        """
        if self._reuse_arrays:
            return obj
        else:
            result = {}
            for name, arr in obj.items():
                result[name] = arr.copy()
            return result

    def _get_spaces(self, c_name: Any) -> gym.spaces.Dict:
        """
        Get a c space and convert to a gym space
        """
        count = self._c_lib.libenv_get_spaces(self._c_env, c_name, self._ffi.NULL)
        if count == 0:
            return gym.spaces.Dict([])

        c_spaces = self._ffi.new("struct libenv_space[%d]" % count)
        self._c_lib.libenv_get_spaces(self._c_env, c_name, c_spaces)

        # convert to gym spaces
        spaces = []
        for i in range(count):
            c_space = c_spaces[i]

            name = self._ffi.string(c_space.name).decode("utf8")
            shape = []
            for j in range(c_space.ndim):
                shape.append(c_space.shape[j])

            if c_space.dtype == self._c_lib.LIBENV_DTYPE_UINT8:
                dtype = np.dtype("uint8")
                low = c_space.low.uint8
                high = c_space.high.uint8
            elif c_space.dtype == self._c_lib.LIBENV_DTYPE_INT32:
                dtype = np.dtype("int32")
                low = c_space.low.int32
                high = c_space.high.int32
            elif c_space.dtype == self._c_lib.LIBENV_DTYPE_FLOAT32:
                dtype = np.dtype("float32")
                low = c_space.low.float32
                high = c_space.high.float32
            else:
                assert False, "unknown dtype"

            if c_space.type == self._c_lib.LIBENV_SPACE_TYPE_BOX:
                space = gym.spaces.Box(shape=shape, low=low, high=high, dtype=dtype)
            elif c_space.type == self._c_lib.LIBENV_SPACE_TYPE_DISCRETE:
                assert shape == [1], "discrete space must have scalar shape"
                assert low == 0 and high > 0, "discrete low/high bounds are incorrect"
                space = gym.spaces.Discrete(n=high + 1)
                space.dtype = dtype
            else:
                assert False, "unknown space type"
            spaces.append((name, space))
        # c spaces are aways a single-layer Dict space
        return gym.spaces.Dict(spaces)

    def reset(self) -> Dict[str, np.ndarray]:
        """
        Reset the environment and return the first observation
        """
        self._state = STATE_WAIT_ACT

        self._c_lib.libenv_reset(self._c_env, self._c_step)
        return self._maybe_copy_dict(self._observations)

    def step_async(self, actions: np.ndarray) -> None:
        """
        Asynchronously take an action in the environment, doesn't return anything.
        """
        assert self._state == STATE_WAIT_ACT
        self._state = STATE_WAIT_WAIT

        if self._debug:
            self._check_arrays({"action": actions}, self.num_envs, self._action_space)

        self._actions[:] = actions
        self._c_lib.libenv_step_async(self._c_env, self._action_buffers, self._c_step)

    def step_wait(
        self
    ) -> Tuple[Dict[str, np.ndarray], np.ndarray, np.ndarray, List[Dict[str, Any]]]:
        """
        Step the environment, returns (obs, rews, dones, infos)
        """
        assert self._state == STATE_WAIT_WAIT
        self._state = STATE_WAIT_ACT

        self._c_lib.libenv_step_wait(self._c_env)

        if self._debug:
            self._check_arrays(
                self._observations, self.num_envs, self.observation_space
            )

        infos = [{} for _ in range(self.num_envs)]  # type: List[Dict]
        for key, values in self._infos.items():
            for env_idx in range(self.num_envs):
                v = values[env_idx]
                if v.shape == (1,):
                    # extract scalar values
                    v = v[0]
                infos[env_idx][key] = v

        return (
            self._maybe_copy_dict(self._observations),
            self._maybe_copy_ndarray(self._rews),
            self._maybe_copy_ndarray(self._dones),
            infos,
        )

    def step(
        self, actions: np.ndarray
    ) -> Tuple[Dict[str, np.ndarray], np.ndarray, np.ndarray, List[Dict[str, Any]]]:
        """
        Step the environment, combines step_async() and step_wait() and makes this interface mostly compatible with the normal gym interface
        """
        self.step_async(actions)
        return self.step_wait()

    def render(self, mode: str = "human") -> Union[bool, np.ndarray]:
        """
        Render the environment.

        mode='human' tells the environment to render in some human visible way and
        returns True if the user user visible window created by the environment is still open

        Other modes are up to the environment, but end up returning a numpy array of some kind
        that will be tiled as if it were an image by this code.  Call get_images() instead if
        that is not the behavior you want.
        """
        if (
            mode == "human"
            and "human" not in self.metadata["render.modes"]
            and "rgb_array" in self.metadata["render.modes"]
        ):
            # fallback human mode
            viewer = self.get_viewer()
            self._render(mode="rgb_array")
            images = self._maybe_copy_ndarray(self._renders["rgb_array"])
            viewer.imshow(self._tile_images(images))
            return viewer.isopen

        isopen = self._render(mode=mode)
        if mode == "human":
            return isopen
        else:
            images = self._maybe_copy_ndarray(self._renders[mode])
            return self._tile_images(images)

    def _render(self, mode) -> Union[bool, np.ndarray]:
        """Internal render method, returns is_open and updates the buffers in self._render"""
        assert self._state == STATE_WAIT_ACT
        assert mode in self.metadata["render.modes"], "unsupported render mode"

        c_mode = self._ffi.new("char[]", mode.encode("utf8"))
        # only pass the render buffers for the selected mode
        space_idx = list(self._render_space.spaces.keys()).index(mode)
        render_buffers = self._renders_buffers[
            space_idx * self.num_envs : (space_idx + 1) * self.num_envs
        ]
        return self._c_lib.libenv_render(self._c_env, c_mode, render_buffers)

    def _tile_images(self, images: np.ndarray) -> np.ndarray:
        """Tile a set of NHWC images"""
        num_images, height, width, chans = images.shape
        width_images = int(np.ceil(np.sqrt(num_images)))
        height_images = int(np.ceil(float(num_images) / width_images))
        result = np.zeros(
            (height_images * height, width_images * width, chans), dtype=np.uint8
        )
        for col in range(width_images):
            for row in range(height_images):
                idx = row * width_images + col
                if idx >= len(images):
                    continue
                result[
                    row * height : (row + 1) * height,
                    col * width : (col + 1) * width,
                    :,
                ] = images[idx]
        return result

    def get_images(self) -> np.ndarray:
        """
        Get rendered images from the environments, if supported.

        The returned array's shape will be (num_envs, height, width, num_colors)
        """
        self._render(mode="rgb_array")
        return self._maybe_copy_ndarray(self._renders["rgb_array"])

    def get_viewer(self):
        """Get the viewer instance being used by render()"""
        if self.viewer is None:
            from gym.envs.classic_control import rendering

            self.viewer = rendering.SimpleImageViewer()
        return self.viewer

    def close_extras(self):
        """
        Override this to close environment-specific resources without having to override close()'

        This method is guaranteed to only be called once, unlike close()
        """

    def close(self) -> None:
        """Close the environment and free any resources associated with it"""
        if not hasattr(self, "closed") or self.closed:
            return

        self.closed = True
        self._c_lib.libenv_close(self._c_env)
        self._c_lib = None
        self._ffi = None
        self._options_keepalives = None
        if self.viewer is not None:
            self.viewer.close()
        self.close_extras()

    def call_func(self, name: str, *args: Any) -> Any:
        """
        Call a function of the libenv declared in c_func_defs
        """
        return getattr(self._c_lib, name)(*args)

    @property
    def unwrapped(self):
        if hasattr(self, "venv"):
            return self.venv.unwrapped  # pylint: disable=no-member
        else:
            return self

    def seed(self, seed=None):
        """
        Seed the environment, this isn't used by VecEnvs but is part of the Gym Env API
        """

    def __del__(self):
        self.close()
