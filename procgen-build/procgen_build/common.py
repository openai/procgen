import subprocess as sp
import time
import shlex


def run(cmd, shell=True, **kwargs):
    print(f"RUN: {cmd}")
    start = time.time()
    if not shell:
        cmd = shlex.split(cmd)
    p = sp.run(cmd, shell=shell, encoding="utf8", **kwargs)
    print(f"ELAPSED: {time.time() - start}")
    if p.returncode != 0:
        print(f"cmd {cmd} failed")
        if p.stdout is not None:
            print(p.stdout[-100000:])
        raise Exception(f"command {cmd} failed")
