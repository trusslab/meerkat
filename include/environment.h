#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

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
};

#endif
