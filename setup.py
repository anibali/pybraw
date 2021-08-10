#!/usr/bin/env python3

from pybind11.setup_helpers import build_ext, Pybind11Extension
from setuptools import setup, find_packages


ext_modules = [
    Pybind11Extension(
        'pybraw._pybraw',
        sources=['ext/pybraw_ext.cpp'],
        include_dirs=['vendor/linux/include'],
        libraries=['BlackmagicRawAPI'],
    ),
]

setup(
    name='pybraw',
    version='2.1.0',
    packages=find_packages('src'),
    package_dir={'': 'src'},
    include_package_data=True,
    author='Aiden Nibali',
    description='Python bindings for the Blackmagic RAW SDK',
    license='MIT',
    long_description=open('README.md').read(),
    long_description_content_type='text/markdown',
    zip_safe=False,
    test_suite='tests',
    ext_modules=ext_modules,
    cmdclass={'build_ext': build_ext},
    install_requires=['numpy'],
    classifiers=[
        'Topic :: Multimedia :: Video',
        'License :: OSI Approved :: MIT License',
    ]
)