"""A setuptools based setup module.

See:
https://packaging.python.org/en/latest/distributing.html
https://github.com/pypa/sampleproject
"""

# Always prefer setuptools over distutils
from setuptools import setup, find_packages

# To use a consistent encoding
from os import path
import os
import sys

here = path.abspath(path.dirname(__file__))
long_description = """
Control program (and module) for iotsa devices. Allows finding of iotsa devices on the local
WiFi network or in the physical vicinity, inspecting and changing configuration
of those devices and uploading new firmware over the air.
"""

# Get the version number from the iotsa module
versionInfo = {}
with open(os.path.join(here, "iotsa/version.py")) as fp:
    exec(fp.read(), versionInfo)


VERSION = versionInfo["__version__"]

setup(
    name="iotsa",
    # Versions should comply with PEP440.  For a discussion on single-sourcing
    # the version across setup.py and the project code, see
    # https://packaging.python.org/en/latest/single_source_version.html
    version=VERSION,
    description="Control iotsa devices",
    long_description=long_description,
    # The project's main homepage.
    url="http://www.cwi.nl",
    # Author details
    author="Jack Jansen",
    author_email="Jack.Jansen@cwi.nl",
    # Choose your license
    license="MIT",
    # See https://pypi.python.org/pypi?%3Aaction=list_classifiers
    classifiers=[
        # How mature is this project? Common values are
        #   3 - Alpha
        #   4 - Beta
        #   5 - Production/Stable
        "Development Status :: 3 - Alpha",
        # Indicate who your project is intended for
        "Intended Audience :: End Users/Desktop",
        "Intended Audience :: Developers",
        "Topic :: Communications",
        "Topic :: Home Automation",
        "Topic :: Internet"
        # Pick your license as you wish (should match "license" above)
        "License :: OSI Approved :: MIT License",
        # Specify the Python versions you support here. In particular, ensure
        # that you indicate whether you support Python 2, Python 3 or both.
        "Programming Language :: Python :: 3",
    ],
    # What does your project relate to?
    # keywords='sample setuptools development',
    # You can just specify the packages manually here if your project is
    # simple. Or you can use find_packages().
    packages=find_packages(),
    # List run-time dependencies here.  These will be installed by pip when
    # your project is installed. For an analysis of "install_requires" vs pip's
    # requirements files see:
    # https://packaging.python.org/en/latest/requirements.html
    install_requires=[
        "requests", 
        "esptool", 
        "zeroconf", 
        "CoAPThon3 @ git+https://github.com/rgerganov/CoAPthon3", 
        "bleaktyped @ git+https://github.com/jackjansen/bleaktyped"],
    # List additional groups of dependencies here (e.g. development
    # dependencies). You can install these using the following syntax,
    # for example:
    # $ pip install -e .[dev,test]
    # extras_require={
    #    'dev': ['check-manifest'],
    #    'test': ['coverage'],
    # },
    # If there are data files included in your packages that need to be
    # installed, specify them here.  If using Python 2.6 or less, then these
    # have to be included in MANIFEST.in as well.
    # package_data=package_data,
    # include_package_data=True,
    # Although 'package_data' is the preferred approach, in some case you may
    # need to place data files outside of your packages. See:
    # http://docs.python.org/3.4/distutils/setupscript.html#installing-additional-files # noqa
    # In this case, 'data_file' will be installed into '<sys.prefix>/my_data'
    # data_files=[('my_data', ['data/data_file'])],
    # To provide executable scripts, use entry points in preference to the
    # "scripts" keyword. Entry points provide cross-platform support and allow
    # pip to create the appropriate form of executable for the target platform.
    entry_points={
        "console_scripts": [
            "iotsa=iotsa.__main__:main",
        ],
    },
)
