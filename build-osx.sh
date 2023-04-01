#!/bin/bash 

set -x


# MAJOR POST

# https://github.com/cjcliffe/CubicSDR/issues/958

brew install fftw hamlib freeglut gtk+3 

export HOME=/Users/kalou/dev/CubicSDR
mkdir -p build

#. travis-ci/build_soapysdr.sh 2>&1 | tee soapysdr.log

. travis-ci/build_mirisdr.sh 2>&1 | tee mirisdr.log
#sudo ln -s /opt/homebrew/lib/libusb* /usr/local/lib/

. travis-ci/build_cubicsdr.sh 2>&1 | tee cubicsdr.log

set +ex
