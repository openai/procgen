"""
Upload wheels from GCS to pypi using twine

python -m pip install --upgrade twine
"""

import argparse
import subprocess
import tempfile
import blobfile as bf


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--for-real", action="store_true")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmpdir:
        for filepath in bf.glob(f"gs://openai-procgen/builds/procgen-{args.version}-*.whl"):
            print(filepath)
            bf.copy(filepath, bf.join(tmpdir, bf.basename(filepath)))
        if args.for_real:
            options = []
        else:
            options = ["--repository-url", "https://test.pypi.org/legacy/"]
        subprocess.run(["python", "-m", "twine", "upload", *options, *bf.glob(bf.join(tmpdir, "*.whl"))], check=True)

    

if __name__ == "__main__":
    main()
