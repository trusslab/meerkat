# How to Read Meerkat Bisection Results

Here, we'll go over an example log from bug0003 with how to read it.
Assuming you use either `artifact-verify.sh` or `mk-manager.sh`, all of Meerkat's output is `tee`'d into a corresponding log file in the log directory.
This directory also has kernel build logs (for the most recent build), syzkaller logs, and some VM logs.

To start off, Meerkat prints the command used to invoke it, as well as pretty much anything you could want to know about the bug.
Note, right after printing the command, Meerkat pulls the Linux kernel as needed.
This part can take upwards of 10 minutes before Meerkat prints anything else.

Slight note, 'PoC' and 'reproducer' mean the same thing.
I'm used to 'reproducer', but 'PoC' was nicer for the paper.

```
./bin/meerkat -i 1 -c /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/meerkat.cfg -a dcb85f85fa6f142aae1fe86f399d4503d49f2b60 -F ff-test,poc-test,stateful-corpus,poc-all-pocs -m 10
Bisecting:          BUG: MAX_LOCK_DEPTH too low!
Syzbot Link:        https://syzkaller.appspot.com/bug?id=ac7db280d77ec300d7cdaa14625a32be584c8eec
Working Name:       bug0003

Aliases:
    BUG: MAX_LOCK_DEPTH too low! (has report)

Anchor Commit:      dcb85f85fa6f142aae1fe86f399d4503d49f2b60
Repository:         https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
Branch:             master
Arch:               amd64

Meerkat:            /mnt/sdd/jtbursey/meerkat/
Version:            1.2.0
Kernel:             /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/kernel/
Syzkaller:          /mnt/sdd/jtbursey/meerkat/syzkaller/
Compilers:          /mnt/sdd/jtbursey/meerkat/compilers/
Compiler:           gcc (Ubuntu 10.5.0-1ubuntu1~20.04) 10.5.0
Linker:             GNU ld (GNU Binutils for Ubuntu) 2.34
Ccache:             ccache version 3.7.7
Image:              /mnt/sdd/jtbursey/meerkat/image/stretch/stretch.img
Image Key:          /mnt/sdd/jtbursey/meerkat/image/stretch/stretch.id_rsa

Workdir:            /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/
Kconfig:            /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/config-bug0003.txt
Reproducers: (2)    /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/reproducers/
Primary PoC:        /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/reproducers/repro-bug0003-2.prog
Syzkaller Workdir:  /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/wd-kaller/
Log Directory:      /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/log/

Syscalls: (13)      ["connect$inet", "sendmmsg", "setsockopt$inet_tcp_TCP_REPAIR", "setsockopt$inet_tcp_TCP_REPAIR_QUEUE", "socket$inet_smc", "socket$inet", "socket", "ioctl$EXT4_IOC_GROUP_EXTEND", "openat$sw_sync_info", "ioctl$sock_SIOCGIFINDEX", "ioctl$ifreq_SIOCGIFINDEX_vcan", "ioctl$IOCTL_GET_NCIDEV_IDX", "openat$nci"]
VM Count:           8
CPU Count:          2
Procs:              16
Memory:             4096
Make Procs:         16
Max Time:           10
Max Attempts:       1
```

Then we go to the anchor commit to test if the bug is reproducible.
Each session prints the wall clock datetime, the session number, the kernel commit (and tag), and the compiler used.
To make the report easier to parse, whenever the correct bug is found, it is highlighed with '***'.

```
==== Anchor Commit ====

2026-06-14 13:09:51
Session:   1
Kernel:    2022-02-03 - dcb85f85fa6f142aae1fe86f399d4503d49f2b60
Compiler:  ccache gcc
Attempt 1:
    Time  Bug Name
***    3  BUG: MAX_LOCK_DEPTH too low! (has report)
```
Here at attempt 2, you can see that the bug was found 5 times at the 3 minute mark.
(The anchor commit is the only time we fuzz more than once as per the paper)
```
Attempt 2: (RETRY)
    Time  Bug Name
***    3  BUG: MAX_LOCK_DEPTH too low! (5) (has report)
Attempt 3: (RETRY)
    Time  Bug Name
***    4  BUG: MAX_LOCK_DEPTH too low! (has report)
The bug was found.

New Max Time: 10
```
So the bug is findable, and we can move on to the major releases. Here, you can see that the bug was not found, but some unrelated bug was.
```
==== Major Release Search ====
11 Releases

2026-06-14 13:33:43
Session:   2
Kernel:    2022-01-09 - df0cc57e057f18e44dac8e6c18aba47ab53202f9 (v5.16)
Compiler:  ccache gcc
Attempt 1:
    Time  Bug Name
       7  WARNING: ODEBUG bug in netdev_run_todo (has report)
      10  WARNING: ODEBUG bug in netdev_run_todo (has report)
The bug was not found.
About 10 releases remaining
```
Since the bug was not found in this release, we can move on to Git bisect.
```
==== Kernel Bisection ====
12432 Linux commits

2026-06-14 13:57:18
Session:   3
Kernel:    2022-01-11 - 9149fe8ba7ff798ea1c6b1fa05eeb59f95f9a94a
Compiler:  ccache gcc
Attempt 1:
    No crashes found.
The bug was not found.
About 6257 commits remaining

2026-06-14 14:20:38
Session:   4
Kernel:    2022-01-18 - aee101d7b95a03078945681dd7f7ea5e4a1e7686
Compiler:  ccache gcc
Attempt 1:
    No crashes found.
The bug was not found.
About 3239 commits remaining

2026-06-14 14:43:57
Session:   5
Kernel:    2022-01-18 - e3a8b6a1e70c37702054ae3c7c07ed828435d8ee
Compiler:  ccache gcc
Attempt 1:
    No crashes found.
The bug was not found.
About 1638 commits remaining

2026-06-14 15:07:13
Session:   6
Kernel:    2022-01-22 - 636b5284d8fa12cadbaa09bb7efa48473aa804f5
Compiler:  ccache gcc
Attempt 1:
    No crashes found.
The bug was not found.
About 821 commits remaining
```
As you can see, we went quite a while without finding the bug. That's fine.
```
2026-06-14 15:30:30
Session:   7
Kernel:    2022-01-31 - 9cef24c8b76c1f6effe499d2f131807c90f7ce9a
Compiler:  ccache gcc
Attempt 1:
    Time  Bug Name
***    2  BUG: MAX_LOCK_DEPTH too low! (4) (has report)
Running syz-repro.
New PoC saved at /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/reproducers/repro-bug0003-0.prog
Primary PoC:     /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/reproducers/repro-bug0003-0.prog
Run Time:        0h8m0s
The bug was found.
About 416 commits remaining
```
Finally we found the bug again, and you get to see how we identify the new PoC/reproducer.
As per the paper, this process does not happen every time we find the bug.
Let's skip ahead now.
```
...
...
...
2026-06-14 18:33:48
Session:   15
Kernel:    2022-01-31 - 341adeec9adad0874f29a0a1af35638207352a39
Compiler:  ccache gcc
Error: index error parsing basic report
Attempt 1:
    Time  Bug Name
***    2  BUG: MAX_LOCK_DEPTH too low! (4) (has report)
The bug was found.
About 10 commits remaining

2026-06-14 18:40:22
Session:   16
Kernel:    2022-01-28 - 6449520391dfc3d2cef134f11a91251a054ff7d0
Compiler:  ccache gcc
Attempt 1:
    No crashes found.
The bug was not found.
About 2 commits remaining

==== Partial Result: Mutation Phase ====
Reproducer:           /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/reproducers/repro-bug0003-0.prog
Bisection Result:     2022-01-31 - 341adeec9adad0874f29a0a1af35638207352a39
Bisected Commit Name: net/smc: Forward wakeup to smc socket waitqueue after fallback
Stage Time:           5h44m58s
Run Time:             5h45m0s
=========================================
```
Here, you can see that Meerkat has arrived at a phase 1 result.
For the result, Meerkat prints the primary PoC/reproducer, the date and hash of the resulting commit, it's title, and the runtime.
But there's still a phase 2 to run.
```
==== Anchor Commit ====

2026-06-14 18:54:49
Session:   17
Kernel:    2022-01-28 - 6449520391dfc3d2cef134f11a91251a054ff7d0
Compiler:  ccache gcc
PoC:       /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/reproducers/repro-bug0003-0.prog
Attempt 0:
    No crashes found.
Attempt 1:
    No crashes found.
Attempt 2:
    No crashes found.
Attempt 3:
    No crashes found.
Attempt 4:
    No crashes found.
Attempt 5:
    No crashes found.
Attempt 6:
    No crashes found.
Attempt 7:
    No crashes found.
The bug was not found.

Failure: This bug cannot be found at the anchor commit.

==== Partial Result: Primary PoC Test ====
Reproducer:           /mnt/sdd/jtbursey/meerkat/wd-meerkat-1/reproducers/repro-bug0003-0.prog
Bisection Result:     Failed to reproduce at the anchor commit.
Stage Time:           0h11m36s
Run Time:             5h56m36s
==========================================
```
Aaaaaaand, the bug does not reproduce before (chronologically prior) the phase 1 result.
Note, this goes on for some time until all PoCs are tested.
After phase 2 has run, Meerkat prints the final decision.
```
====================================================================================================================================================
Bug Name:             BUG: MAX_LOCK_DEPTH too low!
Bug ID:               bug0003
Bug Link:             https://syzkaller.appspot.com/bug?id=ac7db280d77ec300d7cdaa14625a32be584c8eec
Bisection Result:     2022-01-31 - 341adeec9adad0874f29a0a1af35638207352a39
Bisected Commit Name: net/smc: Forward wakeup to smc socket waitqueue after fallback
Run Time:             6h19m54s

Anchor Commit:        2022-02-03 - dcb85f85fa6f142aae1fe86f399d4503d49f2b60
Bisection Result:     2022-01-31 - 341adeec9adad0874f29a0a1af35638207352a39
```
Here, Meerkat will print the most relevant information for the bug, including the bug name, id, and Syzbot link.
Next is the bisection result, same as the partial result above.
Finally, I have Meerkat print the anchor and bisected commits because I couldn't be bothered to move my eyes a whole 2 degrees when looking through these results. :)

You can compare the bisected result to the BiC listed by the kernel developers by:
 1. Following the Syzbot link
 2. Going to the Fix commit: `Fix commit: 1de9770d121e net/smc: Avoid overwriting the copies of clcsock callback functions`
 3. Observing the Fixes tag: `Fixes: 341adeec9ada ("net/smc: Forward wakeup to smc socket waitqueue after fallback")`

Alternatively, the correct BiCs are listed in `results/bics.csv`.
