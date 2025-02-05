#include <fuzz.h>
#include <consts.h>
#include <exec_api.h>
#include <environment.h>
#include <shell_api.h>
#include <file_api.h>
#include <fuzz_prep.h>
#include <bisect.h>
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

std::string make_repro_log(const Environment &env)
{
    std::string reprolog = env.wd+"reprolog.prog";

    std::vector<std::string> out;
    for (std::string file : list_dir(env.reprodir))
    {
        // This lets syz-repro see when a program begins
        out.push_back("executing program 0:");
        std::vector<std::string> lines;
        load_file(file, lines);
        for (std::string line : lines)
            out.push_back(line);
        out.push_back("");
    }

    write_file(reprolog, out);
    return reprolog;
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
    cerr << "Error: Syzkaller has experienced a crash.\n" << flush;
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
Syzkaller_Result run_syzkaller(const Environment &env, bool keep_corpus)
{
    Syzkaller_Result result;
    int time = 0, to_add = 0;
    result.ttf = 0;
    result.found = false;
    result.bad_crashes = 0;
    vector<string> crash_hashes;
    map<string, int> checked_crashes;
    string crash_name;

    prepare_kaller_wd(env, keep_corpus);

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
            break;
            

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
            result.found = fuzz_is_crash_in(crash_name, env.duplicates) ? true : result.found;
            for (int j = 0; j < to_add; j++)
                result.reports.push_back({crash_name, time});

            if (fuzz_is_bad_crash(crash_name))
                result.bad_crashes += to_add;
        }

        if (wc_l(env.syzkaller_log) > 5000)
        {
            cout << "Warning: Syzkaller log file exceeded 5000 lines.\n"
                 << "Saved at " << env.logdir + env.working_name + "-boot_failure.log" << ".\n" << flush;
            copy(env.syzkaller_log, env.logdir + env.working_name + "-boot_failure.log");
            result.bad_crashes++;
            result.reports.push_back({"boot failure", time});
            break;
        }
    }
    result.ttf = time;

    if (check_alive(pid))
        kill_child(pid);
    else
        handle_syzkaller_crash();

    cd(env.home);
    delete[] arg1;
    return result;
}

Syzkaller_Result run_syz_repro(const Environment &env, const std::string &reprolog)
{
    Syzkaller_Result result;
    int time = 0, to_add = 0;
    result.ttf = 0;
    result.found = false;
    result.bad_crashes = 0;

    std::string old_dir = pwd();
    cd(env.syzdir);

    // TODO: add --repro to output the reproducer
    std::vector<std::string> cmd = {"./bin/syz-repro", "-config", env.syzconfig, reprolog};
    const char ** arg_list = new const char*[cmd.size()+1];
    for (int i = 0; i < cmd.size(); i++)
        arg_list[i] = cmd.at(i).c_str();

    arg_list[cmd.size()] = nullptr;

    int pid = exec_and_continue("./bin/syz-repro", (char **)arg_list, env.syzkaller_log, env.syzkaller_log);

    int i = 0;
    while (time < env.max_time && !result.found)
    {
        sleep(60*TIME_INCREMENT);
        time += TIME_INCREMENT;

        std::vector<std::string> loglines;
        load_file(env.syzkaller_log, loglines);
        for (; i < loglines.size(); i++)
        {
            int pos = loglines.at(i).find("program crashed: ");
            if (pos == std::string::npos)
                continue;

            std::string crash_name = loglines.at(i).substr(pos+17);
            result.found = fuzz_is_crash_in(crash_name, env.duplicates) ? true : result.found;
            result.reports.push_back({crash_name, time});

            if (fuzz_is_bad_crash(crash_name))
                result.bad_crashes += to_add;
        }

        if (wc_l(env.syzkaller_log) > 5000)
        {
            cout << "Warning: syz-repro log file exceeded 5000 lines.\n"
                    << "Saved at " << env.logdir + env.working_name + "-boot_failure.log" << ".\n" << flush;
            copy(env.syzkaller_log, env.logdir + env.working_name + "-boot_failure.log");
            result.bad_crashes++;
            result.reports.push_back({"boot failure", time});
            break;
        }
        // syz-repro stopping is normal
        if (!check_alive(pid))
            break;
    }
    result.ttf = time;

    if (check_alive(pid))
        kill_child(pid);

    cd(old_dir);
    return result;
}

Test_Result fuzz_loop_finding(Environment &env, bool keep_corpus)
{
    Test_Result result;
    result.found = false;
    result.retry = false;
    int retries = 0, unstable_count = 0;
    for (int i = 0; i < env.fuzztimes + retries && unstable_count < env.fuzztimes; i++)
    {
        env.port.inc();
        write_syzkaller_config(env);
        result.attempts.push_back(run_syzkaller(env, keep_corpus));
        result.found = result.attempts.back().found ? true : result.found;
        if (result.attempts.back().bad_crashes > 0 && retries < env.fuzztimes)
        {
            retries++;
            unstable_count++;
        }
        log_attempt_result(result.attempts.back(), i + 1, env.duplicates, env.fuzztimes);
    }

    result.stable = unstable_count < result.attempts.size() / 2 || result.found;
    result.suggest_ttf = find_max_time(result.attempts);
    return result;
}

Test_Result fuzz_loop(Environment &env, bool keep_corpus)
{
    Test_Result result;
    result.found = false;
    result.retry = false;
    int retries = 0, unstable_count = 0;
    for (int i = 0; i < env.fuzztimes + retries && !result.found  && unstable_count < env.fuzztimes; i++)
    {
        env.port.inc();
        write_syzkaller_config(env);
        result.attempts.push_back(run_syzkaller(env, keep_corpus));
        result.found = result.attempts.back().found;
        if (result.attempts.back().bad_crashes > 0 && retries < env.fuzztimes)
        {
            retries++;
            unstable_count++;
        }
        log_attempt_result(result.attempts.back(), i + 1, env.duplicates, env.fuzztimes);
    }

    result.stable = unstable_count < result.attempts.size() / 2 || result.found;
    result.suggest_ttf = find_max_time(result.attempts);
    return result;
}

Test_Result poc_loop_finding(Environment &env)
{
    Test_Result result;
    result.found = false;
    result.retry = false;

    std::string reprolog = make_repro_log(env);

    int retries = 0, unstable_count = 0;
    for (int i = 0; i < env.fuzztimes + retries && unstable_count < env.fuzztimes; i++)
    {
        env.port.inc();
        result.attempts.push_back(run_syz_repro(env, reprolog));
        result.found = result.attempts.back().found ? true : result.found;
        if (result.attempts.back().bad_crashes > 0 && retries < env.fuzztimes)
        {
            retries++;
            unstable_count++;
        }
        log_attempt_result(result.attempts.back(), i + 1, env.duplicates, env.fuzztimes);
    }

    result.stable = unstable_count < result.attempts.size() / 2 || result.found;
    result.suggest_ttf = find_max_time(result.attempts);
    remove_file(reprolog);
    return result;
}

Test_Result poc_loop(Environment &env)
{
    Test_Result result;
    result.found = false;
    result.retry = false;

    std::string reprolog = make_repro_log(env);

    int retries = 0, unstable_count = 0;
    for (int i = 0; i < env.fuzztimes + retries && !result.found  && unstable_count < env.fuzztimes; i++)
    {
        env.port.inc();
        result.attempts.push_back(run_syz_repro(env, reprolog));
        result.found = result.attempts.back().found;
        if (result.attempts.back().bad_crashes > 0 && retries < env.fuzztimes)
        {
            retries++;
            unstable_count++;
        }
        log_attempt_result(result.attempts.back(), i + 1, env.duplicates, env.fuzztimes);
    }

    result.stable = unstable_count < result.attempts.size() / 2 || result.found;
    result.suggest_ttf = find_max_time(result.attempts);
    remove_file(reprolog);
    return result;
}

/*
bool check_faulty_result()
{
    bool fault = false;

    // Check if the reproducer is dependent on fault injection
    if (grep_to_find("\\\"fault_call\\\":", bug.allreproducer) && !grep_to_find("\\\"fault_call\\\":-1", bug.allreproducer))
        fault = true;

    return fault;
}
*/
