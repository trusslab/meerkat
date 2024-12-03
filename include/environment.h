#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <bug_info.h>

#include <string>

class Environment
{
public:
    bool use_poc;
    bool find_only;
    bool no_merge;
    bool safe_mode;

    unsigned int max_time;
    unsigned int fuzztimes;
    unsigned int id;

    std::string origin_path;
    std::string logfilename;
    std::string linux_repo_remote;

    std::string wd;                     // wd-inspector-[id]
    std::string syzdir;                 // the directory that houses syzkaller
    std::string kerneldir;              // the directory that houses the kernel

    std::string syzwd;                  // wd-kaller
    std::string syzconfig;              // the config for syzkaller. We write this ourselves
    std::string syzkaller_log;          // the log file to hold syzkaller output

    void parse_config_file(const Bug_Info &, const std::string &);
};

#endif
