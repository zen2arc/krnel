#!/bin/bash
set -e

make
make iso
make disk
make run