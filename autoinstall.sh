#!/bin/bash
set -e

git clone https://github.com/zen2arc/kTTY.git
cd kTTY
make
make iso
make disk
make run