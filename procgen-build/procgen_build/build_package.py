import platform
import os

from .common import run


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def main():
    os.environ.update(
        {
            "CIBW_BUILD": "cp37-macosx_x86_64 cp38-macosx_x86_64 cp39-macosx_x86_64 cp310-macosx_x86_64 cp37-manylinux_x86_64 cp38-manylinux_x86_64 cp39-manylinux_x86_64 cp310-manylinux_x86_64 cp37-win_amd64 cp38-win_amd64 cp39-win_amd64 cp310-win_amd64",
            "CIBW_BEFORE_BUILD": "pip install -r procgen-build/requirements.txt && pip install -e procgen-build && python -u -m procgen_build.build_qt --output-dir /tmp/qt5",
            "CIBW_TEST_EXTRAS": "test",
            "CIBW_BEFORE_TEST": "pip install -r procgen-build/requirements.txt",
            # the --pyargs option causes pytest to use the installed procgen wheel
            "CIBW_TEST_COMMAND": "pytest --verbose --benchmark-disable --durations=16 --pyargs procgen",
            # this is where build-qt.py will put the files
            "CIBW_ENVIRONMENT": "PROCGEN_CMAKE_PREFIX_PATH=/tmp/qt5/qt/build/qtbase/lib/cmake/Qt5",
            # this is a bit too verbose normally
            # "CIBW_BUILD_VERBOSITY": "3",
        }
    )
    if platform.system() == "Linux":
        if "GITHUB_REF" in os.environ:
            # pass TRAVIS_TAG to the container so that it can build wheels with the correct version number
            os.environ["CIBW_ENVIRONMENT"] = (
                os.environ["CIBW_ENVIRONMENT"]
                + " GITHUB_REF=" + os.environ["GITHUB_REF"]
            )
        os.environ["CIBW_ENVIRONMENT"] = (
            os.environ["CIBW_ENVIRONMENT"]
            + f" CACHE_DIR=/host{os.getcwd()}/cache"
        )
    else:
        os.environ["CACHE_DIR"] = os.path.join(os.getcwd(), "cache")

    run("pip install cibuildwheel==2.3.1")
    run("cibuildwheel --output-dir wheelhouse")


if __name__ == "__main__":
    main()
