from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext
import subprocess
import os
import sys
import glob

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PACKAGE_ROOT = os.path.join(SCRIPT_DIR, "procgen")
README = open(os.path.join(SCRIPT_DIR, "README.md"), "rb").read().decode("utf8")

# dynamically determine version number based on git commit
version = open(os.path.join(PACKAGE_ROOT, "version.txt"), "r").read().strip()
sha = "unknown"

try:
    sha = (
        subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=SCRIPT_DIR)
        .decode("ascii")
        .strip()
    )
except Exception:
    pass

if os.environ.get("TRAVIS_TAG", "") != "":
    tag = os.environ["TRAVIS_TAG"]
    assert tag == version, "mismatch in tag vs version, expected: %s actual: %s" % (
        tag,
        version,
    )
elif sha != "unknown":
    version += "+" + sha[:7]


# build shared library
class DummyExtension(Extension):
    """
    This causes build_ext to be run
    """

    def __init__(self):
        Extension.__init__(self, "dummy", sources=[])


class custom_build_ext(build_ext):
    """
    Run our custom build step
    """

    def run(self):
        if self.inplace:
            print("skipping inplace build, extension will be built on demand")
            return
        sys.path.append(PACKAGE_ROOT)
        import build

        lib_dir = build.build(package=True)
        # move into the build_lib directory so that the shared library
        # can be included in the package
        # we will also check for this file at runtime to avoid doing
        # the on-demand build
        for filename in ["libenv.so", "libenv.dylib", "env.dll"]:
            src = os.path.join(lib_dir, filename)
            dst = os.path.join(self.build_lib, "procgen", "data", "prebuilt", filename)
            if os.path.exists(src):
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                os.replace(src, dst)


# package_data ** globs appear to be broken, supply the asset paths manually instead
asset_paths = glob.glob(os.path.join(PACKAGE_ROOT, "data", "**"), recursive=True)
asset_relpaths = [os.path.relpath(path, PACKAGE_ROOT) for path in asset_paths]

setup(
    name="procgen",
    packages=find_packages(),
    version=version,
    install_requires=[
        "numpy>=1.17.0,<2.0.0",
        "gym>=0.15.0,<1.0.0",
        "gym3>=0.3.3,<1.0.0",
        "filelock>=3.0.0,<4.0.0",
    ],
    python_requires=">=3.6.0",
    package_data={
        "procgen": [
            "version.txt",
            *asset_relpaths,
        ]
    },
    extras_require={"test": ["pytest==5.2.1", "pytest-benchmark==3.2.2"]},
    ext_modules=[DummyExtension()],
    cmdclass={"build_ext": custom_build_ext},

    author="OpenAI",
    description="Procedurally Generated Game-Like RL Environments",
    long_description=README,
    long_description_content_type="text/markdown",
    url="https://github.com/openai/procgen",
)
