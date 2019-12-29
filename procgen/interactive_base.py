"""
Interact with Gym environments using the keyboard

An adapter object is defined for each environment to map keyboard commands to actions and extract observations as pixels.
"""

import pyglet

# don't require an x server to import this file
pyglet.options["shadow_window"] = False
import sys
import ctypes
import os
import abc
import time

import numpy as np
from pyglet import gl
from pyglet.window import key as keycodes
import imageio


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CHAR_SIZE = 32
FONT = None
SECONDS_TO_DISPLAY_DONE_INFO = 3


def convert_ascii_to_rgb(ascii):
    """
    Convert ascii observations to an image using the loaded font
    """
    global FONT
    if FONT is None:
        FONT = np.load(os.path.join(SCRIPT_DIR, "font.bin"))["font"]

    height, width = ascii.shape
    images = np.zeros((height * CHAR_SIZE, width * CHAR_SIZE, 3), dtype=np.uint8)
    for y in range(height):
        for x in range(width):
            ch = ascii[y, x]
            images[
                y * CHAR_SIZE : (y + 1) * CHAR_SIZE,
                x * CHAR_SIZE : (x + 1) * CHAR_SIZE,
                :,
            ] = FONT[ch]
    return images


class Interactive(abc.ABC):
    """
    Base class for making gym environments interactive for human use
    """

    def __init__(self, env, sync=True, tps=60, aspect_ratio=None, display_info=False):
        self._record_dir = None
        self._movie_writer = None
        self._episode = 0
        self._display_info = display_info
        self._seconds_to_display_done_info = 0

        obs = env.reset()
        self._image = self.get_image(obs, env)
        assert (
            len(self._image.shape) == 3 and self._image.shape[2] == 3
        ), "must be an RGB image"
        image_height, image_width = self._image.shape[:2]

        if aspect_ratio is None:
            aspect_ratio = image_width / image_height

        # guess a screen size that doesn't distort the image too much but also is not tiny or huge
        display = pyglet.canvas.get_display()
        screen = display.get_default_screen()
        max_win_width = screen.width * 0.9
        max_win_height = screen.height * 0.9
        win_width = image_width
        win_height = int(win_width / aspect_ratio)

        while win_width > max_win_width or win_height > max_win_height:
            win_width //= 2
            win_height //= 2
        while win_width < max_win_width / 2 and win_height < max_win_height / 2:
            win_width *= 2
            win_height *= 2

        self._info_width = win_width // 2
        if display_info:
            win_width += self._info_width
        win = pyglet.window.Window(width=win_width, height=win_height)

        self._key_handler = pyglet.window.key.KeyStateHandler()
        win.push_handlers(self._key_handler)
        win.on_close = self._on_close

        gl.glEnable(gl.GL_TEXTURE_2D)
        self._texture_id = gl.GLuint(0)
        gl.glGenTextures(1, ctypes.byref(self._texture_id))
        gl.glBindTexture(gl.GL_TEXTURE_2D, self._texture_id)
        gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_WRAP_S, gl.GL_CLAMP)
        gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_WRAP_T, gl.GL_CLAMP)
        gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MAG_FILTER, gl.GL_NEAREST)
        gl.glTexParameteri(gl.GL_TEXTURE_2D, gl.GL_TEXTURE_MIN_FILTER, gl.GL_NEAREST)
        gl.glTexImage2D(
            gl.GL_TEXTURE_2D,
            0,
            gl.GL_RGBA8,
            image_width,
            image_height,
            0,
            gl.GL_RGB,
            gl.GL_UNSIGNED_BYTE,
            None,
        )

        self._env = env
        self._win = win

        self._key_previous_states = {}

        self._steps = 0
        self._episode_steps = 0
        self._episode_return = 0
        self._prev_episode_return = 0
        self._last_info = {}

        self._tps = tps
        self._sync = sync
        self._current_time = 0
        self._sim_time = 0
        self._max_sim_frames_per_update = 4

        self._info_label = pyglet.text.Label(
            "<label>",
            font_name="Courier New",
            font_size=self._win.height // 42,
            multiline=True,
            x=self._win.width - self._info_width + 10,
            y=self._win.height//2,
            width=self._info_width,
            anchor_x="left",
            anchor_y="center",
        )

        self._done_info_label = pyglet.text.Label(
            "<label>",
            font_name="Courier New",
            font_size=self._win.height // 42,
            multiline=True,
            x=self._win.width//2,
            y=self._win.height//2,
            width=self._info_width,
            anchor_x="center",
            anchor_y="center",
        )

    def _update(self, dt):
        # if we're displaying done info, don't advance the simulation
        if self._seconds_to_display_done_info > 0:
            self._seconds_to_display_done_info -= dt
            return

        # cap the number of frames rendered so we don't just spend forever trying to catch up on frames
        # if rendering is slow
        max_dt = self._max_sim_frames_per_update / self._tps
        if dt > max_dt:
            dt = max_dt

        # catch up the simulation to the current time
        self._current_time += dt
        while self._sim_time < self._current_time:
            self._sim_time += 1 / self._tps

            keys_clicked = set()
            keys_pressed = set()
            for key_code, pressed in self._key_handler.items():
                if pressed:
                    keys_pressed.add(key_code)

                if not self._key_previous_states.get(key_code, False) and pressed:
                    keys_clicked.add(key_code)
                self._key_previous_states[key_code] = pressed

            if keycodes.ESCAPE in keys_pressed:
                self._on_close()

            # assume that for async environments, we just want to repeat keys for as long as they are held
            inputs = keys_pressed
            if self._sync:
                inputs = keys_clicked

            keys = []
            for keycode in inputs:
                for name in dir(keycodes):
                    if getattr(keycodes, name) == keycode:
                        keys.append(name)

            act = self.keys_to_act(keys)

            if not self._sync or act is not None:
                obs, rew, done, info = self._env.step(act)
                self._image = self.get_image(obs, self._env)
                if self._movie_writer is not None:
                    self._movie_writer.append_data(self._image)

                self._episode_return += rew
                self._steps += 1
                self._episode_steps += 1
                self._last_info = dict(episode_steps=self._episode_steps, episode_return=self._episode_return, **info)
                np.set_printoptions(precision=2)
                done_int = int(done)  # shorter than printing True/False
                if self._sync:
                    print(
                        "done={} steps={} episode_steps={} rew={} episode_return={}".format(
                            done_int,
                            self._steps,
                            self._episode_steps,
                            rew,
                            self._episode_return,
                        )
                    )
                elif self._steps % self._tps == 0 or done:
                    episode_return_delta = (
                        self._episode_return - self._prev_episode_return
                    )
                    self._prev_episode_return = self._episode_return
                    print(
                        "done={} steps={} episode_steps={} episode_return_delta={} episode_return={}".format(
                            done_int,
                            self._steps,
                            self._episode_steps,
                            episode_return_delta,
                            self._episode_return,
                        )
                    )

                if done:
                    print(f"final info={self._last_info}")
                    obs = self._env.reset()
                    self._image = self.get_image(obs, self._env)
                    self._episode_steps = 0
                    self._episode_return = 0
                    self._prev_episode_return = 0
                    self._episode += 1
                    if self._movie_writer is not None:
                        self._restart_recording()
                    if self._display_info:
                        self._seconds_to_display_done_info = SECONDS_TO_DISPLAY_DONE_INFO

    def _format_info(self):
        info_text = ""
        for k, v in sorted(self._last_info.items()):
            info_text += f"{k}: {v}\n"
        return info_text

    def _draw(self):
        self._win.clear()

        if self._seconds_to_display_done_info > 0:
            self._done_info_label.text = "=== episode complete ===\n\n" + self._format_info()
            self._done_info_label.draw()
            return
        
        gl.glBindTexture(gl.GL_TEXTURE_2D, self._texture_id)
        image_bytes = self._image.tobytes()
        video_buffer = ctypes.cast(
            image_bytes, ctypes.POINTER(ctypes.c_short)
        )
        gl.glTexSubImage2D(
            gl.GL_TEXTURE_2D,
            0,
            0,
            0,
            self._image.shape[1],
            self._image.shape[0],
            gl.GL_RGB,
            gl.GL_UNSIGNED_BYTE,
            video_buffer,
        )

        x = 0
        y = 0
        w = self._win.width
        h = self._win.height
        if self._display_info:
            w -= self._info_width

        pyglet.graphics.draw(
            4,
            pyglet.gl.GL_QUADS,
            ("v2f", [x, y, x + w, y, x + w, y + h, x, y + h]),
            ("t2f", [0, 1, 1, 1, 1, 0, 0, 0]),
        )

        self._info_label.text = self._format_info()
        if self._display_info:
            self._info_label.draw()

    def _on_close(self):
        self._env.close()
        sys.exit(0)

    def _restart_recording(self):
        if self._movie_writer is not None:
            self._movie_writer.close()

        self._movie_writer = imageio.get_writer(
            os.path.join(self._record_dir, f"{self._episode:03d}.mp4"),
            fps=self._tps,
            quality=9,
        )

    @abc.abstractmethod
    def get_image(self, obs, env):
        """
        Given an observation and the Env object, return an rgb array to display to the user
        """

    @abc.abstractmethod
    def keys_to_act(self, keys):
        """
        Given a list of keys that the user has input, produce a gym action to pass to the environment

        For sync environments, keys is a list of keys that have been pressed since the last step
        For async environments, keys is a list of keys currently held down
        """

    def run(self, record_dir=None):
        """
        Run the interactive window until the user quits
        """
        self._record_dir = record_dir
        if self._record_dir is not None:
            os.makedirs(self._record_dir, exist_ok=True)
            self._restart_recording()

        # pyglet.app.run() has issues like https://bitbucket.org/pyglet/pyglet/issues/199/attempting-to-resize-or-close-pyglet
        # and also involves inverting your code to run inside the pyglet framework
        # avoid both by using a while loop
        prev_frame_time = time.time()
        while True:
            self._win.switch_to()
            self._win.dispatch_events()
            now = time.time()
            self._update(now - prev_frame_time)
            prev_frame_time = now
            self._draw()
            self._win.flip()
