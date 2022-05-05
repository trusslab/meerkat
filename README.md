# syzInspector

I'll add more stuff later.

## Compilers

In order to have an accurate fuzzing environment for Syzkaller and the Linux kernel, syzInspector uses older gcc compilers for older versions of Linux. This has the added benefit of fixing some compiler-time bugs that have otherwise plagued me. However, this means you need to download multiple versions of gcc. Thankfully, the wonderful people over at Syzkaller have kept a log of all the compiler versions they have used, and the come pre-compiled! The tars for all of the compilers can be found over on the Syzkaller github. Download them and untar them into `compilers/` so that the actual executable is found at `compilers/gcc-7.0.0/bin/gcc`.

**Note**: Test out each compiler before you use it! Some of them require old libraries. You'll need to download them and put them in the right directory for your system. Often times, gcc can use the more recent library, but you'll need to point a symbolic link at it. Then come back here and remind me to make more in depth instructions.

## Helper Programs

There are several helper programs that I made because I'm misusing bash script and feeling the consequences. They can all be found in `helpers/`. Simply run `make` to make them and `make clean` to clean up the executables. I appologies for input/output format inconsistencies.

- `dateplusdays.cpp`: Compiles to `dpd`. Takes in a date (yyyy-mm-dd) and a number (x). Adds x days to the date and returns the new date.
- `decdate`: Takes in a date (yyyy mm dd) and decrements it by one. Returns the new date (yyyy-mm-dd).
- `incdate`: Takes in a date (yyyy mm dd) and increments it by one. Returns the new date (yyyy-mm-dd).
- `diffdate`: Takes in 2 dates (yyyy-mm-dd) and returns their difference in number of days.
- `findmaxtime`: Takes in any number of integers and returns floor(mean + 3 * standard deviation).

## OS Images

Syzkaller has used two different OS images to interface with the kernel since it began. I have included scripts to download both of them (stolen from Syzkaller again). They are slightly modified to change where they pull from (as old repositories get archived). You should be able to simply run them, but no guarantees that they'll work forever.

**Note**: Syzkaller boots these images in read-only mode so that it can run multiple instances off the same image. While necessary, it can be a pain when you compile Linux to run with SELinux (or some other option that requires a persistent change to the image). To fix this, boot the image manually with the config you want to use, then Syzkaller should work.

## Parsers

How to run and such

## Patches

**Do Not Touch the Patch Files**

They are specific to some old versions of Linux and Syzkaller. There should not be a reason to change them unless you want to do your own debugging.

## Template

Compile and short description

## Tools

These are some tools that I found useful while writing and debugging syzInspector. They don't have all the fancy arguments set up, so if you want to change any part of it, edit the code.

`tools/boot.sh` is a simple one-liner to boot a Linux version through Qemu with all the options that Syzkaller uses. You may need to change around the arguments and change the directory for either the OS image or Linux kernel

`tools/kprep.sh` downloads and makes a kernel version as specified by a hash passed as a command line argument. To change the repository or any other parameter, edit the source code.

## Configuration

The global parameters file can be found at `inspector-config/parameters.cfg`. This file is global to all instances of `inspector-manager.sh` that you have running, so keep that in mind if you change anything. There are a few directories that you will need to define for your system:

- `home`: This is the directory that houses the `SyzInspector` directory. It's mostly here to keep things organized.
- `inspectdir`: This is the directoy that holds all of syzInspector. Once `home` is set, you shouldn't need to change this.
- `gccdir`: syzInspector uses different compilers based on when the kernel version is from. You shouldn't need to change this, but make sure you place the compilers in the correct directory.

Next, you can define the resource allocation that syzkaller will use. You can define the number of VMs, the number of CPUs per VM, and the number of procs per VM.

- `numVM`: This is the number of VMs allotted to Syzkaller.
- `numCPU`: This is the number of CPUs allotted to each VM. Note the total number of CPUs used is `numCPU` * `numVM`.
- `numProcs`: This is the number of parallel test programs that each VM will run. This can be configured entirely independently of `numCPU`, but take care to calibrate it to your machine.

Each of the postfixes for the parameters above, namely `d`, `r`, and `st`, stand for default, race, and single threaded, respectively. They allow for different resource allocations for race and single threaded bugs. Note that the current values are calibrated for my machine and were the best results achieved after some tests.

`mem` is the amount of memory allocated for each VM in megabytes. I have it set to 4 GB because I experienced crashes with less. Feel free to change it at will.

`makeproc` is the number of cores to use when building the kernel. Using 72 cores, I could compile in 2 minutes, but it's dialed back to share resources with other instances. The variable is used in `make -j$makeproc`.

Lastly, `GOROOT` is the directory where go is installed. Syzkaller needs go, so make sure you install it in accordance with the Syzkaller documentation, and then point this variable at it.

## syzInspector / Inspector-Manager

Description and how to run them

## Syzkaller

Description
