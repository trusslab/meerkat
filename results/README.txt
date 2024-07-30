AUTHOR:
I first want to acknowledge that the bug numbers do no match up with the number of bugs
we said we gathered for inspection. This is because I'm a silly person who decided to do
most of the filtering AFTER, we gathered potential bugs. This was to prevent having to
call out to the linux git for every bug in Syzbot. The filtering was done by
inspector-manager.sh, our automation aparatus. The numbers reported in the paper are
representative of the bugs that we actually attempted to inspect.

The filtering process is as follows:

bparse.py gathers potential bugs based on repository and available artifacts. The
resulting bugs are stored in a .csv file.
inspector-manager.sh downloads and performs checks on the artifacts such as ensuring
dates are in order.

For instance, in batch2, 145 bugs had out of order dates and were ignored. 12 more were
found too soon after their introduction to be interesting (within 1 day), so they were
also ignored.

        Gathered    Tested  Ignored
Batch1: 106         106     0
Batch2: 1197        1040    157
Batch3: 413         263     150

Total:              1409

There are additional READMEs in each batch explaining any irregularities.
