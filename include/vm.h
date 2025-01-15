#ifndef VM_H
#define VM_H

#include <environment.h>

#include <string>

class VM
{
private:
    const unsigned int RETRIES = 5;
    unsigned int pid;
    unsigned int n;
    std::string wd_path;

public:
    unsigned int port;
    std::string image_path;
    std::string image_key;
    std::string kernel_path;

    VM(const std::string & = "", const std::string & = "", const std::string & = "", const std::string & = "");

    int boot();
    int setup();
    bool is_alive() const;
    int check_booted() const;
    int kill();

    int reboot();
    int reset();

    int scp(const std::string &, const std::string & = "/root") const;
    int run(const std::string &, const std::string &, const std::string &, bool = false, bool = false) const;
    int run(const std::string &, const std::string &, bool = false, bool = false) const;
    int run(const std::string &, bool = false, bool = false) const;
    int kill_proc(const std::string &) const;

    std::string log_file() const;
    std::string debug() const;
};

#endif
