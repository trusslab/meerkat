# Meerkat

Welcome! This is SyzInspector, a system built to find the revealing factors of bugs found by Syzbot.

First things first, this project was built on Ubuntu 20.04 with the 5.15.0-107-generic kernel. It should run on more recent machines, but is untested.

## Syzkaller

Meerkat comes with the Syzkaller version it needs to perform bisection, which can be found in `syzkaller/`. Go ahead and build it and make sure it works properly before running Meerkat. Make sure to also install Go. Meerkat used Go 1.23.1 during testing. Syzkaller will have excellent intructions on installing both Syzkaller and Go.

## Compilers

In order to have an accurate fuzzing environment for Syzkaller and the Linux kernel, Meerkat uses older gcc compilers for older versions of Linux. This has the added benefit of fixing some compiler-time bugs that have otherwise plagued me. However, this means you need to download multiple versions of gcc. Thankfully, the wonderful people over at Syzkaller have kept a log of all the compiler versions they have used, and they come pre-compiled! The tars for all of the compilers can be found over on the Syzkaller github. Download them and untar them into `compilers/` so that the actual executable is found at `compilers/gcc-7.0.0/bin/gcc` (7.0.0 should be the version of gcc).

**Note**: Test out each compiler before you use it! Some of them require old libraries. You'll need to download them and put them in the right directory for your system. Often times, gcc can use the more recent library, but you'll need to point a symbolic link at it.

## OS Image

Syzkaller has used a few different images in the past, but we only need one of them. 

**Note**: Syzkaller boots these images in read-only mode so that it can run multiple instances off the same image. While necessary, it can be a pain when you compile Linux to run with SELinux (or some other option that requires a persistent change to the image). To fix this, boot the image manually with the config you want to use, then Syzkaller should work.

## Configuration

...

## Patches

**Do Not Touch the Patch Files**

They are specific to some old versions of Linux and Syzkaller. There should not be a reason to change them unless you want to do your own debugging.
Meerkat did not use these patches during testing, but they can be turned on as a feature.

## Meerkat Paper

The results presented in "Meerkat: Leveraging Fuzzing for Robust Bisection in a Changing Kernel," can be found in `results/`. There are more README files there as well to explain the results.
