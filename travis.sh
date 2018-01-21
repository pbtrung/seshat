#!/bin/sh

mkdir tmp
curl -L https://bootstrap.pypa.io/get-pip.py -o tmp/get-pip.py
python3.6 tmp/get-pip.py --user
pip -V