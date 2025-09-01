#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <port.h>

#include <string>
#include <vector>
#include <set>

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
    bool ff_no_find_backup;
    bool poc_all_pocs;
    bool stateful_corpus;
    bool no_patch_kernel;
    bool obselete_patches;
};

class Environment
{
private:
    void default_features();
    void config_print(const std::string &, const std::string &, const int = -1) const;

public:
    bool try_patch;
    bool debug;

    unsigned int max_time;
    unsigned int fuzztimes;
    unsigned int id;

    int memory;                         // memory per vm in MB
    int makeprocs;                      // number of threads to using while making

    std::string home;                   // SyzInspector/
    std::string compiler_dir;           // the directory housing all of the gcc compilers
    std::string compiler;               // gcc or clang
    std::string ccache;                 // location of ccache to use
    std::string image;                  // path to the image
    std::string image_key;              // path to the image key
    std::string syzdir;                 // the directory that houses syzkaller

    std::string wd;                     // wd-meerkat-[id]
    std::string kerneldir;              // the directory that houses the kernel
    std::string syzwd;                  // wd-kaller
    std::string syzconfig;              // the config for syzkaller. We write this ourselves
    std::string vmwd;                   // wd-runner
    
    std::string logdir;                 // directory to put all the logs in
    std::string syzkaller_log;          // the log file to hold syzkaller output
    
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
    std::string primary_repro;          // the repro to be used in PoC testing
    std::string buglink;                // the link to the bug in syzbot

    std::string anchor_hash;

    std::vector<std::string> duplicates;

    std::vector<std::string> base_syscalls;
    std::vector<std::string> required_syscalls;

    Features feats;

    // Read some default configs from consts.h
    int init();

    // parses unique bug config to get filenames
    int parse_config_file(const std::string &);
    int handle_features(const std::set<std::string> &);

    std::string syscall_string() const;

    std::string repro_opts_file() const
    { return reprodir + "repro.opts"; }

    std::string kbuildlog() const
    { return logdir + (working_name.empty() ? "" : working_name + "-") + "kbuild.log"; }

    std::string bootfaillog() const
    { return logdir + (working_name.empty() ? "" : working_name + "-") + "boot_failure.log"; }

    std::string reprolog() const
    { return logdir + (working_name.empty() ? "" : working_name + "-") + "repro.log"; }

    // Pretty-print
    void print() const;
};

#endif
