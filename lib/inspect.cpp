#include <inspect.h>
#include <consts.h>
#include <exec_api.h>
#include <shell_api.h>
#include <file_api.h>
#include <bug_info.h>
#include <inspector_config.h>
#include <fuzz_prep.h>

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cmath>

#include <string.h>
#include <unistd.h>

using namespace std;

// Quick and dirty search function for string vectors
bool inspector_is_in(const string &s, const vector<string> &v)
{
    for (string str : v)
        if (str == s)
            return true;
    
    return false;
}

int find_max_time(const vector<Syzkaller_Result> &times)
{
    // time = mean + 1 * std
    int time = 0;
    double mean = 0;
    double std = 0;

    for (Syzkaller_Result sr : times)
        mean += sr.ttf;
    mean = mean / times.size();

    for (Syzkaller_Result sr : times)
        std += pow((sr.ttf - mean), 2);

    std = std / times.size();
    std = sqrt(std);

    time = mean + 1 * std;
    time = (time < 10 ? 10 : time);

    return time;
}

int find_average_time(const vector<Syzkaller_Result> &times)
{
    int sum = 0;

    for (Syzkaller_Result sr : times)
        sum += sr.ttf;

    return sum / times.size();
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

Syzkaller_Result run_syzkaller(const Bug_Info &bug, const InspectorConfig &inspector, const vector<string> &dups, int max_time, bool poc)
{
    Syzkaller_Result ret;
    ret.ttf = 0;
    ret.found = false;
    vector<string> crash_hashes, checked_crashes;
    string crash_name;

    reset_kaller_wd(bug.get_kallerwd());
    if (poc)
        insert_POC_as_seed(bug);

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
        crash_hashes = list_dir(bug.get_kallerwd() + "/crashes");
        for (string hash : crash_hashes)
        {
            if (inspector_is_in(hash, checked_crashes))
                continue;
            else
                checked_crashes.push_back(hash);

            crash_name = get_crash_name(hash);
            ret.bugsfound.push_back(crash_name);
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

Syzkaller_Result fuzz_loop_finding(const Bug_Info &bug, const InspectorConfig &inspector, const std::vector<std::string> &dups,
                            int max_time, const VMConfig &vmc, Port_Info &port, const Date &syz_date, bool poc, bool find_max_time)
{
    vector<Syzkaller_Result> vret;
    Syzkaller_Result session_ret;
    for (int i = 0; i < FUZZTIMES; i++)
    {
        write_syzkaller_config(bug, inspector, vmc, port, syz_date);
        vret.push_back(run_syzkaller(bug, inspector, dups, max_time, poc));
    }

    session_ret.found = false;
    for (Syzkaller_Result ret : vret)
    {
        for (string s : ret.bugsfound)
            if (!inspector_is_in(s, session_ret.bugsfound))
                session_ret.bugsfound.push_back(s);

        session_ret.found = ret.found ? true : session_ret.found;
    }

    if (find_max_time)
        session_ret.ttf = find_max_time(vret);
    else
        session_ret.ttf = find_average_time(vret);

    return session_ret;
}

Syzkaller_Result fuzz_loop(const Bug_Info &bug, const InspectorConfig &inspector, const std::vector<std::string> &dups,
                            int max_time, const VMConfig &vmc, Port_Info &port, const Date &syz_date, bool poc)
{
    Syzkaller_Result ret;
    Syzkaller_Result session_ret;
    ret.found = false;
    for (int i = 0; i < FUZZTIMES & !ret.found; i++)
    {
        write_syzkaller_config(bug, inspector, vmc, port, syz_date);
        ret = run_syzkaller(bug, inspector, dups, max_time, poc);

        // keep a list of all unique bugs found this session
        for (string s : ret.bugsfound)
            if (!inspector_is_in(s, session_ret.bugsfound))
                session_ret.bugsfound.push_back(s);
    }

    // the final return value will have these for us.
    session_ret.found = ret.found;
    session_ret.ttf = ret.ttf;

    return session_ret;
}

bool check_faulty_result(const Bug_Info &bug, const vector<int> &ttfs, int max_time, const string &name)
{
    bool fault = false;

    // merge commits are bad.
    if (!name.empty() && name.substr(0, 9) == "Merge tag")
        fault = true;

    // Check if the reproducer is dependent on fault injection
    if (grep_to_find("\\\"fault_call\\\":", bug.get_repro()) && !grep_to_find("\\\"fault_call\\\":-1", bug.get_repro()))
        fault = true;

    // if the ttf gets too close to timing out, other runs
    // may have timed out without finding the bug when they
    // should not have
    for (int n : ttfs)
        if (n >= max_time * 0.7)
            fault = true;

    return fault;
}
