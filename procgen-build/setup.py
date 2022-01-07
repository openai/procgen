from setuptools import setup, find_packages

setup(
    name="procgen_build",
    packages=find_packages(),
    version="0.0.1",
    install_requires=[
        # rather than rely on system cmake, install it here
        "cmake==3.15.3",
        # this is required by procgen/build.py
        "pillow==8.2.0",  # this is required by gym3, newer versions fail to install on linux cibuildwheel with a missing dependency error
        "gym3==0.3.0",
    ],
)
