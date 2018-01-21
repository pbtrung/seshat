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

curl -L https://download.libsodium.org/libsodium/releases/libsodium-1.0.16.tar.gz -o libsodium-1.0.16.tar.gz
tar xf libsodium-1.0.16.tar.gz
cd libsodium-1.0.16
./configure
make
sudo make install

curl -L https://github.com/facebook/zstd/archive/v1.3.3.tar.gz -o zstd-1.3.3.tar.gz
tar xf zstd-1.3.3.tar.gz
cd zstd-1.3.3
make
sudo make install
zstd --version