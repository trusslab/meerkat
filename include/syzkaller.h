#ifndef SYZKALLER_H
#define SYZKALLER_H

#include <json.h>

#include <string>
#include <vector>

// Library for interfacing with syzkaller

class ProgOpts
{
public:
    bool threaded;              // create threaded program
    unsigned int repeat;        // repeat program that many times (<=0 - infinitely)
    unsigned int procs;         // number of parallel processes
    unsigned int slowdown;      // execution slowdown caused by emulation/instrumentation
    std::string sandbox;        // sandbox to use (none, setuid, namespace, android)
    unsigned int sandbox_arg;   // argument for executor to customize its behavior
    bool segv;                  // catch and ignore SIGSEGV
    bool tmpdir;                // create a temporary dir and execute inside it

    // addtl. features
    bool tun;
    bool net_dev;
    bool net_reset;
    bool cgroups;
    bool binfmt_misc;
    bool close_fds;
    bool devlink_pci;
    bool nic_vf;
    bool usb;
    bool vhci;
    bool wifi;
    bool ieee802154;
    bool sysctl;
    bool swap;

    // legacy features
    bool collide;
    bool fault;
    int fault_call;
    int fault_nth;

    ProgOpts();
    ProgOpts(const std::string &);

    void reset();
    int from_syz_repro(const std::string &);
    int from_prog(const std::string &);
    int from_json(const JSON &);

    void enable_all();
    void sanitize_opts();

    bool any_enabled() const;
    bool all_enabled() const;

    std::string enable_string() const;
    void compile_execopts(std::vector<std::string> &) const;
    std::string execopts_string() const;

private:
    //std::string enable_string() const;
};

std::string opts_from_syz_repro(const std::string &);

#endif
