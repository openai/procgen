import threading
import os
import contextlib
import subprocess as sp
import shutil
import json
import sys
import platform
import multiprocessing as mp

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


global_build_lock = threading.Lock()
global_builds = set()


@contextlib.contextmanager
def nullcontext():
    # this is here for python 3.6 support
    yield


@contextlib.contextmanager
def chdir(newdir):
    curdir = os.getcwd()
    try:
        os.chdir(newdir)
        yield
    finally:
        os.chdir(curdir)


def run(cmd):
    return sp.run(cmd, stdout=sp.PIPE, stderr=sp.STDOUT, encoding="utf8")


def check(proc, verbose):
    if proc.returncode != 0:
        print(f"RUN FAILED {proc.args}:\n{proc.stdout}")
        raise Exception("failed to build procgen from source")
    if verbose:
        print(f"RUN {proc.args}:\n{proc.stdout}")


def build(package=False, debug=False):
    """
    Build the requested environment in a process-safe manner and only once per process.
    """
    build_dir = os.path.join(SCRIPT_DIR, ".build")
    os.makedirs(build_dir, exist_ok=True)

    build_type = "relwithdebinfo"
    if debug:
        build_type = "debug"

    if "MAKEFLAGS" not in os.environ:
        os.environ["MAKEFLAGS"] = f"-j{mp.cpu_count()}"

    if "PROCGEN_CMAKE_PREFIX_PATH" in os.environ:
        cmake_prefix_paths = [os.environ["PROCGEN_CMAKE_PREFIX_PATH"]]
    else:
        # guess some common qt cmake paths, it's unclear why cmake can't find qt without this
        cmake_prefix_paths = ["/usr/local/opt/qt5/lib/cmake"]
        conda_exe = shutil.which("conda")
        if conda_exe is not None:
            conda_info = json.loads(
                sp.run(["conda", "info", "--json"], stdout=sp.PIPE).stdout
            )
            conda_prefix = conda_info["active_prefix"]
            if conda_prefix is None:
                conda_prefix = conda_info["conda_prefix"]
            if platform.system() == "Windows":
                conda_prefix = os.path.join(conda_prefix, "library")
            conda_cmake_path = os.path.join(conda_prefix, "lib", "cmake", "Qt5")
            # prepend this qt since it's likely to be loaded already by the python process
            cmake_prefix_paths.insert(0, conda_cmake_path)

    with chdir(build_dir), global_build_lock:
        # check if we have built yet in this process
        if build_type not in global_builds:
            if package:
                # avoid the filelock dependency when building from setup.py
                lock_ctx = nullcontext()
            else:
                # prevent multiple processes from trying to build at the same time
                import filelock
                lock_ctx = filelock.FileLock(".build-lock")
            with lock_ctx:
                os.makedirs(build_type, exist_ok=True)
                with chdir(build_type):
                    sys.stdout.write("building procgen...")
                    sys.stdout.flush()
                    generator = "Unix Makefiles"
                    if platform.system() == "Windows":
                        generator = "Visual Studio 15 2017 Win64"
                    configure_cmd = [
                        "cmake",
                        "-G", generator,
                        "-DCMAKE_PREFIX_PATH=" + ";".join(cmake_prefix_paths),
                        "../..",
                    ]
                    if package:
                        configure_cmd.append("-DPROCGEN_PACKAGE=ON")
                    if platform.system() != "Windows":
                        # this is not used on windows, the option needs to be passed to cmake --build instead
                        configure_cmd.append(f"-DCMAKE_BUILD_TYPE={build_type}")

                    p = run(configure_cmd)
                    if "CMakeCache.txt is different" in p.stdout:
                        # if the folder is moved we can end up with an invalid CMakeCache.txt
                        # in which case we should re-run configure
                        os.remove("CMakeCache.txt")
                        check(run(configure_cmd), verbose=package)
                    else:
                        check(p, verbose=package)

                    build_cmd = ["cmake", "--build", ".", "--config", build_type]
                    check(run(build_cmd), verbose=package)
                    print("done")

            global_builds.add(build_type)

    lib_dir = os.path.join(build_dir, build_type)
    if platform.system() == "Windows":
        # the built library is in a different location on windows
        lib_dir = os.path.join(lib_dir, build_type)
    return lib_dir
