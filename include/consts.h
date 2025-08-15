#ifndef CONSTS_H
#define CONSTS_H

#define PROJECT_NAME "Meerkat"
#define REVISION "1.0.5"

#define REPRO_FREQ 3
#define DEFAULT_MAX_TIME 10
#define DEFAULT_FUZZ_TIMES 1
#define MAX_FUZZ_TIMES 3

#define EARLIEST_TESTED_VERSION "v4.9"

// Resource Allocation for a single instance of Syzkaller
// VM -> the number of VMs to be given to syzkaller
// CPU -> the number of CPU cores to be given to each VM (VM * CPU = Total Cores)
// procs -> the number of parallel test programs to run in each VM (legal 1-32, recommended 4-8, default 6)

// Default
#define NUM_VM_DEFAULT 4
#define NUM_CPU_DEFAULT 4
#define NUM_PROCS_DEFAULT 24

// Multi Threaded Bug (Race)
#define NUM_VM_MT 4
#define NUM_CPU_MT 4
#define NUM_PROCS_MT 24

// Single Threaded Bug
#define NUM_VM_ST 8
#define NUM_CPU_ST 2
#define NUM_PROCS_ST 16

// memory per VM
#define VM_MEM 4096

// number of processors to build the kernel with
#define MAKE_PROCS 16

#define SPACER "====================================================================================================================================================\n"
#define CONFW 20
#define SESHW 11

#define TIME_INCREMENT 1    // Number of minutes per time increment
#define START_PORT 12000

#define BUF_SIZE 4096

#endif
