import platform
from urllib.request import urlretrieve
import os
import subprocess as sp
import fnmatch

# import blobfile as bf

from .common import run


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


# https://stackoverflow.com/a/50135504
def init_vsvars():
    print("Initializing environment for Visual Studio")

    vcvars_path = "C:/Program Files (x86)/Microsoft Visual Studio/2017/BuildTools/VC/Auxiliary/Build/vcvars64.bat"

    env_bat_file_path = "setup_build_environment_temp.bat"
    env_txt_file_path = "build_environment_temp.txt"
    with open(env_bat_file_path, "w") as env_bat_file:
        env_bat_file.write('call "%s"\n' % vcvars_path)
        env_bat_file.write("set > %s\n" % env_txt_file_path)

    os.system(env_bat_file_path)
    with open(env_txt_file_path, "r") as env_txt_file:
        lines = env_txt_file.read().splitlines()

    os.remove(env_bat_file_path)
    os.remove(env_txt_file_path)
    for line in lines:
        if "=" not in line:
            print(f"invalid line {repr(line)}")
            continue
        k, v = line.split("=", 1)
        os.environ[k] = v


def get_var(pattern):
    for key, value in os.environ:
        if fnmatch.fnmatch(key, pattern):
            return os.environ[key]
    return None


def main():
    os.environ.update(
        {
            "CIBW_BUILD": "cp36-macosx_x86_64 cp37-macosx_x86_64 cp38-macosx_x86_64 cp36-manylinux_x86_64 cp37-manylinux_x86_64 cp38-manylinux_x86_64 cp36-win_amd64 cp37-win_amd64 cp38-win_amd64",
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
    if platform.system() == "Darwin":
        # cibuildwheel's python copy on mac os x sometimes fails with this error:
        # [SSL: CERTIFICATE_VERIFY_FAILED] certificate verify failed: unable to get local issuer certificate (_ssl.c:1076)
        urlretrieve(
            "https://curl.haxx.se/ca/cacert.pem",
            os.environ["TRAVIS_BUILD_DIR"] + "/cacert.pem",
        )
        os.environ["SSL_CERT_FILE"] = os.environ["TRAVIS_BUILD_DIR"] + "/cacert.pem"
    elif platform.system() == "Linux":
        if "TRAVIS_TAG" in os.environ:
            # pass TRAVIS_TAG to the container so that it can build wheels with the correct version number
            os.environ["CIBW_ENVIRONMENT"] = (
                os.environ["CIBW_ENVIRONMENT"]
                + " TRAVIS_TAG=" + os.environ["TRAVIS_TAG"]
            )
        os.environ["CIBW_ENVIRONMENT"] = (
            os.environ["CIBW_ENVIRONMENT"]
            + " INSIDE_DOCKER=1"
        )
    elif platform.system() == "Windows":
        init_vsvars()

    run("pip install cibuildwheel==1.4.1")
    run("cibuildwheel --output-dir wheelhouse")

    # if have_credentials:
    #     print("upload wheels", platform.system())
    #     input_dir = "wheelhouse"
    #     output_dir = f"gs://{GCS_BUCKET}/builds/"
    #     for filename in bf.listdir(input_dir):
    #         src = bf.join(input_dir, filename)
    #         dst = bf.join(output_dir, filename)
    #         print(src, "=>", dst)
    #         bf.copy(src, dst, overwrite=True)


if __name__ == "__main__":
    main()
