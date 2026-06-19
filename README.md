# Meerkat

Welcome! This is Meerkat, a bisection tool that utilizes PoC mutation.

For artifact verification (functional, reproducible) or just more in-depth docs, please see [this README](docs/README.md).
It will handle pretty much everything.

## Meerkat Paper

The results presented in "Meerkat: Pushing the Limits of Dynamic Bisection with PoC Mutation," can be found in `results/`.
There are more README files there as well to explain the results.

### Authors

 - Joseph Bursey  (<jbursey@uci.edu>)
 - Christoph Sendner
 - Ardalan Amiri Sani
 - Zhiyun Qian

## Setup

This string of commands should get you up and running:
```
# Install known dependencies
sudo apt update
sudo apt install make git gcc g++ ccache build-essential flex bison libncurses-dev libelf-dev libssl-dev dwarves libdw-dev qemu-system-x86 libmpfr-dev

# Make sure you can actually boot the VM
sudo usermod -aG kvm `whoami`

# Setup the compilers (download from our zenodo)
mkdir compilers
pushd compilers
wget "https://zenodo.org/records/20316001/files/compilers.tar.gz?download=1" -O compilers.tar.gz
tar -xzf compilers.tar.gz
popd

# The older compilers need libmpfr.so.4, but that library is depricated and no easily obtained.
# So, the solution is to point a symlink at the newer version, which seems to work just fine.
sudo ln -s /usr/lib/x86_64-linux-gnu/libmpfr.so.6 /usr/lib/x86_64-linux-gnu/libmpfr.so.4

# Verify that the compilers work
./verify-compilers.sh

# Setup the OS image
pushd image/stretch
sudo ./create_image.sh
popd

# Setup go (any recent version should work)
wget https://dl.google.com/go/go1.23.6.linux-amd64.tar.gz
tar -xf go1.23.6.linux-amd64.tar.gz
export PATH=`pwd`/go/bin:$PATH
go version

# Build Meerkat and Syzkaller
make clean
make syzkaller-clean
make all

# Run a minimal test
./mk-manager -s 1 -i 1 -b parse/example.csv
```

_A preemtive apology_: **SOMETHING** in Meerkat likes to mess up the tty settings, preventing you from seeing what you're typing on the command line after running Meerkat.
If this occurs for you, get a new console line with `ctrl+C` and then type `stty sane`.
I tried doing some debugging to figure out what is causing the issue, and I just don't know.
It's probably either git or qemu. Sorry.

### Syzkaller

Meerkat comes with the Syzkaller version it needs to perform bisection, which can be found in `syzkaller/`.
Go ahead and build it and make sure it works properly before running Meerkat.
Make sure to also install Go. Meerkat used Go 1.23.1 during testing, but a more recent version should also work.
Syzkaller will have excellent intructions on installing both [Syzkaller and Go](https://github.com/google/syzkaller/blob/master/docs/setup.md).

### Compilers

In order to have an accurate fuzzing environment for Syzkaller and the Linux kernel, Meerkat uses older gcc compilers for older versions of Linux.
This has the added benefit of fixing some compiler-time bugs that have otherwise plagued me. However, this means you need to download multiple versions of gcc.
Thankfully, the wonderful people over at Syzkaller have kept a log of all the compiler versions they have used, and they come pre-compiled!
For ease, we have uploaded these compilers to a separate [Zenodo repository](https://zenodo.org/records/20316001).
Apparently they were too big for Github.
Download them and untar them into `compilers/` so that the actual executable is found at `compilers/gcc-10.1.0/bin/gcc` (10.1.0 should be the version of gcc).

**Note**: Test out each compiler before you use it!
Some of them require old libraries.
You'll need to download them and put them in the right directory for your system.
Often times, gcc can use the more recent library, but you'll need to point a symbolic link at it.
(This is a best effort kind of tool. Just use what works on your system.)

### OS Image

Syzkaller has used a few different images in the past, but we only need one of them. Be sure to build `image/stretch/` by running `sudo ./create_image.sh`.
