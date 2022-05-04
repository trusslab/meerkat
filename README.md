# syzInspector

I'll add more stuff later.

## Compilers

Stuff about how to get compilers and mark them for inspector to use

## Helper Programs

How to compiler and a shoort description

## OS Images

How to compile and such

## Parsers

How to run and such

## Patches

Don't touch them

## Template

Compile and short description

## Tools

Short description

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

`makeproc` is the number of cores to use when building the kernel. Using the full 72 cores, I could get it done in 2 minutes, but it is dialed back to share resources with other instances. The variable is used in `make -j$makeproc`.

Lastly, `GOROOT` is the directory where go is installed. Syzkaller needs go, so make sure you install it in accordance with the Syzkaller documentation, and then point this variable at it.

## syzInspector / Inspector-Manager

Description and how to run them

## Syzkaller

Description
