#ifndef BUG_INFO_H
#define BUG_INFO_H

#include <string>

class Bug_Info
{
private:
    int number;
    std::string name;

    std::string kpreface;
    std::string repository;
    std::string kernel_config;
    std::string reproducer;
    std::string syzkaller_wd;
    std::string syzkaller_config;
    std::string wd;
    std::string syzkaller_dir;
    std::string bug_link;

    std::string syzkaller_log;

public:
    Bug_Info()
    { return; }

    void parse_config_file(const std::string &);

    int get_number() const
    { return number; }

    std::string get_name() const
    { return name; }

    std::string get_kpref() const
    { return kpreface; }

    std::string get_repo() const
    { return repository; }

    std::string get_kconfig() const
    { return kernel_config; }

    std::string get_repro() const
    { return reproducer; }

    std::string get_kallerwd() const
    { return syzkaller_wd; }

    std::string get_syzconfig() const
    { return syzkaller_config; }

    std::string get_wd() const
    { return wd; }

    std::string get_syzdir() const
    { return syzkaller_dir; }

    std::string get_buglink() const
    { return bug_link; }

    std::string get_kaller_log() const
    { return syzkaller_log; }
};

#endif