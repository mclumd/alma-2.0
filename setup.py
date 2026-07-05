import os
from setuptools import setup, Extension
import numpy

def read(rel_path: str) -> str:
    here = os.path.abspath(os.path.dirname(__file__))
    with open(os.path.join(here, rel_path), 'r') as f:
        return f.read()

def get_version(rel_path: str) -> str:
    for line in read(rel_path).splitlines():
        if line.startswith("__version__"):
            delim = '"' if '"' in line else "'"
            return line.split(delim)[1]
    raise RuntimeError("Unable to find version string.")

compile_args = [
    "-std=c11",
    "-pedantic-errors",
    "-Wall",
    "-Wshadow",
    "-Wpedantic",
    "-g",
    "-fPIC",
    "-O0",
]

alma_ext = Extension(
    "alma",
    sources=["almamodule.c"],
    include_dirs=[numpy.get_include()],        # use numpy.get_include() rather than a hard path
    library_dirs=["/home/justin/alma-2.0"],
    libraries=["alma"],
    extra_compile_args=compile_args,
)

setup(
    name="alma",
    # either hard-code the version or read it dynamically:
    version=get_version("/home/justin/alma-2.0/alma_python/__init__.py"),
    description="Python interface for alma",
    ext_modules=[alma_ext],
    setup_requires=["numpy"],  # ensure numpy is available for include_dirs
    zip_safe=False,
)
