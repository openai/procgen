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

    if platform.system() == "Windows":
        # using the installer seems to hang so use chocolatey instead
        run("choco install miniconda3 --version 4.7.12.1 --no-progress --yes")
        os.environ["PATH"] = "C:\\tools\\miniconda3;C:\\tools\\miniconda3\\Library\\bin;C:\\tools\\miniconda3\\Scripts;" + os.environ["PATH"]
    else:
        installer_urls = {
            "Linux": "https://repo.anaconda.com/miniconda/Miniconda2-4.7.12.1-Linux-x86_64.sh",
            "Darwin": "https://repo.anaconda.com/miniconda/Miniconda2-4.7.12.1-MacOSX-x86_64.sh",
        }
        installer_url = installer_urls[platform.system()]
        urlretrieve(
            installer_url,
            "miniconda-installer.sh",
        )
        conda_path = os.path.join(os.getcwd(), "miniconda")
        run(f"bash miniconda-installer.sh -b -p {conda_path}")
        os.environ["PATH"] = f"/{conda_path}/bin/:" + os.environ["PATH"]
    run("conda env update --name base --file environment.yml")
    run("conda init")
    run("pip install -e .[test]")
    run("""python -c "from procgen import ProcgenEnv; ProcgenEnv(num_envs=1, env_name='coinrun')" """)
    run("pytest --verbose --benchmark-disable --durations=16 .")


if __name__ == "__main__":
    main()