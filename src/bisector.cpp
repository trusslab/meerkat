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
#include <my_string.h>
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
#include <chrono>

using namespace std;

Git prep_kernel_local_repo(Environment &env, const Bug_Info &bug)
{
    env.linux_repo_remote = LINUX_REPO_REMOTE + bug.repository;

    Git linux_git(env.kerneldir, env.linux_repo_remote, "master");
    return linux_git;
}

/* Bisection Workflow
 * 
 * Input:
 *  Bug Name
 *  Bug Aliases (Duplicates, optional)
 *  Anchor (Finding Commit)
 *  Linux Repository
 *  Kernel Config
 *  1 or more syz reproducers
 * 
 *  Features
 * 
 *  Workdir
 *  Built Syzkaller (Custom)
 *  Compilers
 *  
 * 
 * Features:
 *  PoC Mode (default)
 *  Focused Fuzzing (req. dedup)
 *  Stateful Corpus (req. FF)
 *  Deduplication
 *  All PoCs
 *  Optimize Fuzzer (procs, no gen, etc.)
 *  Optimize Repro (procs)
 *  Kernel Patches (will be off for experiment)
 *
 * Setup:
 *  Read features and config files.
 *  Check all files.
 *      Pull linux
 *      Make sure Syzkaller is built
 *  Keep only unique reproducers.
 * 
 * Finding Commit:
 *  Test with FF first (if FF feature)
 * 
 * FF Bisection:
 *  Do FF bisection. (release and then git)
 *  Every X tests run syz-repro to find champion PoC (add to corpus)
 *  Return Introducing commit (first bad commit) and last good commit
 * 
 * PoC Test:
 *  Attempt to reproduce the bug with champion PoC
 *  Commit is either last good commit from FF or Finding commit
 * 
 * PoC Bisection:
 *  Do PoC bisection
 *  Return introducing commit
 * 
 * Return Introducing Commit
 */

int do_bisection(Argparse &args, Environment &env, Bug_Info &bug, Bisect &bisector, Git &linux_git)
{
    int err = 0;
    Test_Result result;

    // ======================================================================================================
    // Fuzz at the anchor commit

    reset_kaller_wd(env);
    bisector.next_phase(Bisect_Finding, env, linux_git);
    if (err < 0)
    {
        cerr << "Failed to advance to anchor commit phase.\n" << flush;
        return err;
    }

    cout << "\n==== Anchor Commit ====\n" << flush;

    // Begin with Syzkaller from finding date. This emulates Syzbot for any algorithm
    if (bisector.next_session(env, bug, linux_git) < 0)
    {
        cerr << "Error: Failed to go to anchor commit\n" << flush;
        return err;
    }

    if (false /* TODO: Make a feature */)
    {
        write_syzkaller_config(env, bug);
        cout << "Setup-only complete.\n" << flush;
        return 1;
    }

    result = bisector.test_current(env, bug, linux_git);

    if (false /* TODO: Make a feature */)
    {
        cout << "Average TTF: " << result.suggest_ttf << endl;
        cout << "Find-only complete.\n";
        return 1;
    }

    if (!result.found)
    {
        cout << "\nFailure: This bug cannot be found at the anchor commit.\n" << flush;
        return 1;
    }
    else
    {
        cout << "\nNew Max Time: " << env.max_time << "\n" << flush;
    }

    bisector.record(result, linux_git);

    // ======================================================================================================
    // Major Release Search

    bisector.next_phase(Bisect_Releases, env, linux_git);
    if (err < 0)
    {
        cerr << "Failed to advance to Release search phase.\n" << flush;
        return -1;
    }

    cout << "\n==== Major Release Search ====\n" 
         << bisector.remaining() << " Release" << (bisector.remaining() == 1 ? "" : "s") << "\n" << flush;

    while ((err = bisector.next_session(env, bug, linux_git)) == 0)
    {
        result = bisector.test_current(env, bug, linux_git);
        bisector.record(result, linux_git);
        cout << "About " << bisector.remaining() << " releases remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    // Result: a bisect session and index into the releases.
    // Bisect session is the oldest release where the bug reproduced. Index points to it in the releases array.

    // ======================================================================================================
    // Major Release Search (This time for kernel commits)

skip_syzkaller:
    bisector.next_phase(Bisect_Releases, env, linux_git);
    if (err < 0)
    {
        cerr << "Failed to advance to Release search phase.\n" << flush;
        return -1;
    }

    cout << "\n==== Major Release Search ====\n" 
         << bisector.remaining() << " Release" << (bisector.remaining() == 1 ? "" : "s") << endl << flush;

    while ((err = bisector.next_session(env, bug, linux_git)) == 0)
    {
        result = bisector.test_current(env, bug, linux_git);
        bisector.record(result, linux_git);
        cout << "About " << bisector.remaining() << " releases remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    // ======================================================================================================
    // Kernel Bisection

    err = bisector.next_phase(Bisect_Kernel, env, linux_git);
    if (err < 0)
    {
        cerr << "Failed to advance to kernel bisection phase.\n" << flush;
        return -1;
    }

    cout << "\n==== Kernel Bisection ====\n"
         << bisector.remaining() << " Linux commit" << (bisector.remaining() == 1 ? "" : "s") << endl << flush;

    while ((err = bisector.next_session(env, bug, linux_git)) == 0)
    {
        result = bisector.test_current(env, bug, linux_git);
        bisector.record(result, linux_git);
        cout << "About " << bisector.remaining() << " commits remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    return 0;
}

int bisect(Argparse &args, Environment &env, Bug_Info &bug)
{
    int err = 0;
    Bisect bisector;
    Test_Result result;

    chrono::steady_clock::time_point starttime = chrono::steady_clock::now();

    Git linux_git = prep_kernel_local_repo(env, bug);
    if (linux_git.error() < 0)
        goto finish;

    // begin logging
    cout << "Bisecting:       " << bug.name << endl
         << "Syzbot Link:     " << bug.buglink << endl
         << args.origin() << "\n\n"
         << "Repository:      " << bug.repository << endl
         << "Arch:            " << bug.arch << endl
         << "anchor:          " << linux_git.get_commit_date(bug.find_hash).get_date() << " - " << bug.find_hash << endl << flush;

    bug.duplicates.clear();
    bug.duplicates.push_back(bug.name);
    // Manual aliases would go here

    if (bug.duplicates.size() > 1)
    {
        cout << "Duplicate Bugs:\n";
        for (string s : bug.duplicates)
            cout << "    " << s << endl;
    }
    else
        cout << "No duplicate bugs found.\n";

    // TODO: handle port a different way (we will have multiple vms)
    env.port.init(START_PORT, env.id, 5);

    // TODO: Maybe don't need this
    determine_threadedness(env, bug);

    cout << "Max time:        " << env.max_time << endl 
         << "Max attempts:    " << env.fuzztimes << endl << flush;
    
    // ======================================================================================================
    // Begin Bisection
    // ======================================================================================================

    bisector.init(env, bug, linux_git);
    if (bisector.releases.size() <= 1)
        goto finish;
    
    cout << "Found " << bisector.releases.size() << " major releases\n" << flush;

    // TODO: set bisector mode

    // TODO: Make sure Syzkaller is built (find all executables needed)

    // ======================================================================================================
    // Break off here by bisection mode
    
    if (bisector.mode() == Mode_FF)
    {
        // Do FF bisection
    }

    if (bisector.mode() == Mode_PoC)
    {
        // Do PoC bisection
    }

    if (err < 0)
        goto finish;

    // ======================================================================================================
    // Finish
    // ======================================================================================================

    bisector.next_phase(Bisect_Done, env, linux_git);

    cout << "\n" << SPACER
         << bisector.print_result(env, bug, linux_git, starttime) << flush;

    if (bug.blocking_bugs.list_blocking_bugs().size() > 0)
    {
        cout << "\nPossible Blocking Bugs:\n";
        for (string b : bug.blocking_bugs.list_blocking_bugs())
            cout << "    " << b << endl;
    }

finish:
    // clean up reproducer and config
    if (!check_file(env.wd + "old/"))
        make_dir(env.wd + "old/");

    if (check_file(bug.kconfig))
        move(bug.kconfig, env.wd + "old/");

    remove_files_in_dir(bug.reprodir);

setup_only_finish:
    return err;
}

void print_help()
{
    cout << "Usage: ./bin/bisector -c [config] -i [id]\n"
        << "    -m [max time]: the maximum time (minutes) allowed when fuzzing (default 30).\n"
        << "    -i [id]: REQUIRED. The id of the inspector (i.e. 1).\n"
        << "    --config (c) [config.cfg]: [config]: REQUIRED. The config file containing the bug information.\n"
        << "    --anchor (a) [hash]: the hash of the commit where the bug was found.\n"
        << "    --feature (F) [feature list]: extra features to use.\n"
        << endl << flush;
}

int handle_bug_config(Environment &env, Bug_Info &bug, const Argparse &args)
{
    int err = 0;
    string filename;
    if (args.is_set("config"))
        filename = args.get_string("config");
    else if (args.is_set('c'))
        filename = args.get_string('c');
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

    return err;
}

int default_features(Environment &env)
{
    return 0;
}

int handle_features(Argparse &args, Environment &env)
{
    if (!args.is_set("feature") && !args.is_set('F'))
    {
        default_features(env);
    }

    string flist = args.is_set("feature") ? args.get_string("feature") : args.get_string('f');
    set<string> features;
    for (string feat : split(flist, ','))
        features.insert(feat);

    // PoC usage will be determined in manager/by user

    if (features.find("all") != features.end())
    {

    }
    
    if (features.find("poc-test") != features.end())
    {

    }

    if (features.find("ff-test") != features.end())
    {

    }

    if (features.find("stateful-corpus") != features.end())
    {

    }

    if (features.find("deduplication") != features.end())
    {

    }

    if (features.find("optimize-syzkaller") != features.end())
    {

    }

    if (features.find("patch-kernel") != features.end())
    {

    }

    // check that at least one feature is set

    return 0;
}

int main(int argc, char ** argv)
{
    Argparse args;
    Environment env;
    Bug_Info bug;

    args.expect("mihcaF");
    args.expect(vector<string>({ "help", "config", "feature", "anchor", "safe-mode" }));
    args.parse(argc, argv);
    if (args.is_set('h') || args.is_set("help"))
    {
        print_help();
        return 0;
    }

    env.fuzztimes = 3;

    if (args.is_set('m'))
        env.max_time = args.get_int('m');
    else
        env.max_time = 30;

    if (args.is_set('i'))
        env.id = args.get_int('i');
    else
    {
        cout << "Error: No id given. Please use -i [id]\n";
        return -1;
    }

    if (env.parse_parameters_file("parameters/config.cfg") < 0)
        return -1;

    // get information about the bug
    if (handle_bug_config(env, bug, args) < 0)
        return -1;

    if (handle_features(args, env) < 0)
        return -1;

    if (bug.name.find("KMSAN") != string::npos)
        env.compiler_setting = COMPILER_CLANG_14;
    else
        env.compiler_setting = COMPILER_GCC;

    if (args.is_set("safe-mode") || bug.name.substr(0, 11) == "memory leak")
        set_safe_mode(env.safe_mode, env.max_time, env.fuzztimes);
    else
        env.safe_mode = false;

    // make sure all of the needed files are here.
    if (!check_file(env.logdir))
    {
        make_dir(env.logdir);
    }

    if (!check_file(bug.kconfig))
    {
        cerr << "Error: No kernel config file " << bug.kconfig << " exists.\n";
        return -1;
    }

    if (!check_file(env.image_dir + "stretch/stretch.img"))
    {
        cerr << "Error: No image file for stretch.\n";
        return -1;
    }

    if (!check_file(env.image_dir + "wheezy/wheezy.img"))
    {
        cerr << "Error: No image file for wheezy.\n";
        return -1;
    }

    return bisect(args, env, bug);
}
