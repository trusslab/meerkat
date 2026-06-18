# How to use Meerkat Manager (mk-manager.sh)

`mk-manager.sh` is a lovely little tool that turns the entries in bugs.csv into Meerkat bisection runs.
Among other things, it downloads all of the bug artifacts like PoCs/reproducers, the kernel config, etc.
I highly recommend using `mk-manager.sh` over calling `./bin/meerkat` yourself.
While it does have a `-h` option, I'll explain how to use it here.

## Command Line Options

### `-i <int>`

This argument is the ID of the Meerkat instance. It determines the name of the working directory as well as the port range for VMs.
If you want to run multiple Meerkat's simultaneously, use this ID to separate them.
Please only put a positive integer here.

### `-s <line num>`

`mk-manager.sh` parses bug files (i.e. `parse/bugs.csv`) to get the information and artifacts for each bug.
This argument is the line in that file that it should start at.
Please keep in mind that the line number and bug ID are two different things and don't match up (i.e. `-s 3` will not run bug0003).
It wasn't my brightest idea, but here we are.
This argument is mandatory.

### `-e <line num>`

This is the line number that `mk-manager.sh` should stop at.
Please note that it defines an inclusive range, so `-s 1 -e 2` will run both lines 1 and 2.
If this argument is not provided, `mk-manager.sh` will continue until the end of the big file.

### `-b <filename>`

This is the bug file that `mk-manager.sh` should read from.
The ones we used are `parse/bugs.csv` and `parse/short.csv`.

### -F `<feature list>`

This argument accepts a comma-separated list of features that Meerkat should use.
They are described below:

 - `poc-test`: run phase 2 testing with just the PoCs (not including this means phase 2 will not be run).
 - `ff-test`: run phase 1 testing with PoC mutation (not including this means phase 1 will not be run).
 - `poc-all-pocs`: use all known PoCs for a bug rather than just one.
 - `stateful-corpus`: keep the corpus between mutation sessions.
 - `setup-only`: stop after buiding the anchor commit.
 - `find-only`: stop after testing the anchor commit.
 - `ff-no-find-backup`: if the bug is not found at the anchor commit, stop. Don't fall back to phase 2.
 - `default`: equivalent to `poc-test,ff-test,poc-all-pocs,stateful-corpus`. This is the default if `-F` is not used.

 - `no-patch-kernel`: don't patch the older kernel (don't use this).
 - `obselete-patches`: use the old/manual method of patching the kernel (don't use this).
 - `old-syzkaller`: use syzkaller command that is compatible with a slightly older version (don't use this).

### `-m <int>`

The maximum fuzzing time.
The default is 10 if `-m` is not specified.

## Example

This is what a reasonable command looks like:
```
./mk-manager.sh -i 1 -s 1 -e 5 -b parse/bugs.csv -F ff-test,poc-test,poc-all-pocs,stateful-corpus
```
