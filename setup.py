from distutils.core import setup, Extension
import os

cwd = os.getcwd()

#compile_args = ['-std=c11', '-pedantic-errors', '-Wall', '-Werror', '-Wshadow', '-Wpedantic', '-g', '-fPIC']
compile_args = ['-std=c11', '-pedantic-errors', '-Wall', '-Wshadow', '-Wpedantic', '-g', '-fPIC', '-O0']

def main():
    setup(name="alma",
          version="2.0.0",
          description="Python interface for the alma C library",
          ext_modules=[Extension("alma",
                                 sources = ["almamodule.c"],
                                 include_dirs = ["/usr/lib/python2.7/dist-packages/numpy/core/include/numpy"],
#                                 include_dirs = ["alma_command.h",
#                                  "alma_kb.h",
#                                  "alma_print.h"],
                                 extra_compile_args=compile_args,
                                 library_dirs=["/home/justin/alma-2.0"],
#                                 library_dirs=["/usr/local/lib"],
                                 libraries=["alma"])])

if __name__ == "__main__":
    main()
