#!/bin/bash

clean=0
h=$1
id=1
repo=torvalds/linux.git
pref=linux
kern="$pref-$h"
home=`pwd`

cd $home/wd-inspector-$id/kernels/
if (( $clean == 1 )); then
    rm -r *
fi
wget https://git.kernel.org/pub/scm/linux/kernel/git/$repo/snapshot/$kern.tar.gz
tar -xf $kern.tar.gz
rm $kern.tar.gz
cd $kern
cp ../../config-bug1.txt .config
make -f Makefile olddefconfig
make -f Makefile -j72

cd $home
