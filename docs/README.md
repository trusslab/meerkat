# Artifacts Functional/Reproducible

Hello! This is the documentation for the evaluation of the Artifacts Functional and Reproducible badges for Meerkat.
You are, of course, welcome to use this tool however you wish, but to make the evaluation eaasier, we have put together some easy 'profiles' for you to use.
These profiles run a small subset of the bugs in Meerkat's dataset, primarily becuase there are so many and that would take a long time.

## Environment

 - Meerkat has been tested on Ubuntu 20.04 and 24.04. It should be able to run on any debian-based system, but has not been tested.
 - Meerkat expects to be run on an x86 machine.
 - Meerkat can be run in a VM assuming nested virtualization is enabled (and the VM has enough cores).
 - Meerkat expects to be on a system with at least 16 threads (`nproc` >= 16), at least 20 is recommended.
 - Meerkat allocates 32GB of memory across 8 VMs. Your system should have more than this to prevent bottlenecks.

## Setup

This is just here to tell you that you don't (or shouldn't) need to do anything!
The script `./artifact-verify.sh` calls `setup.sh`, which will pull and setup all dependencies automatically.
Please note that `./setup.sh` will require sudo access for certain dependencies.

Assuming you start from scratch, the script will:
 - Pull some packages from `apt`.
 - Add the current user to the kvm group.
 - Pull the compilers from our zenodo and verify them.
 - Create a Debian Stretch image.
 - Download go and export it to the path.
 - Build Meerkat and the included Syzkaller.

A note on exporting Go: `artifact-verify.sh` will always export go to itself if needed, but this does not mean it will show up in your path.
If you want to do your own thing with Go, consider exporting it yourself or permanently adding it to your path.
Of course if you already have a Go install, that should work just fine.

One more note: Go does a lot of funky things with `$GOROOT` which Meerkat completely ignores and tries to work around (mostly because I think forcing all Go programs to be in subdirectories of the Go install is silly).
If, for some reason, you have your own Go install and get errors related to `$GOROOT`, just try clearing the environment variable.

## Functional

To test the basic functionality of Meerkat, we have prepared a single bug that should complete relatively quickly.
To run it, simply run `./artifact-verify.sh functional`.

## Reproducible

Testing the reproducibility of our entire results would take a long time and a lot of compute.
So, we have provided a strategic subset which should be enough to verify our claims while taking less time.
The subset we have provided consists of the 37 bugs for which Meerkat was able to find the BiC, but Syz-bisect could not.
This subset can be found in `parse/short.csv`.
The smaller dataset _should_ complete in about 14 days, assuming the 9 hour average runtime holds.

You can run this dataset with `./artifact-verify.sh reproducible`

### 37 is not 34

In the paper, we said that Meerkat found the BiC in 34 more cases than Syz-bisect.
This is correct.
Meerkat actually did better in 37 cases, and Syz-bisect did better than Mk in 3 (so the difference is 34).
The cases where Meerkat did better are explained below:

 - bug0075: SB got lucky with a flakey bug and beat Mk.
 - bug0144: SB traced back an (inter-report) duplicate that Mk could not identify.
 - bug0901: SB traced back a completely different bug to the oldest tested release (which is technically the correct result).

## Other Profiles

Of course you are free to run Meerkat in any configuration you desire.
Some alternative profiles for each iteration of our ablation study (mk1p, mknm, mkm1p, mk) have been set up as well as some 'dataset scope' arguments (basic, short, full).
Each profile uses the same features as you would expect from the paper.
The arguments basic, short, and full, each correspond to how many and which bugs will be bisected in order.
These are described below.

### Profiles:

 - mk1p: Uses `-F poc-test`
 - mknm: Uses `-F poc-test,poc-all-pocs`
 - mkm1p: Uses `-F ff-test,poc-test,stateful-corpus`
 - mk: Uses: `-F ff-test,poc-test,stateful-corpus,poc-all-pocs`

### Profile Args (dataset scope):

 - basic: Run a single bug. Specifically, bug0003.
 - short: Run the 37 bugs selected for the reproducible badge, descriped above.
 - full: Run all 200 bugs.

For example, if you wanted to run the full dataset as mk1p, you would use `./artifact-verify.sh mk1p full`

Finally, you can run `mk-manager.sh` directly.
For documentation, use `-h`.

## Understanding the Results

see [reading-results.md](reading-results.md)

