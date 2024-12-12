#ifndef BUG_INFO_H
#define BUG_INFO_H

#include <blocking_bugs.h>
#include <date.h>
#include <environment.h>
#include <result.h>

#include <string>
#include <vector>

class Bug_Info
{
public:
    int number;                         // the bug number
    std::string name;                   // the name of the bug given by syzbot
    std::string numName;                // the number name given by inspector-manager.sh (i.e. bug0003)

    std::string kpreface;               // the preface of the repository (linux, linux-next, net, etc..)
    std::string arch;                   // either amd64 or i386 for 64 or 32 bit POC 
    std::string repository;             // the repository in a form that fits the link (torvalds/linux.git)
    std::string kconfig;                // the config file for the kernel
    std::string reproducer;             // the directory with all of the PoCs
    std::string allreproducer;          // the file with all PoCs concatenated
    std::string buglink;                // the link to the bug in syzbot

    std::string guilty_hash;
    std::string find_hash;
    Date find_date;

    std::vector<std::string> duplicates;
    Blocking_Bugs blocking_bugs;

    Bug_Info()
    { return; }

    // takes in a filename and parses it into the data structure
    int parse_config_file(const Environment &, const std::string &);
};

int patch_blocking_bugs(const Test_Result &, const Bug_Info &);

#endif
