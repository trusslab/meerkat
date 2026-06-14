#! /bin/bash

# Joseph Bursey <jbursey@uci.edu>

# This script will fetch the necessary dependencies and build Meerkat

set -e

self=${0}
user=$(whoami)

log() {
    echo "${self}: $@"
}

log "Hello! I need root privileges to set up a few things:"
log "First, pulling the right dependencies."
log "Second, adding ${user} to 'kvm'."
log "Last, creating a simple debian image."

sudo apt update
sudo apt install -y make git gcc g++ ccache build-essential flex bison libncurses-dev libelf-dev libssl-dev dwarves libdw-dev qemu-system-x86

# Make sure you can actually boot the VM
sudo usermod -aG kvm ${user}

# Setup the compilers (download from our zenodo)
if [ ! -d compilers ]; then
    mkdir compilers
fi
pushd compilers
if [ ! -f compilers.tar.gz ]; then
    log "Pulling compilers"
    wget "https://zenodo.org/records/20316001/files/compilers.tar.gz?download=1" -O compilers.tar.gz
fi
if [ ! -d gcc-10.1.0 ]; then
    log "Unpacking Compilers"
    tar -xzf compilers.tar.gz
fi
popd
./verify-compilers.sh


# Setup the OS image
pushd image/stretch
if [ ! -f stretch.img ]; then
    log "Creating Debian image"
    sudo ./create_image.sh
    sudo chown -R ${user}:${user} ./*
fi
popd

# Setup go (any recent version should work)
which go > /dev/null 2> /dev/null
if (( $? != 0 )); then
    wget https://dl.google.com/go/go1.23.6.linux-amd64.tar.gz
    tar -xf go1.23.6.linux-amd64.tar.gz
    export PATH=`pwd`/go/bin:$PATH
    go version
fi

# Build Meerkat and Syzkaller
make clean
set +e
make syzkaller-clean
set -e
make all -j`nproc`

