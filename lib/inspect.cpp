#include <inspect.h>
#include <consts.h>
#include <exec_api.h>
#include <shell_api.h>
#include <file_api.h>
#include <bug_info.h>
#include <inspector_config.h>

#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include <string.h>
#include <unistd.h>

using namespace std;

bool inspector_is_in(const string &s, const vector<string> &v)
{
    for (string str : v)
        if (str == s)
            return true;
    
    return false;
}

void handle_syzkaller_crash()
{
    cerr << "Error: Syzkaller has experienced a crash.\n";
    exit(-1);
}

string get_crash_name(const string &hash)
{
    ifstream inf;
    string filename = hash + "/description";
    string line;
    inf.open(filename);
    if (!inf)
    {
        cerr << "Error: Failed to open file " << filename << ".\n";
        return "";
    }

    // description file only has one line
    getline(inf, line);
    return line;
}

void reset_kaller_wd(const string &wd)
{
    if (check_file(wd))
    {
        cout << "Reseting Syzkaller's working directory.\n";
        remove_dir(wd);
    }

    make_dir(wd);
    make_dir(wd + "/crashes");
    return;
}

int write_syzkaller_config(const Bug_Info &bug, const InspectorConfig &inspector, const VMConfig &vmc, Port_Info &p)
{
    p.port = p.start_port + p.port_count;
    p.port_count = (p.port_count + 1) % (FUZZTIMES + 1);

    ofstream outf;
    outf.open(bug.get_syzconfig());
    if (!outf)
    {
        cerr << "Error: Failed to open file " << bug.get_syzconfig() << ".\n";
        return -1;
    }

    outf << "{\n";

    // if(target flag exists) // target was added on 2017-09-15
    outf << "    \"target\": \"linux/amd64\",\n";
    // endif

    outf << "    \"http\": \"127.0.0.1:" << p.port << "\",\n"
         << "    \"workdir\": \"" << bug.get_kallerwd() << "\",\n";

    // "vmlinux" until 2018-06-27, then "kernel_obj" starting on 2018-06-28
    //if (( $($inspectdir/helpers/diffdate $sdate "2018-06-28") >= 0 )); then
        outf << "    \"kernel_obj\": \"" << bug.get_kerneldir() << "\",\n";
    //else
    //    outf << "    \"vmlinux\": \"" << bug.get_kerneldir() << "/vmlinux\",\n";
    //endif

    // change image when syzkaller did. It shouldn't matter, but who knows.
    //if (( $($inspectdir/helpers/diffdate $sdate "2018-09-04") >= 0 )); then
        outf << "    \"image\": \"" << inspector.get_inspect_dir() << "/image/stretch/stretch.img\",\n"
             << "    \"sshkey\": \"" << inspector.get_inspect_dir() << "/image/stretch/stretch.id_rsa\",\n";
    //else
    //    outf << "    \"image\": \"" << inspector.get_inspect_dir() << "/image/wheezy/wheezy.img\",\n"
    //         << "    \"sshkey\": \"" << inspector.get_inspect_dir() << "/image/wheezy/ssh/id_rsa\",\n";
    //fi

    outf << "    \"syzkaller\": \"" << bug.get_syzdir() << "\",\n"
         << "    \"procs\": " << vmc.numProcs << ",\n"
         << "    \"type\": \"qemu\",\n"
         << "    \"reproduce\": false,\n"
         << "    \"vm\": {\n"
         << "        \"count\": " << vmc.numVM << ",\n"
         << "        \"kernel\": \"" << bug.get_kerneldir() << "/arch/x86/boot/bzImage\",\n"
         << "        \"cpu\": " << vmc.numCPU << ",\n"
         << "        \"mem\": " << inspector.get_mem() << "\n"
         << "    }\n"
         << "}";

    outf.close();
    return 0;
}

Syzkaller_Result run_syzkaller(const Bug_Info &bug, const InspectorConfig &inspector, const vector<string> &dups, int max_time)
{
    Syzkaller_Result ret;
    ret.ttf = 0;
    ret.found = false;
    vector<string> crash_hashes;
    string crash_name;

    reset_kaller_wd(bug.get_kallerwd());

    // run syzkaller
    cd(bug.get_syzdir());

    char command[] = "./bin/syz-manager";
    string configArg = "-config=" + bug.get_syzconfig();
    char * arg1 = new char[configArg.size() + 1];
    strcpy(arg1, configArg.c_str());
    char * arg_list[] = {command, arg1, nullptr};

    cout << "Running Syzkaller...\n";
    int pid = exec_and_continue("./bin/syz-manager", arg_list, bug.get_kaller_log(), bug.get_kaller_log());

    // watch syzkaller's progress
    while (ret.ttf < max_time && !ret.found)
    {
        sleep(60*TIME_INCREMENT);
        ret.ttf += TIME_INCREMENT;

        // make sure syzkaller stays alive
        if (!check_alive(pid))
            handle_syzkaller_crash();

        // check crashes
        // I let the loop finish checking even if we find our bug
        crash_hashes = list_dir(bug.get_kallerwd() + "/crashes");
        for (string hash : crash_hashes)
        {
            if (inspector_is_in(hash, ret.bugsfound))
                continue;
            else
                ret.bugsfound.push_back(hash);

            crash_name = get_crash_name(hash);
            if (inspector_is_in(crash_name, dups))
                ret.found = true;
        }
    }

    if (check_alive(pid))
        kill_child(pid);
    else
        handle_syzkaller_crash();

    cd(inspector.get_inspect_dir());
    delete[] arg1;
    return ret;
}

Syzkaller_Result fuzz_loop(const Bug_Info &bug, const InspectorConfig &inspector, const std::vector<std::string> &dups, int max_time, const VMConfig &vmc, Port_Info &port)
{
    Syzkaller_Result ret;
    Syzkaller_Result session_ret;
    ret.found = false;
    for (int i = 0; i < FUZZTIMES & !ret.found; i++)
    {
        write_syzkaller_config(bug, inspector, vmc, port);
        ret = run_syzkaller(bug, inspector, dups, max_time);

        // keep a list of all unique bugs found this session
        for (string s : ret.bugsfound)
            if (!inspector_is_in(s, session_ret.bugsfound))
                session_ret.bugsfound.push_back(s);
    }

    // the final return value will have these for us.
    session_ret.found = ret.found;
    session_ret.ttf - ret.ttf;

    return session_ret;
}