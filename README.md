# SyzInspector

Welcome! This is SyzInspector, a system built to find the revealing factors of bugs found by Syzbot.

First things first, this project was built on Ubuntu 20.04 with 5.15.0-107-generic. It should run on more recent machines, but is untested.

## Syzkaller

SyzInspector will download and build Syzkaller on its own during inspection, but you should go through and install it and its dependencies to make sure they work. The most important of these will be go. Syzkaller will have excellent intructions on installing it.

## Compilers

In order to have an accurate fuzzing environment for Syzkaller and the Linux kernel, syzInspector uses older gcc compilers for older versions of Linux. This has the added benefit of fixing some compiler-time bugs that have otherwise plagued me. However, this means you need to download multiple versions of gcc. Thankfully, the wonderful people over at Syzkaller have kept a log of all the compiler versions they have used, and they come pre-compiled! The tars for all of the compilers can be found over on the Syzkaller github. Download them and untar them into `compilers/` so that the actual executable is found at `compilers/gcc-7.0.0/bin/gcc` (7.0.0 should be the version of gcc).

**Note**: Test out each compiler before you use it! Some of them require old libraries. You'll need to download them and put them in the right directory for your system. Often times, gcc can use the more recent library, but you'll need to point a symbolic link at it.

## OS Images

Syzkaller has used two different OS images to interface with the kernel since it began. I have included scripts to download both of them (stolen from Syzkaller again). They are slightly modified to change where they pull from (as old repositories get archived). You should be able to simply run them, but no guarantees that they'll work forever.

**Note**: Syzkaller boots these images in read-only mode so that it can run multiple instances off the same image. While necessary, it can be a pain when you compile Linux to run with SELinux (or some other option that requires a persistent change to the image). To fix this, boot the image manually with the config you want to use, then Syzkaller should work.

## Configuration

The global parameters file can be found at `inspector-config/parameters.cfg`. This file is global to all instances of `inspector-manager.sh` that you have running, so keep that in mind if you change anything. There are a few directories that you will need to define for your system:

- `home`: This is the directory that houses the `SyzInspector` directory. It's mostly here to keep things organized.
- `inspectdir`: This is the directoy that holds all of SyzInspector. Once `home` is set, you shouldn't need to change this.
- `gccdir`: syzInspector uses different compilers based on when the kernel version is from. You shouldn't need to change this, but make sure you place the compilers in the correct directory.
- `godir` : The directory where go is installed. Syzkaller needs go, so make sure you install it in accordance with the Syzkaller documentation, and then point this variable at it. I recommend go-1.20.1.
- `imagedir` : The directory that houses the directories for stretch and wheezy. Modified scripts for creating these images are provided in the image/ directory. Otherwise you can get them from the Syzkaller git.

Next, you can define the resource allocation that syzkaller will use. You can define the number of VMs, the number of CPUs per VM, and the number of procs per VM. Please note that your machone will need at least `numCPU` * `numVM` cores. The study was carried out by dividing up 16 cores.

- `numVM`: This is the number of VMs allotted to Syzkaller.
- `numCPU`: This is the number of CPUs allotted to each VM. Note the total number of CPUs used is `numCPU` * `numVM`.
- `numProcs`: This is the number of parallel test programs that each VM will run. This can be configured entirely independently of `numCPU`, but take care to calibrate it to your machine.

Each of the postfixes for the parameters above, namely `d`, `r`, and `st`, stand for default, race, and single threaded, respectively. They allow for different resource allocations for race and single threaded bugs. Note that the current values are calibrated for my machine and were the best results achieved after some tests.

`mem` is the amount of memory allocated for each VM in megabytes. I have it set to 4 GB because I experienced crashes with less. Feel free to change it at will.

`makeproc` is the number of cores to use when building the kernel. Using 72 cores, I could compile in 2 minutes, but it's dialed back to share resources with other instances. The variable is used in `make -j #`.

## Building

Depends on all of the dependencies from Syzkaller, plus: `lynx`, `g++`.

`syz-env` : Required for cross compiling syzkaller (i386). Recommend pulling the most recent syzkaller version and copying `syz-env` into `tools/`.

To build SyzInspector, simply run make. Then, run make in `helpers/` as well (It was easier to have this external program that shellscript can call).

## Parser

`parse/bparse.py` is a script to crawl the Syzbot public webpage for fixed bugs. You should ba able to simply run it to gather a few bugs. bparse will continue until it has looked at all fixed bugs, so feel free to stop it early. It will output to `bugs.csv` by default. Depends on `python3`. If you want to skip this, `parse/example.csv` has been provided with a singl ebug to test things out.

## Running SyzInpsector

It is recommended that you use the inspector-manager.sh script. To do so, simply run the script with the following arguments:

`./inspector-manager.sh -s 1 -e 1 -i 1 -b example.csv`

 - `-s #` : The line number of the first bug to inspect in the bug file (bugs.csv).
 - `-e #` : The line number of the last bug to inspect in the bug file. Finishes after testing this bug.
 - `-i #` : An id number to use. This is important for running multiple instances at once, where each needs a different id.
 - `-b bugfile` : the file containing the bugs parsed with bparse as it appears in `parse/`, `parse/example.csv` for example would be passed as `-b example.csv`. `parse/example.csv` has been provided with a single bug to allow you to test out SyzInspector.

The arch can also be specified as either i386 or amd64. Default is amd64. SyzInspector will only inspect bugs for the arch which has been set.
 - `-a amd64/i386`

### Notes

- Not every parsed bug can be inspected. inspector-manager.sh has its own checks to ensure the bug is valid. As of right now, it also filters out bugs from 2024. These checks are performed here to reduce spamming Linux and Syzkaller gits.
- Inspecting i386 bugs requires docker. if you do not have rootless docker, this means you must run `sudo ./inspector-manager.sh`. This also has the downside of messing up the file ownership of the working directory.
- The name of this project changed partway through. Some files refer to "retrospect" rather than "inspect". Please ignore these differences.

## Patches

**Do Not Touch the Patch Files**

They are specific to some old versions of Linux and Syzkaller. There should not be a reason to change them unless you want to do your own debugging.

## Tools

These are some tools that I found useful while writing and debugging syzInspector. They don't have all the fancy arguments set up, so if you want to change any part of it, edit the code.

`tools/boot.sh` is a simple one-liner to boot a Linux version through Qemu with all the options that Syzkaller uses. You may need to change around the arguments and change the directory for either the OS image or Linux kernel

`tools/kprep.sh` downloads and makes a kernel version as specified by a hash passed as a command line argument. To change the repository or any other parameter, edit the source code.

## SyzInspector Paper

The results presented in "SyzInspector: A Large-Scale Analysis of Syzbot," can be found in `results/`. There are more README files there as well to explain the results.
