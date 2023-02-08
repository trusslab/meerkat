#ifndef BUG_INFO_H
#define BUG_INFO_H

#include <string>

class Bug_Info
{
private:
    int number;                         // the bug number
    std::string name;                   // the name of the bug given by syzbot

    std::string kpreface;               // the preface of the repository (linux, linux-next, net, etc..)
    std::string arch;                   // either amd64 or i386 for 64 or 32 bit POC 
    std::string repository;             // the repository in a form that fits the link (torvalds/linux.git)
    std::string kernel_config;          // the config file for the kernel
    std::string reproducer;             // the directory with all of the PoCs
    std::string allreproducer;          // the file with all PoCs concatenated
    std::string syzkaller_wd;           // wd-kaller
    std::string syzkaller_config;       // the config for syzkaller. We write this ourselves
    std::string wd;                     // wd-inspector-[id]
    std::string syzkaller_dir;          // the directory that houses syzkaller
    std::string kernel_dir;             // the directory that houses the kernel
    std::string bug_link;               // the link to the bug in syzbot

    std::string syzkaller_log;          // the log file to hold syzkaller output

public:
    Bug_Info()
    { return; }

    // takes in a filename and parses it into the data structure
    void parse_config_file(const std::string &);

    int get_number() const
    { return number; }

    std::string get_name() const
    { return name; }

    std::string get_kpref() const
    { return kpreface; }

    std::string get_arch() const
    { return arch; }

    std::string get_repo() const
    { return repository; }

    std::string get_kconfig() const
    { return kernel_config; }

    std::string get_repro() const
    { return reproducer; }

    std::string get_allrepro() const
    { return allreproducer; }

    std::string get_kallerwd() const
    { return syzkaller_wd; }

    std::string get_syzconfig() const
    { return syzkaller_config; }

    std::string get_wd() const
    { return wd; }

    std::string get_syzdir() const
    { return syzkaller_dir; }

    std::string get_kerneldir() const
    { return kernel_dir; }

    std::string get_buglink() const
    { return bug_link; }

    std::string get_kaller_log() const
    { return syzkaller_log; }
};

#endif
