#include <vm.h>
#include <exec_api.h>
#include <my_string.h>

#include <iostream>
#include <string>
#include <vector>

#include <string.h>
#include <unistd.h>

VM::VM(const std::string &k, const std::string &i, const std::string &ik, const std::string &wd)
{
    n = 0;
    pid = 0;
    port = 12800;
    kernel_path = k;
    image_path = i;
    image_key = ik;
    wd_path = wd;
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
                        "-net nic,model=e1000 -net user,host=10.0.2.10,hostfwd=tcp::" + std::to_string(port) + "-:22 -hda "
                        + image_path + " -snapshot -kernel " + kernel + " -append";

    std::vector<std::string> spl = split(argstr, ' ');
    spl.push_back(app_args);

    // const here for the assignment
    const char ** arg_list = new const char*[spl.size()+1];
    for (int i = 0; i < spl.size(); i++)
        arg_list[i] = spl.at(i).c_str();

    arg_list[spl.size()] = nullptr;

    int err = exec_and_continue("qemu-system-x86_64", (char**)arg_list, log_file(), "/dev/null");

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
    return check_booted();
}

bool VM::is_alive() const
{
    return pid != 0 && check_alive(pid);
}

int VM::check_booted() const
{
    int err;
    for (int i = 0; i < RETRIES; i++)
    {
        err = run("exit", false, true);
        if (err == 0)
            return 0;
        sleep(30);
    }
    std::cerr << "Warning: VM has not responded after " << RETRIES << " attempts.\n" << std::flush;
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
    n++;
    port++;

    return res;
}

// Reset the vm and vm log number. For use with a new test case.
int VM::reset()
{
    int res = 0;
    if (is_alive())
        res = kill();
    pid = 0;
    n = 0;
    port++;

    return res;
}

// scp -P port -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -i ../../images/bullseye/bullseye.id_rsa file root@localhost:/root
int VM::scp(const std::string &file, const std::string &dest) const
{
    std::string argstr = "scp -P " + std::to_string(port) + " -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "
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
    std::string argstr = "ssh -i " + image_key + " -q -p " + std::to_string(port) + " -o";
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
    return wd_path + "vm-" + std::to_string(n) + ".log";
}

std::string VM::debug() const
{
    return "Kernel: " + kernel_path + "\n"
            + "Port: " + std::to_string(port) + "\n"
            + "PID: " + std::to_string(pid) + "\n"
            + "Log: " + log_file();
}
