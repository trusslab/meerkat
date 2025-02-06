#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <string>
#include <vector>
#include <set>

enum Compiler_Setting {COMPILER_GCC = 0, COMPILER_CLANG, COMPILER_CLANG_14};

// stores info about the host port for easy arithmetic
// The port must change in between runs of Syzkaller in
// case the previous port was not freed yet.
class Port_Info
{
public:
    int port;
    int port_count;
    int start_port;
    int range;

    Port_Info()
        : port(0), port_count(0), start_port(0), range(3)
    { return; }
    
    Port_Info(int p, int pc, int sp, int r)
        : port(p), port_count(pc), start_port(sp), range(r)
    { return; }

    int init(const int, const int, const int);
    int inc();
};

// stores vm resource allocation
class VMConfig
{
public:
    int numVM;
    int numCPU;
    int numProcs;
};

class Features
{
public:
    bool poc_test;
    bool ff_test;
    bool setup_only;
    bool find_only;
    bool stateful_corpus;
    bool patch_kernel;
};

class Environment
{
private:
    void default_features();
    void config_print(const std::string &, const std::string &) const;

public:
    bool try_patch;
    bool safe_mode;

    unsigned int max_time;
    unsigned int fuzztimes;
    unsigned int id;

    int memory;                         // memory per vm in MB
    int makeprocs;                      // number of threads to using while making

    std::string home;                   // SyzInspector/
    std::string gcc_dir;                // the directory housing all of the gcc compilers
    std::string image_dir;              // directory of the os images
    std::string syzdir;                 // the directory that houses syzkaller

    std::string wd;                     // wd-inspector-[id]
    std::string kerneldir;              // the directory that houses the kernel
    std::string syzwd;                  // wd-kaller
    std::string syzconfig;              // the config for syzkaller. We write this ourselves
    
    std::string logdir;                 // directory to put all the logs in
    std::string syzkaller_log;          // the log file to hold syzkaller output

    Compiler_Setting compiler_setting;
    
    VMConfig vmd;                       // vm resource allocations for default, race, and single-thread
    VMConfig vmr;
    VMConfig vmst;
    VMConfig vmc;

    Port_Info port;

    // Bug Info
    std::string name;                   // the name of the bug given by syzbot
    std::string working_name;           // bug0001

    std::string arch;                   // either amd64 or i386 for 64 or 32 bit POC 
    std::string repository;             // the kernel repository
    std::string branch;                 // The kernel branch
    std::string kconfig;                // the config file for the kernel
    std::string reprodir;               // the directory with all of the PoCs
    std::string buglink;                // the link to the bug in syzbot

    std::string anchor_hash;

    std::vector<std::string> duplicates;

    std::vector<std::string> required_syscalls;

    Features feats;

    // Read some default configs from consts.h
    int init();

    // parses unique bug config to get filenames
    int parse_config_file(const std::string &);
    int handle_features(const std::set<std::string> &);

    // Pretty-print
    void print() const;
};

#endif
