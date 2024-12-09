#include <fuzz.h>
#include <consts.h>
#include <exec_api.h>
#include <environment.h>
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
#include <map>
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

void handle_syzkaller_crash(ostream &logfile)
{
    cerr << "Error: Syzkaller has experienced a crash.\n" << flush;
    logfile << "Error: Syzkaller has experienced a crash.\n" << flush;
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

// The time is rough here and rounds up to the nearest time increment (usually 1 minute)
// result.ttf is the time it took to find the bug, or the max_time
Syzkaller_Result run_syzkaller(ofstream &logfile, const Environment &env,const Bug_Info &bug, const InspectorConfig &inspector)
{
    Syzkaller_Result result;
    int time = 0, to_add = 0;
    result.ttf = 0;
    result.found = false;
    result.bad_crashes = 0;
    vector<string> crash_hashes;
    map<string, int> checked_crashes;
    string crash_name;

    prepare_kaller_wd(env, bug);

    // run syzkaller
    cd(env.syzdir);

    char command[] = "./bin/syz-manager";
    string configArg = "-config=" + env.syzconfig;
    char * arg1 = new char[configArg.size() + 1];
    strcpy(arg1, configArg.c_str());
    char * arg_list[] = {command, arg1, nullptr};

    cout << "Running Syzkaller...\n";
    int pid = exec_and_continue("./bin/syz-manager", arg_list, env.syzkaller_log, env.syzkaller_log);

    // watch syzkaller's progress
    while (time < env.max_time && !result.found)
    {
        sleep(60*TIME_INCREMENT);
        time += TIME_INCREMENT;

        // make sure syzkaller stays alive
        if (!check_alive(pid))
            handle_syzkaller_crash(logfile);

        // list the unique crashes Syzkaller has found
        crash_hashes = list_dir(env.syzwd + "/crashes");
        for (string hash : crash_hashes)
        {
            to_add = 0;
            if (checked_crashes.find(hash) == checked_crashes.end())
            {
                to_add = 1;
                checked_crashes.insert({hash, 1});
            }

            while (check_file(hash + "/log" + to_string(checked_crashes.at(hash))))
            {
                to_add++;
                checked_crashes.at(hash)++;
            }

            if (to_add == 0)
                continue;
            
            crash_name = get_crash_name(hash);
            result.found = fuzz_is_crash_in(crash_name, bug.duplicates) ? true : result.found;
            for (int j = 0; j < to_add; j++)
                result.reports.push_back({crash_name, time});

            if (fuzz_is_bad_crash(crash_name))
                result.bad_crashes += to_add;
        }

        if (wc_l(env.syzkaller_log) > 5000)
        {
            cout << "Warning: Syzkaller log file exceeded 5000 lines. Assuming boot failure\n";
            logfile << "Warning: Syzkaller log file exceeded 5000 lines.\n"
                    << "Saved at " << env.wd + "/log/" + bug.numName + "-boot_failure.log" << ".\n" << flush;
            copy(env.syzkaller_log, env.wd + "/log/" + bug.numName + "-boot_failure.log");
            result.bad_crashes++;
            result.reports.push_back({"boot failure", time});
            break;
        }
    }
    result.ttf = time;

    if (check_alive(pid))
        kill_child(pid);
    else
        handle_syzkaller_crash(logfile);

    cd(inspector.get_inspect_dir());
    delete[] arg1;
    return result;
}

Test_Result fuzz_loop_finding(ofstream &logfile, const Environment &env, const Bug_Info &bug, InspectorConfig &inspector,
                            const Date &syz_date)
{
    Test_Result result;
    result.found = false;
    result.retry = false;
    int retries = 0, unstable_count = 0;
    for (int i = 0; i < env.fuzztimes + retries && unstable_count < env.fuzztimes; i++)
    {
        inspector.port.inc();
        write_syzkaller_config(env, bug, inspector, syz_date);
        result.attempts.push_back(run_syzkaller(logfile, env, bug, inspector));
        result.found = result.attempts.back().found ? true : result.found;
        if (result.attempts.back().bad_crashes > 0 && retries < env.fuzztimes)
        {
            retries++;
            unstable_count++;
        }
        log_attempt_result(logfile, result.attempts.back(), i + 1, bug.duplicates, env.fuzztimes);
    }

    result.stable = unstable_count < result.attempts.size() / 2 || result.found;
    result.suggest_ttf = (env.find_only ? find_average_time(result.attempts) : find_max_time(result.attempts));

    return result;
}

Test_Result fuzz_loop(ofstream &logfile, const Environment &env, const Bug_Info &bug, InspectorConfig &inspector,
                    const Date &syz_date)
{
    Test_Result result;
    result.found = false;
    result.retry = false;
    int retries = 0, unstable_count = 0;
    for (int i = 0; i < env.fuzztimes + retries && !result.found  && unstable_count < env.fuzztimes; i++)
    {
        inspector.port.inc();
        write_syzkaller_config(env, bug, inspector, syz_date);
        result.attempts.push_back(run_syzkaller(logfile, env, bug, inspector));
        result.found = result.attempts.back().found;
        if (result.attempts.back().bad_crashes > 0 && retries < env.fuzztimes)
        {
            retries++;
            unstable_count++;
        }
        log_attempt_result(logfile, result.attempts.back(), i + 1, bug.duplicates, env.fuzztimes);
    }

    result.stable = unstable_count < result.attempts.size() / 2 || result.found;
    result.suggest_ttf = find_max_time(result.attempts);

    return result;
}

bool check_faulty_result(const Bug_Info &bug)
{
    bool fault = false;

    // Check if the reproducer is dependent on fault injection
    if (grep_to_find("\\\"fault_call\\\":", bug.allreproducer) && !grep_to_find("\\\"fault_call\\\":-1", bug.allreproducer))
        fault = true;

    return fault;
}
