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



### `-m <int>`


