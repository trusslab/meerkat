#!/bin/bash

set -e

git pull --rebase
make clean
make all-meerkat -j`nproc`
