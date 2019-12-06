import subprocess as sp
import multiprocessing as mp
import os
import time
import tarfile
import argparse
import threading
import platform
import shutil
import hashlib

import blobfile as bf

from .common import run, GCS_BUCKET

BUILD_VERSION = 11


def cache_folder(name, dirpath, options, build_fn):
    if os.path.exists(dirpath):
        print(f"cache for {name} found locally")
        return
    
    options_hash = hashlib.md5("|".join(options).encode("utf8")).hexdigest()
    cache_path = bf.join(f"gs://{GCS_BUCKET}", "cache", f"{name}-{options_hash}.tar")
    if "GOOGLE_APPLICATION_CREDENTIALS" not in os.environ:
        # we don't have any credentials to do the caching, always build in this case
        print(f"building without cache for {name}")
        start = time.time()
        build_fn()
        print(f"build elapsed {time.time() - start}")
    elif bf.exists(cache_path):
        print(f"downloading cache for {name}: {cache_path}")
        start = time.time()
        with bf.BlobFile(cache_path, "rb") as f:
            with tarfile.open(fileobj=f, mode="r") as tf:
                tf.extractall()
        print(f"download elapsed {time.time() - start}")
    else:
        print(f"building cache for {name}")
        start = time.time()
        build_fn()
        print(f"cache build elapsed {time.time() - start}")
        print(f"uploading cache for {name}")
        start = time.time()
        if not bf.exists(cache_path):
            with bf.BlobFile(cache_path, "wb") as f:
                with tarfile.open(fileobj=f, mode="w") as tf:
                    tf.add(dirpath)
        print(f"upload elapsed {time.time() - start}")

# workaround for timeout error
# https://docs.travis-ci.com/user/common-build-problems/#build-times-out-because-no-output-was-received
# since we may be running inside a docker image without the travis_wait command, do this manually
def no_timeout_worker():
    while True:
        time.sleep(60)
        print(".")


def build_qt(output_dir):
    no_timeout_thread = threading.Thread(target=no_timeout_worker, daemon=True)
    no_timeout_thread.start()

    qt_version = "5.13.2"
    os.makedirs(output_dir, exist_ok=True)
    os.chdir(output_dir)
    os.makedirs("qt", exist_ok=True)
    os.chdir("qt")

    modules = ["qtbase"]

    def download_source():
        run("git clone https://code.qt.io/qt/qt5.git")
        os.chdir("qt5")
        run(f"git checkout v{qt_version}")
        run("perl init-repository --module-subset=" + ",".join(modules))
        os.chdir("..")

    # downloading the source from git takes 25 minutes on travis
    # so cache the source so we don't have to use git
    cache_folder("qt-source", dirpath="qt5", options=[qt_version, platform.system()] + modules, build_fn=download_source)

    qt_options = [
        "-confirm-license",
        "-static",
        "-release",
        # -qtnamespace should in theory reduce the likelihood of symbol conflicts
        "-qtnamespace",
        "ProcGenQt",
        "-opensource",
        "-nomake",
        "examples",
        "-nomake",
        "tests",
        "-nomake",
        "tools",
        # travis mac os x server does not seem to support avx2
        "-no-avx2",
        "-no-avx512",
        # extra stuff we don't need
        "-no-pch",
        "-no-harfbuzz",
        "-no-openssl",
        "-no-dbus",
        "-no-opengl",
        "-no-xcb",
        "-no-libjpeg",
        "-no-ico",
        "-no-gif",
        # useful for profiling
        # "-force-debug-info",
    ]
    if platform.system() == "Windows":
        # parallelize the windows build
        qt_options.append("-mp")
    
    def compile_qt():
        os.makedirs("build")
        os.chdir("build")
        if platform.system() == "Windows":
            qt_configure = "..\\qt5\\configure"
        else:
            qt_configure = "../qt5/configure"
        run(f"{qt_configure} -prefix {os.getcwd()}/qtbase " + " ".join(qt_options))
        if platform.system() == "Windows":
            run("nmake", stdout=sp.PIPE, stderr=sp.STDOUT)
        else:
            run(f"make -j{mp.cpu_count()}", stdout=sp.PIPE, stderr=sp.STDOUT)
        os.chdir("..")
        run("du -hsc build")
        for root, dirs, files in os.walk("."):
            for dirname in dirs:
                if dirname in (".obj", ".pch"):
                    dirpath = os.path.join(root, dirname)
                    print(f"remove dir {dirpath}")
                    shutil.rmtree(dirpath)
        run("du -hsc build")

    cache_folder("qt-build", dirpath="build", options=[platform.system(), os.environ.get("TRAVIS_OSX_IMAGE", "")] + qt_options, build_fn=compile_qt)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    build_qt(args.output_dir)


if __name__ == "__main__":
    main()
