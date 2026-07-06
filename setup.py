import os
from setuptools import setup, Extension
import numpy

# Dynamically get the directory containing this setup.py
HERE = os.path.abspath(os.path.dirname(__file__))

def read(rel_path: str) -> str:
    with open(os.path.join(HERE, rel_path), 'r') as f:
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
    include_dirs=[numpy.get_include()],
    library_dirs=[HERE],  # Dynamically points to the current directory
    libraries=["alma"],
    extra_compile_args=compile_args,
)

setup(
    name="alma",
    # Passes a relative path; read() handles joining it with HERE
    version=get_version(os.path.join("alma_python", "__init__.py")),
    description="Python interface for alma",
    ext_modules=[alma_ext],
    package_dir={"": "alma_python"}, # Treat the contents of this folder as root
    py_modules=["alma_utils"],       # Explicitly expose this specific module
     setup_requires=["numpy"],
    zip_safe=False,
)
