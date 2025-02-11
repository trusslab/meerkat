#include <exec_api.h>
#include <file_api.h>
#include <my_string.h>
#include <port.h>
#include <vm.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>

#include <string.h>
#include <unistd.h>

VM::VM(const unsigned int p, const std::string &k, const std::string &i, const std::string &ik, const std::string &lf)
{
    pid = 0;
    port.init(p);
    kernel_path = k;
    image_path = i;
    image_key = ik;
    logfile = lf;
}

int VM::boot()
{
    std::string app_args = "earlyprintk=serial oops=panic nmi_watchdog=panic panic_on_warn=1 panic=1 ftrace_dump_on_oops=orig_cpu "
                            "rodata=n vsyscall=native net.ifnames=0 biosdevname=0 root=/dev/sda console=ttyS0 kvm-intel.nested=1 "
                            "kvm-intel.unrestricted_guest=1 kvm-intel.vmm_exclusive=1 kvm-intel.fasteoi=1 kvm-intel.ept=1 "
                            "kvm-intel.flexpriority=1 kvm-intel.vpid=1 kvm-intel.emulate_invalid_guest_state=1 kvm-intel.eptad=1 "
                            "kvm-intel.enable_shadow_vmcs=1 kvm-intel.pml=1 kvm-intel.enable_apicv=1";

    std::string kernel = kernel_path + "arch/x86/boot/bzImage";
    // Added ",sockets=2,cores=1" to "-smp 2" to fix boot error on nested VM.
    // https://groups.google.com/u/1/g/syzkaller/c/H8gmw9_JRoY/m/dQc5n5_6AAAJ
    std::string argstr = "qemu-system-x86_64 "
                        "-m 4096 -smp 2,sockets=2,cores=1 -display none -serial stdio -no-reboot -enable-kvm -cpu host,migratable=off "
                        "-net nic,model=e1000 -net user,host=10.0.2.10,hostfwd=tcp::" + std::to_string(port.port) + "-:22 -hda "
                        + image_path + " -snapshot -kernel " + kernel + " -append";

    std::vector<std::string> spl = split(argstr, ' ');
    spl.push_back(app_args);

    // const here for the assignment
    const char ** arg_list = new const char*[spl.size()+1];
    for (int i = 0; i < spl.size(); i++)
        arg_list[i] = spl.at(i).c_str();

    arg_list[spl.size()] = nullptr;

    int err = exec_and_continue("qemu-system-x86_64", (char**)arg_list, log_file(), log_file());

    if (err > 0)
        pid = err;

    delete[] arg_list;
    return (err < 0 ? -1 : 0);
}

int VM::setup()
{
    int err = boot();
    if (err < 0)
        return -1;
    sleep(60);
    return check_booted_loop();
}

bool VM::is_alive() const
{
    return pid != 0 && check_alive(pid);
}

int VM::check_booted_once() const
{
    return run("exit", false, true) == 0 ? 0 : -1;
}

int VM::check_booted_loop() const
{
    int err;
    for (int i = 0; i < VM_RETRIES; i++)
    {
        err = check_booted_once();
        if (err == 0)
            return 0;
        sleep(30);
    }
    std::cerr << "Warning: VM has not responded after " << VM_RETRIES << " attempts.\n" << std::flush;
    return -1;
}

int VM::kill()
{
    if (pid != 0)
        return kill_child(pid);
    else
        return -1;
}

// To use the VM again for the same test case (does not actually boot again. TODO: rename better)
int VM::reboot()
{
    int res = 0;
    if (is_alive())
        res = kill();
    pid = 0;
    port.inc();

    return res;
}

// Reset the vm and vm log number. For use with a new test case.
int VM::reset()
{
    int res = 0;
    if (is_alive())
        res = kill();
    pid = 0;
    port.inc();

    return res;
}

// scp -P port -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -i ../../images/bullseye/bullseye.id_rsa file root@localhost:/root
int VM::scp(const std::string &file, const std::string &dest) const
{
    std::string argstr = "scp -P " + std::to_string(port.port) + " -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "
                        "-i " + image_key + " " + file + " root@localhost:" + dest;
    std::vector<std::string> spl = split(argstr, ' ');

    const char ** arg_list = new const char*[spl.size()+1];
    for (int i = 0; i < spl.size(); i++)
        arg_list[i] = spl.at(i).c_str();

    arg_list[spl.size()] = nullptr;

    int err = exec_and_wait("scp", (char**)arg_list, "/dev/null", "/dev/null");
    if (err != 0)
        std::cerr << "Warning: scp exited with error status 0x" << std::hex << err << std::endl << std::dec << std::flush;

    delete[] arg_list;
    return (err != 0 ? -1 : 0);
}

int VM::run(const std::string &cmd, const std::string &outfile, const std::string &errfile, bool bg, bool quiet) const
{
    std::string argstr = "ssh -i " + image_key + " -q -p " + std::to_string(port.port) + " -o";
    std::vector<std::string> spl = split(argstr, ' ');
    spl.push_back("StrictHostKeyChecking no");
    spl.push_back("root@localhost");
    if (!cmd.empty())
        spl.push_back(cmd);

    const char ** arg_list = new const char*[spl.size()+1];
    for (int i = 0; i < spl.size(); i++)
        arg_list[i] = spl.at(i).c_str();

    arg_list[spl.size()] = nullptr;

    int err = 0;
    if (bg)
        err = exec_and_continue("ssh", (char**)arg_list, outfile, errfile);
    else
        err = exec_and_wait("ssh", (char**)arg_list, outfile, errfile);

    if (!quiet && !bg && err == 0xff)
        std::cerr << "Warning: ssh exited with error status 0x" << std::hex << err << std::endl << std::dec << std::flush;

    delete[] arg_list;
    return (err < 0 ? -1 : err);
}

// ssh -i ../../images/bullseye/bullseye.id_rsa -p port -o "StrictHostKeyChecking no" root@localhost "./test_case"
int VM::run(const std::string &cmd, const std::string &outfile, bool bg, bool quiet) const
{
    return run(cmd, outfile, "/dev/null", bg, quiet);
}

int VM::run(const std::string &cmd, bool bg, bool quiet) const
{
    return run(cmd, "/dev/null", bg, quiet);
}

// rm -f prog; pkill -9 -f prog
int VM::kill_proc(const std::string &prog) const
{
    return run("rm -f " + prog + "; pkill -9 -f " + prog);
}

std::string VM::log_file() const
{
    return logfile;// wd_path + "vm-" + std::to_string(n) + ".log"
}

#define VMW 12

std::string VM::debug() const
{
    std::stringstream ss;
    ss << std::left << std::setw(VMW) << "Kernel:" << kernel_path << std::endl
        << std::left << std::setw(VMW) << "Port:" << std::to_string(port.port) << std::endl
        << std::left << std::setw(VMW) << "PID:" << std::to_string(pid) << std::endl
        << std::left << std::setw(VMW) << "Log:" << log_file() << std::endl;
    return  ss.str();
}

std::string VM::debug_full() const
{
    std::stringstream ss;
    ss << std::left << std::setw(VMW) << "Kernel:" << kernel_path << std::endl
        << std::left << std::setw(VMW) << "Image:" << image_path << std::endl
        << std::left << std::setw(VMW) << "Image Key:" << image_key << std::endl
        << std::left << std::setw(VMW) << "Port:" << std::to_string(port.port) << std::endl
        << std::left << std::setw(VMW) << "PID:" << std::to_string(pid) << std::endl
        << std::left << std::setw(VMW) << "Log:" << log_file() << std::endl;
    return  ss.str();
}

VMPool::VMPool(const unsigned int num, const VM_Config &cfg)
{
    unsigned int port = cfg.port;
    for (int i = 0; i < num; i++)
    {
        vms.push_back(VM(port, cfg.kernel_path, cfg.image_path, cfg.image_key, cfg.wd_path + "vm-" + std::to_string(i) + ".log"));
        port += PORT_RANGE;
        status.push_back(VM_Idle);
    }
}

int VMPool::boot_and_check_all()
{
    int count = 0, expected = vms.size();
    // boot
    for (int i =0; i < vms.size(); i++)
    {
        if (status.at(i) == VM_Idle)
        {
            status.at(i) = VM_Boot;
            vms.at(i).boot();
        }
        else
        {
            std::cerr << "Error: Could not boot vm " << i << ". Unexpected Status " << status.at(i) << ".\n" << std::flush;
            expected--;
        }
    }

    if (expected == 0)
        return -1;

    // check booted
    for (int r = 0; r < VM_RETRIES; r++)
    {
        sleep(15);
        for (int i = 0; i < vms.size(); i++)
        {
            if (status.at(i) == VM_Boot)
            {
                if (vms.at(i).check_booted_once() == 0)
                {
                    status.at(i) = VM_Ready;
                    count++;
                }
            }
        }
        if (count == expected)
            break;
    }
    if (count != expected)
        std::cerr << "Warning: VM has not responded after " << VM_RETRIES << " attempts.\n" << std::flush;
    return count;
}

int VMPool::copy_all(const std::string &file)
{
    int err = 0;
    if (!check_file(file))
        return -1;

    for (int i = 0; i < vms.size(); i++)
    {
        if (status.at(i) == VM_Ready)
        {
            if (vms.at(i).scp(file) < 0)
            {
                status.at(i) = VM_Err;
            }
        }
    }
    return 0;
}

int VMPool::run_all(const std::string &cmd)
{
    int err = 0;
    child_pids.clear();
    child_pids.resize(vms.size());
    for (int i = 0; i < vms.size(); i++)
    {
        if (status.at(i) == VM_Ready)
        {
            err = vms.at(i).run(cmd, true);
            if (err < 0)
            {
                status.at(i) = VM_Err;
                continue;
            }
            child_pids.at(i) = err;
            status.at(i) = VM_Running;
        }
    }
    return 0;
}

// Wait for child procs to finish or crash or timeout.
// Timeout is given in seconds
int VMPool::wait_loop(unsigned int timeout)
{
    int expect = 0;
    for (int i = 0; i < status.size(); i++)
        expect += status.at(i) == VM_Running ? 1 : 0;
    
    int count = 0, time = 0;
    while (expect > 0 && count < expect && time < timeout)
    {
        sleep(5);
        for (int i = 0; i < vms.size(); i++)
        {
            if (status.at(i) != VM_Running)
                continue;

            if (!vms.at(i).is_alive())
            {
                status.at(i) = VM_Crash;
                count++;
                continue;
            }
            if (!check_alive(child_pids.at(i)))
            {
                status.at(i) = VM_Done;
                count++;
                continue;
            }
        }
        time += 5;
    }
    return 0;
}

int VMPool::kill_all()
{
    for (int i =0; i < vms.size(); i++)
    {
        if (status.at(i) == VM_Ready || status.at(i) == VM_Running || status.at(i) == VM_Err)
        {
            vms.at(i).kill();
            status.at(i) = VM_Done;
        }
    }
    return 0;
}

void VMPool::debug() const
{
    std::cout << "VMPool Debug:\n\n" << std::flush;
    for (int i = 0; i < vms.size(); i++)
        std::cout << "VM " << i << ":\n" << vms.at(i).debug_full() 
                << std::left << std::setw(VMW) << "Status:" << status.at(i) << std::endl << std::endl << std::flush;
}

std::vector<std::string> VMPool::log_files() const
{
    std::vector<std::string> files;
    for (VM vm : vms)
        files.push_back(vm.log_file());
    return files;
}

std::vector<std::string> VMPool::to_symbolize() const
{
    std::vector<std::string> files;
    for (int i = 0; i < vms.size(); i++)
        if (status.at(i) == VM_Ready || status.at(i) == VM_Crash)
            files.push_back(vms.at(i).log_file());
    return files;
}
