import subprocess as sp
from urllib.request import urlretrieve
import os
import platform

from .common import run


def apt_install(packages, recommends=False):
    os.environ["DEBIAN_FRONTEND"] = "noninteractive"
    sp.run(["sudo", "apt-get", "update"], check=True)
    cmd = ["sudo", "apt-get", "install", "--yes"]
    if not recommends:
        cmd += ["--no-install-recommends"]
    sp.run(cmd + list(packages), check=True)


def main():
    if platform.system() == "Linux":
        apt_install(["mesa-common-dev"])

    installer_urls = {
        "Linux": "https://repo.anaconda.com/miniconda/Miniconda3-py37_4.9.2-Linux-x86_64.sh",
        "Darwin": "https://repo.anaconda.com/miniconda/Miniconda3-py37_4.9.2-MacOSX-x86_64.sh",
        "Windows": "https://repo.anaconda.com/miniconda/Miniconda3-py37_4.9.2-Windows-x86_64.exe",
    }
    installer_url = installer_urls[platform.system()]
    urlretrieve(
        installer_url,
        "miniconda-installer.exe" if platform.system() == "Windows" else "miniconda-installer.sh",
    )
    if platform.system() == "Windows":
        run("miniconda-installer.exe /S /D=c:\\miniconda3")
        os.environ["PATH"] = "C:\\miniconda3;C:\\miniconda3\\Library\\bin;C:\\miniconda3\\Scripts;" + os.environ["PATH"]
    else:
        conda_path = os.path.join(os.getcwd(), "miniconda")
        run(f"bash miniconda-installer.sh -b -p {conda_path}")
        os.environ["PATH"] = f"/{conda_path}/bin/:" + os.environ["PATH"]

    def run_in_conda_env(cmd):
        run(f"conda run --name dev {cmd}", shell=False)

    run("conda env update --name dev --file environment.yml")
    run_in_conda_env("pip show gym3")
    run_in_conda_env("pip install -e .[test]")
    run_in_conda_env("""python -c "from procgen import ProcgenGym3Env; ProcgenGym3Env(num=1, env_name='coinrun')" """)
    run_in_conda_env("pytest --verbose --benchmark-disable --durations=16 .")


if __name__ == "__main__":
    main()
