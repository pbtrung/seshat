#!/bin/sh

mkdir tmp
curl -L https://bootstrap.pypa.io/get-pip.py -o tmp/get-pip.py
python3.6 tmp/get-pip.py --user
export PATH=$PATH:~/.local/bin
pip install --user meson

curl -L https://github.com/ninja-build/ninja/releases/download/v1.8.2/ninja-linux.zip -o tmp/ninja-linux.zip
unzip tmp/ninja-linux.zip -d tmp
export PATH=$PATH:$PWD/tmp
ninja --version