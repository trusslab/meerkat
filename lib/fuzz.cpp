#include <bisect.h>
#include <consts.h>
#include <environment.h>
#include <exec_api.h>
#include <file_api.h>
#include <fuzz.h>
#include <linux.h>
#include <my_string.h>
#include <dedup.h>
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

int find_max_time(const std::vector<Syzkaller_Result> &times)
{
    // simplify. Find the max time and add 2 minutes to it
    int max = 0;

    for (Syzkaller_Result sr : times)
        max = sr.ttf > max ? sr.ttf : max;

    max = (max + 2 < 10 ? 10 : max + 2);

    return max;
}

int find_average_time(const std::vector<Syzkaller_Result> &times)
{
    int sum = 0;

    for (Syzkaller_Result sr : times)
        sum += sr.ttf;

    return sum / times.size();
}

void handle_syzkaller_crash()
{
    std::cerr << "Error: Syzkaller has experienced a crash.\n" << std::flush;
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

bool fuzz_is_bad_crash(const std::string &crash_name)
{
    return crash_name.find("SYZFATAL") != std::string::npos
            || crash_name.find("SYZFAIL:") != std::string::npos || crash_name == "boot failure";
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
    std::vector<std::string> crash_hashes;
    std::map<std::string, int> checked_crashes;
    std::map<std::string, BugAlias> reuse_alias;
    BugAlias crash;

    std::string managerbin = env.syzdir + "bin/syz-manager";
    std::vector<std::string> spl = {managerbin, "-config=" + env.syzconfig};
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
        for (std::string hash : crash_hashes)
        {
            to_add = 0;
            if (checked_crashes.find(hash) == checked_crashes.end())
            {
                to_add = 1;
                checked_crashes.insert({hash, 1});
            }

            while (check_file(hash + "/log" + std::to_string(checked_crashes.at(hash))))
            {
                to_add++;
                checked_crashes.at(hash)++;
            }

            if (to_add == 0)
                continue;

            // Reuse aliases from previous loops to cut down on parsing
            if (reuse_alias.find(hash) == reuse_alias.end())
            {
                crash = BugAlias(hash);
                crash.init();
                reuse_alias.insert({hash, crash});
            }
            else
            {
                crash = reuse_alias.at(hash);
            }
            
            if (!result.found)
                result.found = deduplicate(crash, env.duplicates);
            result.reports.push_back({crash, time, to_add});

            if (fuzz_is_bad_crash(crash.name))
                result.bad_crashes += to_add;
        }

        if (wc_l(env.syzkaller_log) > 5000)
        {
            std::cout << "Warning: Syzkaller log file exceeded 5000 lines.\n" << std::flush;
            fuzz_fail = true;
            break;
        }
    }

    if (!completed_machine_check(env))
    {
        std::cout << "Warning: Syzkaller never completed machine check.\n" << std::flush;
        fuzz_fail = true;
    }

    if (fuzz_fail)
    {
        std::string logfile = env.bootfaillog();
        std::cout << "Saved log at " << logfile << ".\n" << std::flush;
        copy(env.syzkaller_log, logfile);
        result.bad_crashes++;
        BugAlias badalias = BugAlias();
        badalias.name = "boot failure";
        result.reports.push_back({badalias, time, 1});
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
        log_attempt_result(result.attempts.back(), i + 1, env);
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
        log_attempt_result(result.attempts.back(), i + 1, env);
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

    // from syzkaller: /syz-execprog -executor=/syz-executor -arch=amd64 -sandbox=none -procs=1 -repeat=0 -threaded=true -collide=false -cover=0 -optional=slowdown=1:sandboxArg=0 /syzkaller2976420098

    // Run the prog
    vmpool.copy_all(env.primary_repro);
    vmpool.copy_all(env.syzdir + "bin/linux_amd64/syz-execprog");
    vmpool.copy_all(env.syzdir + "bin/linux_amd64/syz-executor");
    std::string cmd;
    if (env.feats.old_syzkaller)
        cmd = "./syz-execprog -executor=./syz-executor -arch=amd64 -sandbox=none -procs=1 -repeat=0 -threaded=true -collide=false -cover=0 -optional=slowdown=1:sandboxArg=0 " + split(env.primary_repro, '/').back();
    else
        cmd = "./syz-execprog -executor=./syz-executor " + opts.execopts_string() + " " + split(env.primary_repro, '/').back();
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
        log_attempt_result_poc(result.attempts.back(), i, env);
        result.found = result.attempts.back().found ? true : result.found;
        if (result.attempts.back().bad_crashes > 0)
            unstable_count++;
    }

    //result.stable = unstable_count < result.attempts.size() / 2 || result.found;
    return result;
}

bool ignore_name(const std::string &name)
{
    std::set<std::string> ignore = {"unexpected kernel reboot", "kernel panic: panic_on_warn set", "kernel panic: hung_task: blocked tasks",
                                "kernel panic: Fatal exception", "kernel panic: KASAN: panic_on_warn set ...", "kernel panic: kernel: panic_on_warn set ..."};
    return ignore.count(name) > 0;
}

Syzkaller_Result symbolize(Environment &env, const std::string &file)
{
    Syzkaller_Result res;
    res.found = false;
    res.bad_crashes = 0;
    res.ttf = 0;

    // JTBURSEY: If we ever want to switch back to fuzzing after poc testing (and want to preserve corpus),
    // we will need to use prepare_kaller_wd() instead. For now this saves cycles.
    reset_kaller_wd(env);

    // ./syzkaller/bin/syz-symbolize --kernel_obj wd/kernel/ --outdir wd/wd-kaller/crashes/ vm.log
    std::string symbolizebin = env.syzdir + "bin/syz-symbolize";
    std::string cmd = symbolizebin + " --kernel_obj " + env.kerneldir + " --outdir " + env.syzwd + "crashes/ " + file;

    std::vector<std::string> spl = split(cmd, ' ');
    const char ** arg_list = new const char*[spl.size()+1];
    for (int i = 0; i < spl.size(); i++)
        arg_list[i] = spl.at(i).c_str();

    arg_list[spl.size()] = nullptr;

    int pid = exec_and_wait(symbolizebin, (char **)arg_list, "/dev/null", "/dev/null");
    
    std::vector<std::string> hashes = list_dir(env.syzwd + "crashes/");
    for (std::string hash : hashes)
    {
        BugAlias crash = BugAlias(hash);
        crash.init();

        if (ignore_name(crash.name))
            continue;
        
        res.reports.push_back({crash, 0, 1});
        if (!res.found)
            res.found = deduplicate(crash, env.duplicates);
        res.bad_crashes += fuzz_is_bad_crash(crash.name) ? 1 : 0;
    }

    delete[] arg_list;
    return res;
}

