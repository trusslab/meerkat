#ifndef BUG_INFO_H
#define BUG_INFO_H

#include <blocking_bugs.h>
#include <environment.h>
#include <result.h>

#include <string>
#include <vector>

class Bug_Info
{
public:
    std::string name;                   // the name of the bug given by syzbot

    std::string arch;                   // either amd64 or i386 for 64 or 32 bit POC 
    std::string repository;             // the kernel repository
    std::string kconfig;                // the config file for the kernel
    std::string reprodir;               // the directory with all of the PoCs
    std::string buglink;                // the link to the bug in syzbot

    std::string find_hash;

    std::vector<std::string> duplicates;
    Blocking_Bugs blocking_bugs;

    Bug_Info()
    { return; }

    // takes in a filename and parses it into the data structure
    int parse_config_file(const Environment &, const std::string &);
};

#endif
