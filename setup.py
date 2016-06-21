from setuptools import setup, Extension

import os
from distutils.sysconfig import get_config_var

opt = get_config_var('OPT')
if opt:
    os.environ['OPT'] = " ".join(
        flag for flag in opt.split() if flag != '-Wstrict-prototypes'
    )

setup(
    # Name of this package
    name="ethereum-serpent",

    # Package version
    version='2.0.2',

    description='Serpent compiler',
    maintainer='Vitalik Buterin',
    maintainer_email='v@buterin.com',
    license='WTFPL',
    url='http://www.ethereum.org/',

    # Describes how to build the actual extension module from C source files.
    ext_modules=[
        Extension(
            'serpent_pyext',         # Python name of the module
            sources=['keccak-tiny.cpp', 'bignum.cpp', 'util.cpp',
                     'tokenize.cpp', 'lllparser.cpp', 'parser.cpp',
                     'functions.cpp', 'optimize.cpp', 'opcodes.cpp',
                     'rewriteutils.cpp', 'preprocess.cpp', 'rewriter.cpp',
                     'compiler.cpp', 'funcs.cpp', 'pyserpent.cpp'],
            extra_compile_args=['-Wno-sign-compare']
        )],
    py_modules=[
        'serpent',
        'pyserpent'
    ],
    scripts=[
        'serpent.py'
    ],
    entry_points={
        'console_scripts': [
            'serpent = serpent:main',
        ],
    }
)
