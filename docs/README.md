# Artifacts Functional/Reproducible

Hello! This is the documentation for the evaluation of the Artifacts Functional and Reproducible badges for Meerkat.
You are, of course, welcome to use this tool however you wish, but to make the evaluation eaasier, we have put together some easy 'profiles' for you to use.
These profiles run a small subset of the bugs in Meerkat's dataset, primarily becuase there are so many and that would take a long time.

## Environment

 - Meerkat has been tested on Ubuntu 20.04 and 24.04. It should be able to run on any debian-base system, but has not been tested.
 - Meerkat expects to be run on an x86 machine.
 - Meerkat can be run in a VM assuming nested virtualization is enabled (and the VM has enough cores).
 - Meerkat expects to be on a system with at least 16 threads (`nproc` >= 16), at least 20 is recommended.
 - Meerkat allocates 32GB of memory across 8 VMs. Your system should have more than this to prevent bottlenecks.

## Setup

This is just here to tell you that you don't (or shouldn't) need to do anything!
The script `./artifact-verify.sh` calls `setup.sh`, which will pull and setup all dependencies automatically.
Please note that `./setup.sh` will require sudo access for certain dependencies.

## Functional

To test the basic functionality of Meerkat, we have prepared a single bug that should complete relatively quickly.
To run it, simply run `./artifact-verify.sh functional`.

Assuming you start from scratch, the script will pull some packages from `apt`

## Reproducible

### Example:

## Understanding the Results

### Example:

    see <results.md>

