from setuptools import setup, find_packages

setup(
    name="procgen_build",
    packages=find_packages(),
    version="0.0.1",
    install_requires=[
        # rather than rely on system cmake, install it here
        "cmake==3.21.3",
        # this is required by procgen/build.py
        "gym3==0.3.0",
    ],
)
 