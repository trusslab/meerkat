# Meerkat

Welcome! This is Meerkat, a bisection tool that utilizes PoC mutation.

## Meerkat Paper

The results presented in "Meerkat: Leveraging Fuzzing for Robust Bisection in a Changing Kernel," can be found in `results/`. There are more README files there as well to explain the results.

## Syzkaller

Meerkat comes with the Syzkaller version it needs to perform bisection, which can be found in `syzkaller/`. Go ahead and build it and make sure it works properly before running Meerkat. Make sure to also install Go. Meerkat used Go 1.23.1 during testing. Syzkaller will have excellent intructions on installing both Syzkaller and Go.

## Compilers

In order to have an accurate fuzzing environment for Syzkaller and the Linux kernel, Meerkat uses older gcc compilers for older versions of Linux. This has the added benefit of fixing some compiler-time bugs that have otherwise plagued me. However, this means you need to download multiple versions of gcc. Thankfully, the wonderful people over at Syzkaller have kept a log of all the compiler versions they have used, and they come pre-compiled! The tars for all of the compilers can be found over on the Syzkaller github. Download them and untar them into `compilers/` so that the actual executable is found at `compilers/gcc-10.1.0/bin/gcc` (10.1.0 should be the version of gcc).

**Note**: Test out each compiler before you use it! Some of them require old libraries. You'll need to download them and put them in the right directory for your system. Often times, gcc can use the more recent library, but you'll need to point a symbolic link at it.

## OS Image

Syzkaller has used a few different images in the past, but we only need one of them.
