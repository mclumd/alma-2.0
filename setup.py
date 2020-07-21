from distutils.core import setup, Extension
import os

cwd = os.getcwd()

def main():
    setup(name="alma",
          version="2.0.0",
          description="Python interface for the alma C library",
          ext_modules=[Extension("alma",
                                 sources = ["almamodule.c"],
#                                 include_dirs = ["alma_command.h",
#                                  "alma_kb.h",
#                                  "alma_print.h"],
                                 library_dirs=["/usr/local/lib"],
                                 libraries=["alma"])])

if __name__ == "__main__":
    main()
