#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <bug_info.h>

#include <string>

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

class Environment
{
public:
    bool use_poc;
    bool stateful_corpus;
    bool prune_corpus;
    bool find_only;
    bool no_merge;
    bool safe_mode;

    unsigned int max_time;
    unsigned int fuzztimes;
    unsigned int id;

    int memory;                         // memory per vm in MB
    int makeprocs;                      // number of threads to using while making

    std::string origin_path;
    std::string logfilename;
    std::string linux_repo_remote;

    std::string wd;                     // wd-inspector-[id]
    std::string syzdir;                 // the directory that houses syzkaller
    std::string kerneldir;              // the directory that houses the kernel

    std::string syzwd;                  // wd-kaller
    std::string syzconfig;              // the config for syzkaller. We write this ourselves
    std::string syzkaller_log;          // the log file to hold syzkaller output

    std::string home;                   // SyzInspector/
    std::string gcc_dir;                // the directory housing all of the gcc compilers
    std::string go_dir;                 // the directory housing go
    std::string image_dir;              // directory of the os images

    Compiler_Setting compiler_setting;
    
    VMConfig vmd;                       // vm resource allocations for default, race, and single-thread
    VMConfig vmr;
    VMConfig vmst;
    VMConfig vmc;

    Port_Info port;

    // parses the parameters.cfg into the data structure
    void parse_parameters_file(const std::string &);

    void parse_config_file(const Bug_Info &, const std::string &);
};

#endif
