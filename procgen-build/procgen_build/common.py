import subprocess as sp
import time


GCS_BUCKET = "openai-procgen"


def run(cmd, **kwargs):
    print(f"RUN: {cmd}")
    start = time.time()
    p = sp.run(cmd, shell=True, encoding="utf8", **kwargs)
    print(f"ELAPSED: {time.time() - start}")
    if p.returncode != 0:
        print(f"cmd {cmd} failed")
        if p.stdout is not None:
            print(p.stdout[-100000:])
        raise Exception(f"command {cmd} failed")