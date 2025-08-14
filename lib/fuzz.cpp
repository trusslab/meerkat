#include <bisect.h>
#include <consts.h>
#include <environment.h>
#include <exec_api.h>
#include <file_api.h>
#include <fuzz.h>
#include <fuzz_prep.h>
#include <my_string.h>
#include <result.h>
#include <shell_api.h>
#include <syzkaller.h>
#include <vm.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
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
    // simplify. Find the max time and add 2 minutes to it
    int max = 0;

    for (Syzkaller_Result sr : times)
        max = sr.ttf > max ? sr.ttf : max;

    max = (max + 2 < 10 ? 10 : max + 2);

    return max;
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

// Reads the syzkaller log to see if any syscalls were disabled
bool check_enabled_syscalls(const Environment &env)
{
    std::vector<std::string> loglines, disabled;
    load_file(env.syzkaller_log, loglines);

    std::set<std::string> syscalls(env.base_syscalls.begin(), env.base_syscalls.end());
    for (int i = 0; i < loglines.size(); i++)
    {
        if (loglines.at(i).find("transitively disabled the following syscalls") != std::string::npos)
        {
            for (i++; i < loglines.size() && !loglines.at(i).empty(); i++)
            {
                std::string sys = split(loglines.at(i), ' ').front();
                if (syscalls.find(sys) != syscalls.end())
                    std::cout << "Warning: " << sys << " was disabled by Syzkaller.\n" << std::flush;
            }
            return true;
        }
    }
    return false;
}

bool completed_machine_check(const Environment &env)
{
    std::vector<std::string> loglines;
    load_file(env.syzkaller_log, loglines);
    for (std::string line : loglines)
    {
        if (line.find("machine check:") != std::string::npos)
            return true;
    }
    return false;
}

// The time is rough here and rounds up to the nearest time increment (usually 1 minute)
// result.ttf is the time it took to find the bug, or the max_time
Syzkaller_Result run_syzkaller(const Environment &env)
{
    Syzkaller_Result result;
    int time = 0, to_add = 0;
    bool fuzz_fail = false;
    result.ttf = 0;
    result.found = false;
    result.bad_crashes = 0;
    vector<string> crash_hashes;
    map<string, int> checked_crashes;
    string crash_name;

    string managerbin = env.syzdir + "bin/syz-manager";
    vector<string> spl = {managerbin, "-config=" + env.syzconfig};
    const char ** arg_list = new const char*[spl.size()+1];
    for (int i = 0; i < spl.size(); i++)
        arg_list[i] = spl.at(i).c_str();

    arg_list[spl.size()] = nullptr;

    int pid = exec_and_continue(managerbin, (char **)arg_list, env.syzkaller_log, env.syzkaller_log);

    // watch syzkaller's progress
    bool checked_syscalls = false;
    while (time < env.max_time && !result.found)
    {
        sleep(60*TIME_INCREMENT);
        time += TIME_INCREMENT;

        if (!checked_syscalls)
            checked_syscalls = check_enabled_syscalls(env);

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
            cout << "Warning: Syzkaller log file exceeded 5000 lines.\n" << flush;
            fuzz_fail = true;
            break;
        }
    }

    if (!completed_machine_check(env))
    {
        cout << "Warning: Syzkaller never completed machine check.\n" << flush;
        fuzz_fail = true;
    }

    if (fuzz_fail)
    {
        string logfile = env.bootfaillog();
        cout << "Saved log at " << logfile << ".\n" << flush;
        copy(env.syzkaller_log, logfile);
        result.bad_crashes++;
        result.reports.push_back({"boot failure", time});
    }

    result.ttf = time;

    if (!check_alive(pid))
        handle_syzkaller_crash();

    kill_child(pid, SIGKILL);

    delete[] arg_list;
    return result;
}

// Runs syz-repro. Returns true if it was successful at reproducing the bug
bool run_syz_repro(const Environment &env, const std::string &dest_prog, const std::string &crash_log)
{
    int time = 0;
    std::string reprobin = env.syzdir + "bin/syz-repro";
    std::vector<std::string> cmd = {reprobin, "-config", env.syzconfig, "-output", dest_prog, crash_log};
    const char ** arg_list = new const char*[cmd.size()+1];
    for (int i = 0; i < cmd.size(); i++)
        arg_list[i] = cmd.at(i).c_str();

    arg_list[cmd.size()] = nullptr;

    int pid = exec_and_continue(reprobin, (char **)arg_list, env.reprolog(), env.reprolog());

    while (time < 60)
    {
        sleep(60*TIME_INCREMENT);
        time += TIME_INCREMENT;

        // syz-repro stopping is normal
        if (!check_alive(pid, true))
            break;
    }

    if (check_alive(pid, true))
    {
        std::cout << "syz-repro was terminated after " << time << " minutes\n" << std::flush;
    }
    kill_child(pid, SIGKILL);

    delete[] arg_list;
    return check_file(dest_prog);
}

Test_Result fuzz_loop_finding(Environment &env)
{
    Test_Result result;
    result.found = false;
    result.retry = false;
    result.stable = true;

    reset_kaller_wd(env);

    int retries = 0;
    for (int i = 0; i < 3 /* hardcoded fuzztimes */; i++)
    {
        env.port.inc();
        write_syzkaller_config(env);
        prepare_kaller_wd(env);
        result.attempts.push_back(run_syzkaller(env));
        result.found = result.attempts.back().found ? true : result.found;
        log_attempt_result(result.attempts.back(), i + 1, env.duplicates, env.fuzztimes);
    }

    result.suggest_ttf = find_max_time(result.attempts);
    return result;
}

Test_Result fuzz_loop(Environment &env)
{
    Test_Result result;
    result.found = false;
    result.retry = false;
    int retries = 0, unstable_count = 0;
    for (int i = 0; i < env.fuzztimes + retries && !result.found  && unstable_count < env.fuzztimes; i++)
    {
        env.port.inc();
        write_syzkaller_config(env);
        prepare_kaller_wd(env);
        result.attempts.push_back(run_syzkaller(env));
        result.found = result.attempts.back().found;
        if (result.attempts.back().bad_crashes > 0 && retries < env.fuzztimes)
        {
            retries++;
            unstable_count++;
        }
        log_attempt_result(result.attempts.back(), i + 1, env.duplicates, env.fuzztimes);
    }

    result.stable = unstable_count < result.attempts.size() || result.found;
    result.suggest_ttf = find_max_time(result.attempts);
    return result;
}

Test_Result poc_loop(Environment &env)
{
    int err = 0;
    Test_Result result;
    result.found = false;
    result.stable = true;
    result.retry = false;

    ProgOpts opts;
    opts.from_prog(env.primary_repro);
    opts.procs = env.vmc.numProcs;

    VM_Config vmc;
    vmc.port = env.port.inc();
    vmc.image_path = env.image;
    vmc.image_key = env.image_key;
    vmc.kernel_path = env.kerneldir;
    vmc.wd_path = env.vmwd;
    // Prep the vms
    VMPool vmpool(env.vmc.numVM, vmc);

    reset_runner_wd(env);
    err = vmpool.boot_and_check_all();
    if (err < env.vmc.numVM)
        std::cerr << "Warning: Only booted " << err << " VMs.\n" << std::flush;
    if (err == 0)
    {
        vmpool.kill_all();
        result.stable = false;
        return result;
    }

    // Run the prog
    vmpool.copy_all(env.primary_repro);
    vmpool.copy_all(env.syzdir + "bin/linux_amd64/syz-execprog");
    vmpool.copy_all(env.syzdir + "bin/linux_amd64/syz-executor");
    std::string cmd = "./syz-execprog -executor=./syz-executor " + opts.execopts_string() + " " + split(env.primary_repro, '/').back();
    vmpool.run_all(cmd);
    // timeout used by syzkaller
    // https://github.com/google/syzkaller/blob/master/sys/targets/targets.go#L834
    int timeout = (5 * opts.slowdown) + 1;
    vmpool.wait_loop(timeout*60);
    std::vector<std::string> logs = vmpool.log_files();
    vmpool.kill_all();

    int unstable_count = 0;
    for (int i = 0; i < logs.size(); i++)
    {
        result.attempts.push_back(symbolize(env, logs.at(i)));
        log_attempt_result_poc(result.attempts.back(), i, env.duplicates);
        result.found = result.attempts.back().found ? true : result.found;
        if (result.attempts.back().bad_crashes > 0)
            unstable_count++;
    }

    //result.stable = unstable_count < result.attempts.size() / 2 || result.found;
    return result;
}

Syzkaller_Result symbolize(Environment &env, const std::string &file)
{
    Syzkaller_Result res;
    res.found = false;
    res.bad_crashes = 0;
    res.ttf = 0;

    std::string tmp = env.wd + "tmp_symbolize.txt";
    std::string symbolizebin = env.syzdir + "bin/syz-symbolize";
    std::string cmd = symbolizebin + " --kernel_obj " + env.kerneldir + " " + file;

    vector<string> spl = split(cmd, ' ');
    const char ** arg_list = new const char*[spl.size()+1];
    for (int i = 0; i < spl.size(); i++)
        arg_list[i] = spl.at(i).c_str();

    arg_list[spl.size()] = nullptr;

    int pid = exec_and_wait(symbolizebin, (char **)arg_list, tmp, tmp);

    std::vector<string> ignore = {"unexpected kernel reboot", "kernel panic: panic_on_warn set", "kernel panic: hung_task: blocked tasks"};
    bool do_ignore = false;
    std::vector<string> lines;
    load_file(tmp, lines);
    for (std::string line : lines)
    {
        if (starts_with(line, "TITLE: "))
        {
            do_ignore = false;
            std::string name = line.substr(7);
            for (std::string ign : ignore)
                if (name == ign)
                    do_ignore = true;
            
            if (!do_ignore)
            {
                res.reports.push_back(Crash_Report(name, 0));
                res.found = fuzz_is_crash_in(name, env.duplicates) ? true : res.found;
                res.bad_crashes += fuzz_is_bad_crash(name) ? 1 : 0;
            }
        }
    }

    remove_file(tmp);
    delete[] arg_list;
    return res;
}

