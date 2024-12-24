#include <argparse.h>
#include <bisect.h>
#include <blocking_bugs.h>
#include <bug_info.h>
#include <consts.h>
#include <date.h>
#include <environment.h>
#include <file_api.h>
#include <fuzz_prep.h>
#include <fuzz.h>
#include <git.h>
#include <git_traverse.h>
#include <psf.h>
#include <result.h>
#include <session.h>
#include <shell_api.h>
#include <template_parse.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>

using namespace std;

Git prep_syzkaller_local_repo(const Environment &env)
{
    Git syzkaller_git(env.syzdir, SYZKALLER_REPO_REMOTE, "master");
    return syzkaller_git;
}

Git prep_kernel_local_repo(Environment &env, const Bug_Info &bug)
{
    env.linux_repo_remote = LINUX_REPO_REMOTE + bug.repository;

    Git linux_git(env.kerneldir, env.linux_repo_remote, "master");
    return linux_git;
}

int focused_fuzz_bisection(ofstream &logfile, Argparse &args, Environment &env, Bug_Info &bug, Bisect &bisector, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    Test_Result result;
    // ======================================================================================================
    // Major Release Search

    bisector.next_phase(Bisect_Releases, env, linux_git, syzkaller_git);
    if (err < 0)
    {
        cerr << "Failed to advance to Release search phase.\n" << flush;
        return -1;
    }

    // for known syzkaller, start with kernel bisection (major releases again)
    // finding -> major releases -> syzkaller bisection -> major releases -> kernel bisection
    if (args.is_set("known-syz"))
    {
        if ((err = bisector.skip_syzkaller(args.get_arg_as_string("known-syz"), syzkaller_git)) < 0)
        {
            cerr << "Could not advance to syzkaller phase via skip.\n" << flush;
            return -1;
        }
        goto skip_syzkaller;
    }

    logfile << "\n==== Major Release Search ====\n" 
            << bisector.remaining() << " Release" << (bisector.remaining() == 1 ? "" : "s") << "\n" << flush;

    while ((err = bisector.next_session(logfile, env, bug, linux_git, syzkaller_git)) == 0)
    {
        result = bisector.test_current(logfile, env, bug, linux_git);
        bisector.record(result, linux_git);
        logfile << "About " << bisector.remaining() << " releases remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    // Result: a bisect session and index into the releases.
    // Bisect session is the oldest release where the bug reproduced. Index points to it in the releases array.

    // ======================================================================================================
    // Syzkaller Bisection

    err = bisector.next_phase(Bisect_Syzkaller, env, linux_git, syzkaller_git);
    if (err < 0)
    {
        cerr << "Failed to advance to Syzkaller bisection phase.\n" << flush;
        return -1;
    }

    logfile << "\n==== Syzkaller Bisection ====\n" 
            << bisector.remaining() << " Syzkaller commit" << (bisector.remaining() == 1 ? "" : "s") << "\n" << flush;

    while ((err = bisector.next_session(logfile, env, bug, linux_git, syzkaller_git)) == 0)
    {
        result = bisector.test_current(logfile, env, bug, linux_git);
        bisector.record(result, linux_git);
        logfile << "About " << bisector.remaining() << " commits remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    bisector.lock_syzkaller();

    // ======================================================================================================
    // Major Release Search (This time for kernel commits)

skip_syzkaller:
    bisector.next_phase(Bisect_Releases, env, linux_git, syzkaller_git);
    if (err < 0)
    {
        cerr << "Failed to advance to Release search phase.\n" << flush;
        return -1;
    }

    logfile << "\n==== Major Release Search ====\n" 
            << bisector.remaining() << " Release" << (bisector.remaining() == 1 ? "" : "s") << endl
            << "Syzkaller Version: " << bisector.this_session().syzkaller.date.get_date() << " - " << bisector.this_session().syzkaller.name << endl << flush;

    while ((err = bisector.next_session(logfile, env, bug, linux_git, syzkaller_git)) == 0)
    {
        result = bisector.test_current(logfile, env, bug, linux_git);
        bisector.record(result, linux_git);
        logfile << "About " << bisector.remaining() << " releases remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    // ======================================================================================================
    // Kernel Bisection

    err = bisector.next_phase(Bisect_Kernel, env, linux_git, syzkaller_git);
    if (err < 0)
    {
        cerr << "Failed to advance to kernel bisection phase.\n" << flush;
        return -1;
    }

    logfile << "\n==== Kernel Bisection ====\n"
            << bisector.remaining() << " Linux commit" << (bisector.remaining() == 1 ? "" : "s") << endl
            << "Syzkaller Version: " << bisector.this_session().syzkaller.date.get_date() << " - " << bisector.this_session().syzkaller.name << endl << flush;

    while ((err = bisector.next_session(logfile, env, bug, linux_git, syzkaller_git)) == 0)
    {
        result = bisector.test_current(logfile, env, bug, linux_git);
        bisector.record(result, linux_git);
        logfile << "About " << bisector.stable_remaining() << " commits remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    return 0;
}

int poc_bisection(ofstream &logfile, Argparse &args, Environment &env, Bug_Info &bug, Bisect &bisector, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    Test_Result result;

    // ======================================================================================================
    // Major Release Search

    bisector.lock_syzkaller();

restart:
    bisector.next_phase(Bisect_Releases, env, linux_git, syzkaller_git);
    if (err < 0)
    {
        cerr << "Failed to advance to Release search phase.\n" << flush;
        return -1;
    }

    logfile << "\n==== Major Release Search ====\n" 
            << bisector.remaining() << " Release" << (bisector.remaining() == 1 ? "" : "s") << "\n" << flush;

    while ((err = bisector.next_session(logfile, env, bug, linux_git, syzkaller_git)) == 0)
    {
        result = bisector.test_current(logfile, env, bug, linux_git);
        bisector.record(result, linux_git);
        logfile << "About " << bisector.remaining() << " releases remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    // ======================================================================================================
    // Kernel Bisection

    err = bisector.next_phase(Bisect_Kernel, env, linux_git, syzkaller_git);
    if (err < 0)
    {
        cerr << "Failed to advance to kernel bisection phase.\n" << flush;
        return -1;
    }

    logfile << "\n==== Kernel Bisection ====\n"
            << bisector.remaining() << " Linux commit" << (bisector.remaining() == 1 ? "" : "s") << endl << flush;

    while ((err = bisector.next_session(logfile, env, bug, linux_git, syzkaller_git)) == 0)
    {
        result = bisector.test_current(logfile, env, bug, linux_git);
        bisector.record(result, linux_git);
        logfile << "About " << bisector.stable_remaining() << " commits remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    // Kernel bisection should give a bisect version and a good version (no bug).
    // We fuzz on the good commit to try to find a repro, then restart if found.
    // Otherwise, bisect version is the result

    // ======================================================================================================
    // Syzkaller Fuzz Test

    err = bisector.next_phase(Bisect_Syzkaller, env, linux_git, syzkaller_git);
    if (err < 0)
    {
        cerr << "Failed to advance to kernel bisection phase.\n" << flush;
        return -1;
    }

    logfile << "\n==== Syzkaller Fuzzing ====\n" << flush;

    err = bisector.next_session(logfile, env, bug, linux_git, syzkaller_git);
    if (err < 0)
    {
        cerr << "Failed to get or build Syzkaller session.\n" << flush;
        return -1;
    }
    result = bisector.test_current(logfile, env, bug, linux_git);
    bisector.record(result, linux_git);

    if (result.found)
    {
        bisector.lock_syzkaller();
        goto restart;
    }

    return 0;
}

int bisect(Argparse &args, Environment &env, Bug_Info &bug)
{
    int err = 0;
    string starttime = date("%Y-%m-%d %T");
    ofstream logfile;
    Bisect bisector;
    Test_Result result;

    env.logfilename = env.logdir + bug.numName + ".log";
    logfile.open(env.logfilename);
    if(!logfile)
    {
        cerr << "Error: Failed to open log file " << env.logfilename << ".\n";
        return -1;
    }

    // Set up syzkaller and linux repositories locally
    Git syzkaller_git = prep_syzkaller_local_repo(env);
    Git linux_git = prep_kernel_local_repo(env, bug);
    if (linux_git.error() < 0 || syzkaller_git.error() < 0)
        goto finish;

    // begin logging
    logfile << "Bisecting: " << bug.name << "," << bug.buglink << endl
            << args.origin() << "\n\n"
            << "Repository:      " << bug.kpreface << endl
            << "Arch:            " << bug.arch << endl
            << "Finding:         " << linux_git.get_commit_date(bug.find_hash).get_date() << " - " << bug.find_hash << endl << flush;
    if (args.is_set("known-syz"))
            logfile << "Using Syzkaller: " << syzkaller_git.get_commit_date(args.get_arg_as_string("known-syz")).get_date() << " - " << args.get_arg_as_string("known-syz") << endl << flush;

    if (false)
    {
        // Parse Syzbot for duplicate bugs
        cout << "Gathering bug fixes from Syzbot.\n";
        gather_duplicates(env, bug);
    }
    else
    {
        bug.duplicates.clear();
        bug.duplicates.push_back(bug.name);
    }

    if (bug.duplicates.size() > 1)
    {
        cout << "Duplicate Bugs:\n";
        logfile << "Duplicate Names:\n";
        for (string s : bug.duplicates)
        {
            cout << "    " << s << endl;
            logfile << "    " << s << endl;
        }
    }
    else
    {
        cout << "No duplicate bugs found.\n";
        logfile << "No Duplicates\n";
    }

    env.port.init(START_PORT, env.id, 5);

    determine_threadedness(env, bug, logfile);

    logfile << "Max time:        " << env.max_time << endl 
            << "Max attempts:    " << env.fuzztimes << endl << flush;
    
    // ======================================================================================================
    // Begin Bisection
    // ======================================================================================================

    cout << "Initializing Bisector...\n";
    bisector.init(env, bug, linux_git);
    if (bisector.releases.size() <= 1)
        goto finish;
    
    cout << "Found " << bisector.releases.size() << " major releases\n" << flush;

    if (!args.is_set("algorithm") || bisector.set_algorithm(args.get_arg_as_string("algorithm")) < 0)
    {
        cerr << "Error: invalid algorithm choice\n" << flush;
        goto finish;
    }

    // ======================================================================================================
    // Fuzz at the finding commit
    
    reset_kaller_wd(env);
    bisector.next_phase(Bisect_Finding, env, linux_git, syzkaller_git);
    if (err < 0)
    {
        cerr << "Failed to advance to finding commit phase.\n" << flush;
        goto finish;
    }

    logfile << "\n==== Finding Commit ====\n" << flush;

    // Begin with Syzkaller from finding date. This emulates Syzbot for any algorithm
    if (bisector.next_session(logfile, env, bug, linux_git, syzkaller_git) < 0)
    {
        cerr << "Error: Failed to go to finding commit\n" << flush;
        goto finish;
    }

    cout << SPACER;
    if (bisector.algorithm() == ALG_SETUP)
    {
        write_syzkaller_config(env, bug, bisector.this_session().syzkaller.date);
        logfile << "Setup-only complete.\n" << flush;
        cout << "Setup complete.\n";
        goto setup_only_finish;
    }

    result = bisector.test_current(logfile, env, bug, linux_git);

    if (bisector.algorithm() == ALG_FINDING)
    {
        logfile << "Average TTF: " << result.suggest_ttf << endl;
        cout << "Find-only complete.\n";
        goto finish;
    }

    if (!result.found)
    {
        cout << "This bug cannot be found at the finding commit. Ignoring this bug.\n";
        logfile << "\nFailure: This bug cannot be found at the finding commit.\n" << flush;
        goto finish;
    }
    else
    {
        logfile << "\nNew Max Time: " << env.max_time << "\n" << flush;
    }

    bisector.record(result, linux_git);

    // ======================================================================================================
    // Break off here by bisection algorithm
    switch(bisector.algorithm())
    {
    case ALG_FF_CLEAN:
    case ALG_FF_STATEFUL:
        err = focused_fuzz_bisection(logfile, args, env, bug, bisector, linux_git, syzkaller_git);
        break;
    case ALG_BISECT_FF:
    case ALG_SYZ_BISECT:
        err = poc_bisection(logfile, args, env, bug, bisector, linux_git, syzkaller_git);
        break;
    }
    if (err < 0)
        goto finish;

    // ======================================================================================================
    // Finish
    // ======================================================================================================

    bisector.next_phase(Bisect_Done, env, linux_git, syzkaller_git);

    logfile << "\n" << SPACER
            << bisector.print_result(env, bug, linux_git, starttime) << flush;

    logfile << "\nPossible Blocking Bugs:\n";
    for (string b : bug.blocking_bugs.list_blocking_bugs())
        logfile << "    " << b << endl;

finish:
    cout << SPACER
        << "Cleaning up...";

    // clean up reproducer and config
    if (!check_file(env.wd + "old/"))
        make_dir(env.wd + "old/");

    if (check_file(bug.kconfig))
        move(bug.kconfig, env.wd + "old/");
    
    if (check_file(bug.allreproducer))
        move(bug.allreproducer, env.wd + "old/");

    remove_files_in_dir(bug.reproducer);

setup_only_finish:
    if (logfile)
    {
        logfile << flush;
        logfile.close();
    }
    
    cout << "Done.\n" << flush;
    return err;
}

void print_help()
{
    cout << "Usage: ./bin/syzInspector -c [config] -i [id]\n"
        << "    -m [max_time]: the maximum time (minutes) allowed when fuzzing (default 30).\n"
        << "    -i [id]: REQUIRED. The id of the inspector (i.e. 1).\n"
        << "    --config (c): [config]: REQUIRED. The config file containing the bug information.\n"
        << "    --algorithm: REQUIRED. the bisection algorithm to use.\n"
        << "    --safe-mode: use safe mode.\n"
        << "    --known-syz: use a specific syzkaller hash.\n"
        << endl << flush;
}

int handle_bug_config(Environment &env, Bug_Info &bug, const Argparse &args)
{
    int err = 0;
    string filename;
    if (args.is_set("config"))
        filename = args.get_arg_as_string("config");
    else if (args.is_set('c'))
        filename = args.get_arg_as_string('c');
    else
    {
        cerr << "Error: No config file given (use --config)\n" << flush;
        return -1;
    }

    err = env.parse_config_file(filename);
    if (err < 0)
        return err;
    err = bug.parse_config_file(env, filename);
    if (err < 0)
        return err;
    
    env.syzkaller_log = env.logdir + bug.numName + "-kaller.log";
    return err;
}

int main(int argc, char ** argv)
{
    Argparse args;
    Environment env;
    Bug_Info bug;

    args.expect("mihc");
    args.expect(vector<string>({ "help", "config", "algorithm", "safe-mode", "try-patch", "known-syz" }));
    args.parse(argc, argv);
    if (args.is_set('h') || args.is_set("help"))
    {
        print_help();
        return 0;
    }

    // This clarifies some nonsense with commit times
    set_timezone("UTC0");

    env.fuzztimes = 3;

    if (args.is_set('m'))
        env.max_time = args.get_arg_as_int('m');
    else
        env.max_time = 30;

    if (args.is_set('i'))
        env.id = args.get_arg_as_int('i');
    else
    {
        cout << "Error: No id given. Please use -i [id]\n";
        return -1;
    }

    // get config for how to run
    cout << SPACER
         << "Parsing configs.\n";
    if (env.parse_parameters_file("parameters/config.cfg") < 0)
        return -1;

    // get information about the bug
    if (handle_bug_config(env, bug, args) < 0)
        return -1;

    if (bug.name.find("KMSAN") != string::npos)
        env.compiler_setting = COMPILER_CLANG_14;
    else
        env.compiler_setting = COMPILER_GCC;

    export_go(env);
    env.origin_path = get_path();

    if (args.is_set("safe-mode") || bug.name.substr(0, 11) == "memory leak")
        set_safe_mode(env.safe_mode, env.max_time, env.fuzztimes);

    env.try_patch = args.is_set("try-patch");

    // make sure all of the needed files are here.
    cout << "Checking files.\n";
    if (!check_file(env.logdir))
    {
        cout << "Creating log directory.\n";
        make_dir(env.logdir);
    }
    else
        cout << "Found log directory.\n";

    if (!check_file(bug.allreproducer))
    {
        cerr << "Error: No reproducer file " << bug.allreproducer << " exists.\n";
        return -1;
    }
    else
        cout << "Found reproducer.\n";

    if (!check_file(bug.kconfig))
    {
        cerr << "Error: No kernel config file " << bug.kconfig << " exists.\n";
        return -1;
    }
    else
        cout << "Found kernel config.\n";

    if (!check_file(env.image_dir + "stretch/stretch.img"))
    {
        cerr << "Error: No image file for stretch.\n";
        return -1;
    }
    else
        cout << "Found stretch image.\n";

    if (!check_file(env.image_dir + "wheezy/wheezy.img"))
    {
        cerr << "Error: No image file for wheezy.\n";
        return -1;
    }
    else
        cout << "Found wheezy image.\n";

    return bisect(args, env, bug);
}
