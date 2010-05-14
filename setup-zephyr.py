#!/usr/bin/env python
from setuptools import setup
from distutils.extension import Extension
from Pyrex.Distutils import build_ext

setup(name="Zephyr",
	version="6.3",
	url="https://www.as.cmu.edu/~geek/Zephyr.html",
	author="Brian E. Gallew",
	author_email="geek+python@cmu.edu",
	description="Python bindings for Zephyr, including a GUI client",
	#data_files=[('bin',['pyzwgc',],),],
	#packages=["Zephyr"],
	#ext_package="Zephyr",
	ext_modules=[Extension(
		"Zephyr",
		["Zephyr.c"],
		libraries=["zephyr", "krb5", "des", "com_err", "crypt"]
		)],
    cmdclass= {"build_ext": build_ext}
)
