#include <fuzz.h>
#include <consts.h>
#include <exec_api.h>
#include <shell_api.h>
#include <file_api.h>
#include <bug_info.h>
#include <inspector_config.h>
#include <fuzz_prep.h>
#include <retrospect.h>
#include <result.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <fstream>
#include <cmath>

#include <string.h>
#include <unistd.h>

using namespace std;

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

// The time is rough here and rounds up to the nearest time increment (usually 1 minute)
// result.ttf is the time it took to find the bug, or the max_time
Syzkaller_Result run_syzkaller(const Bug_Info &bug, const InspectorConfig &inspector, const vector<string> &dups, const int max_time, bool poc)
{
    Syzkaller_Result result;
    int time = 0, to_add = 0, i = 0;
    result.ttf = 0;
    result.found = false;
    result.bad_crashes = 0;
    vector<string> crash_hashes;
    vector<Crash_Report> checked_crashes;
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
    while (time < max_time && !result.found)
    {
        sleep(60*TIME_INCREMENT);
        time += TIME_INCREMENT;

        // make sure syzkaller stays alive
        if (!check_alive(pid))
            handle_syzkaller_crash();

        // check crashes
        crash_hashes = list_dir(bug.get_kallerwd() + "/crashes");
        for (string hash : crash_hashes)
        {
            to_add = 0;
            i = cr_find(hash, checked_crashes);
            if (i >= 0)
            {
                // reusing crash_report becuase it has the same data layout that I need, even if it is the wrong name.
                // The hash string here has the whole path in it. Realistically this deserves a fix, but this will work for now.
                while (check_file(hash + "/log" + to_string(checked_crashes.at(i).time)))
                {
                    to_add++;
                    checked_crashes.at(i).time++;
                }

                if (to_add == 0)
                    continue;
            }
            else
            {
                to_add = 1;
                checked_crashes.push_back({hash, 1});
            }

            crash_name = get_crash_name(hash);
            result.found = fuzz_is_in(crash_name, dups) ? true : result.found;
            for (int j = 0; j < to_add; j++)
                result.reports.push_back({crash_name, time});

            if (fuzz_is_bad_crash(crash_name))
                result.bad_crashes += to_add;
        }
    }
    result.ttf = time;

    if (check_alive(pid))
        kill_child(pid);
    else
        handle_syzkaller_crash();

    cd(inspector.get_inspect_dir());
    delete[] arg1;
    return result;
}

Test_Result fuzz_loop_finding(ofstream &logfile, const Bug_Info &bug, const InspectorConfig &inspector, const vector<string> &dups,
                            const int max_time, const int fuzztimes, const VMConfig &vmc, Port_Info &port, const Date &syz_date, bool poc, bool find_only)
{
    Test_Result result;
    result.found = false;
    int retries = 0, unstable_count = 0;
    for (int i = 0; i < fuzztimes + retries; i++)
    {
        port.inc();
        write_syzkaller_config(bug, inspector, vmc, port, syz_date);
        result.attempts.push_back(run_syzkaller(bug, inspector, dups, max_time, poc));
        result.found = result.attempts.back().found ? true : result.found;
        if (result.attempts.back().bad_crashes > 0 && retries < fuzztimes)
        {
            retries++;
            unstable_count++;
        }
        log_attempt_result(logfile, result.attempts.back(), i + 1, dups, fuzztimes);
    }

    result.stable = unstable_count < result.attempts.size() / 2 || result.found;
    result.suggest_ttf = (find_only ? find_average_time(result.attempts) : find_max_time(result.attempts));

    return result;
}

Test_Result fuzz_loop(ofstream &logfile, const Bug_Info &bug, const InspectorConfig &inspector, const vector<string> &dups,
                    const int max_time, const int fuzztimes, const VMConfig &vmc, Port_Info &port, const Date &syz_date, bool poc)
{
    Test_Result result;
    result.found = false;
    int retries = 0, unstable_count = 0;
    for (int i = 0; i < fuzztimes + retries & !result.found; i++)
    {
        port.inc();
        write_syzkaller_config(bug, inspector, vmc, port, syz_date);
        result.attempts.push_back(run_syzkaller(bug, inspector, dups, max_time, poc));
        result.found = result.attempts.back().found;
        if (result.attempts.back().bad_crashes > 0 && retries < fuzztimes)
        {
            retries++;
            unstable_count++;
        }
        log_attempt_result(logfile, result.attempts.back(), i + 1, dups, fuzztimes);
    }

    result.stable = unstable_count < result.attempts.size() / 2 || result.found;
    result.suggest_ttf = find_max_time(result.attempts);

    return result;
}

bool check_faulty_result(const Bug_Info &bug)
{
    bool fault = false;

    // Check if the reproducer is dependent on fault injection
    if (grep_to_find("\\\"fault_call\\\":", bug.get_allrepro()) && !grep_to_find("\\\"fault_call\\\":-1", bug.get_allrepro()))
        fault = true;

    return fault;
}
