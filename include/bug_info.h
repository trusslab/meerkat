#ifndef BUG_INFO_H
#define BUG_INFO_H

#include <string>
#include <vector>

class Bug_Info
{
public:
    int number;                         // the bug number
    std::string name;                   // the name of the bug given by syzbot

    std::string kpreface;               // the preface of the repository (linux, linux-next, net, etc..)
    std::string arch;                   // either amd64 or i386 for 64 or 32 bit POC 
    std::string repository;             // the repository in a form that fits the link (torvalds/linux.git)
    std::string kconfig;                // the config file for the kernel
    std::string reproducer;             // the directory with all of the PoCs
    std::string allreproducer;          // the file with all PoCs concatenated
    std::string syzwd;                  // wd-kaller
    std::string syzconfig;              // the config for syzkaller. We write this ourselves
    std::string wd;                     // wd-inspector-[id]
    std::string syzdir;                 // the directory that houses syzkaller
    std::string kerneldir;              // the directory that houses the kernel
    std::string buglink;                // the link to the bug in syzbot

    std::vector<std::string> duplicates;

    std::string syzkaller_log;          // the log file to hold syzkaller output

    Bug_Info()
    { return; }

    // takes in a filename and parses it into the data structure
    void parse_config_file(const std::string &);
};

#endif
