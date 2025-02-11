#ifndef VM_H
#define VM_H

#include <port.h>

#include <string>

// Status:
// Idle - VM class is created, but not booting or running
// Boot - VM has bee told to boot. No further checks have been made.
// Ready - VM has booted and is awaiting command
// Running - VM is running a job
// Crash - VM has crashed for some reason
// Done - VM has shut down normally
enum VMStatus { VM_Idle, VM_Boot, VM_Ready, VM_Running, VM_Crash, VM_Done, VM_Err };

#define VM_RETRIES 5

class VM_Config
{
public:
    unsigned int port;
    std::string image_path;
    std::string image_key;
    std::string kernel_path;
    std::string wd_path;
};

class VM
{
private:
    unsigned int pid;
    std::string logfile;

public:
    Port_Info port;
    std::string image_path;
    std::string image_key;
    std::string kernel_path;

    VM(const unsigned int, const std::string & = "", const std::string & = "", const std::string & = "", const std::string & = "");

    int boot();
    int setup();
    bool is_alive() const;
    int check_booted_once() const;
    int check_booted_loop() const;
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
    std::string debug_full() const;
};

class VMPool
{
private:
    std::vector<VM> vms;
    std::vector<VMStatus> status;
    std::vector<int> child_pids;

public:
    // Creates a pool of X VMs with the same configurations
    // Takes a number of vms and a configuration.
    VMPool(const unsigned int, const VM_Config &);

    int boot_and_check_all();

    int copy_all(const std::string &);
    int run_all(const std::string &);
    int wait_loop(unsigned int);

    int kill_all();

    void debug() const;
    std::vector<std::string> log_files() const;
    std::vector<std::string> to_symbolize() const;
};
#endif
