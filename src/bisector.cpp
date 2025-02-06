#include <argparse.h>
#include <bisect.h>
#include <consts.h>
#include <date.h>
#include <environment.h>
#include <file_api.h>
#include <fuzz_prep.h>
#include <fuzz.h>
#include <git.h>
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
#include <iomanip>
#include <fstream>
#include <chrono>

using namespace std;

Git prep_kernel_local_repo(Environment &env)
{
    Git linux_git(env.kerneldir, env.repository, env.branch);
    return linux_git;
}

int check_syzkaller(const Environment &env)
{
    vector<string> syzbins = { "syz-sysgen", "syz-symbolize", "syz-repro", "syz-prog2c", "syz-mutate", "syz-manager", "syz-db", "linux_amd64/syz-execprog", "linux_amd64/syz-executor" };
    for (string bin : syzbins)
    {
        if (!check_file(env.syzdir + "bin/" + bin))
        {
            cerr << "Error: ";
            return -1;
        }
    }
    return 0;
}

int uniqify_reproducers(const Environment &env)
{
    vector<string> repros = list_dir(env.reprodir);
    if (repros.size() <= 0)
    {
        cerr << "Error: No reproducer files found.\n" << flush;
        return -1;
    }

    vector<string> keep, remove;
    bool removed = false;
    keep.push_back(repros.front());
    for (int i = 1; i < repros.size(); i++)
    {
        removed = false;
        for (string kept : keep)
        {
            if (compare_files(kept, repros.at(i)))
            {
                remove.push_back(repros.at(i));
                removed = true;
                break;
            }
        }
        if (!removed)
            keep.push_back(repros.at(i));
    }

    for (string r : remove)
        remove_file(r);

    return 0;
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

int do_bisection(Environment &env, Bisect &bisector, Git &linux_git)
{
    int err = 0;
    Test_Result result;

    // ======================================================================================================
    // Fuzz at the anchor commit

    reset_kaller_wd(env);
    bisector.next_phase(Bisect_Anchor, env, linux_git);
    if (err < 0)
    {
        cerr << "Failed to advance to anchor commit phase.\n" << flush;
        return err;
    }

    cout << "\n==== Anchor Commit ====\n" << flush;

    // Begin with Syzkaller from finding date. This emulates Syzbot for any algorithm
    if (bisector.next_session(env, linux_git) < 0)
    {
        cerr << "Error: Failed to go to anchor commit\n" << flush;
        return err;
    }

    if (env.feats.setup_only)
    {
        write_syzkaller_config(env);
        cout << "Setup-only complete.\n" << flush;
        return 1;
    }

    result = bisector.test_current(env, linux_git);

    if (env.feats.find_only)
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

    while ((err = bisector.next_session(env, linux_git)) == 0)
    {
        result = bisector.test_current(env, linux_git);
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

    bisector.next_phase(Bisect_Releases, env, linux_git);
    if (err < 0)
    {
        cerr << "Failed to advance to Release search phase.\n" << flush;
        return -1;
    }

    cout << "\n==== Major Release Search ====\n" 
         << bisector.remaining() << " Release" << (bisector.remaining() == 1 ? "" : "s") << endl << flush;

    while ((err = bisector.next_session(env, linux_git)) == 0)
    {
        result = bisector.test_current(env, linux_git);
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

    while ((err = bisector.next_session(env, linux_git)) == 0)
    {
        result = bisector.test_current(env, linux_git);
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

int bisect(Environment &env)
{
    int err = 0;
    Bisect bisector;
    Test_Result result;

    chrono::steady_clock::time_point starttime = chrono::steady_clock::now();

    Git linux_git = prep_kernel_local_repo(env);
    if (linux_git.error() < 0)
        goto finish;

    // begin logging
    cout << left << setw(CONFW) << "Bisecting:" << env.name << endl;

    env.duplicates.clear();
    env.duplicates.push_back(env.name);
    // Manual aliases would go here

    // TODO: handle port a different way (we will have multiple vms)
    env.port.init(START_PORT, env.id, 5);

    // TODO: Maybe don't need this
    determine_threadedness(env);

    // ======================================================================================================
    // Begin Bisection
    // ======================================================================================================

    bisector.init(env, linux_git);
    if (bisector.releases.size() <= 1)
    {
        cerr << "Failed to find Linux releases.\n" << flush;
        goto finish;
    }

    if (check_syzkaller(env) < 0)
        goto finish;

    if (uniqify_reproducers(env) < 0)
        goto finish;

    if (env.feats.ff_test)
        bisector.set_mode(Mode_FF);
    else if (env.feats.poc_test)
        bisector.set_mode(Mode_PoC);
    else
    {
        cerr << "Error: Invalid bisection mode state.\n" << flush;
        goto finish;
    }

    env.print();
    return 0;

    // ======================================================================================================
    // Break off here by mode
    
    if (bisector.mode() == Mode_FF)
    {
        env.required_syscalls = get_reproduer_syscall_descriptions(env);
        do_bisection(env, bisector, linux_git);
    }

    if (env.feats.poc_test)
        bisector.set_mode(Mode_PoC);

    if (bisector.mode() == Mode_PoC)
    {
        do_bisection(env, bisector, linux_git);
    }

    if (err < 0)
        goto finish;

    // ======================================================================================================
    // Finish
    // ======================================================================================================

    bisector.next_phase(Bisect_Done, env, linux_git);

    cout << "\n" << SPACER
         << bisector.print_result(env, linux_git, starttime) << flush;

finish:
    return err;
}

void print_help()
{
    cout << "Usage: ./bin/" << PROJECT_NAME << " -c [config] -i [id]\n"
        << "    -m [max time]: the maximum time (minutes) allowed when fuzzing (default 30).\n"
        << "    -i [id]: REQUIRED. The id of the inspector (i.e. 1).\n"
        << "    --config (c) [config.cfg]: [config]: REQUIRED. The config file containing the bug information.\n"
        << "    --anchor (a) [hash]: REQUIRED. the hash of the commit where the bug was found.\n"
        << "    --feature (F) [feature list]: extra features to use.\n"
        << endl << flush;
}

int handle_config(Environment &env, const Argparse &args)
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

    return err;
}

int handle_features(Argparse &args, Environment &env)
{
    string flist;
    if (args.is_set("feature") || args.is_set('F'))
    {
        flist = args.is_set("feature") ? args.get_string("feature") : args.get_string('F');
    }

    set<string> features;
    for (string feat : split(flist, ','))
        features.insert(feat);

    return env.handle_features(features);
}

int main(int argc, char ** argv)
{
    Argparse args;
    Environment env;

    args.expect("mihcaF");
    args.expect(vector<string>({ "help", "config", "feature", "anchor", "safe-mode" }));
    args.parse(argc, argv);
    if (args.is_set('h') || args.is_set("help"))
    {
        print_help();
        return 0;
    }

    env.init();

    if (args.is_set('m'))
        env.max_time = args.get_int('m');
    else
        env.max_time = 30;

    if (args.is_set('i'))
        env.id = args.get_int('i');
    else
    {
        cerr << "Error: No id was given. Please use -i [id]\n" << flush;
        return -1;
    }

    if (args.is_set('a') || args.is_set("anchor"))
        env.anchor_hash = args.is_set('a') ? args.get_string('a') : args.get_string("anchor");
    else
    {
        cerr << "Error: No anchor commit was given. Use -a [hash]\n" << flush;
    }

    // get information about the bug
    if (handle_config(env, args) < 0)
        return -1;

    if (handle_features(args, env) < 0)
        return -1;

    if (env.name.find("KMSAN") != string::npos)
        env.compiler_setting = COMPILER_CLANG_14;
    else
        env.compiler_setting = COMPILER_GCC;

    if (args.is_set("safe-mode") || env.name.substr(0, 11) == "memory leak")
        set_safe_mode(env.safe_mode, env.max_time, env.fuzztimes);
    else
        env.safe_mode = false;

    // make sure all of the needed files are here.
    if (!check_file(env.logdir))
    {
        make_dir(env.logdir);
    }

    if (!check_file(env.kconfig))
    {
        cerr << "Error: No kernel config file " << env.kconfig << " exists.\n";
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

    cout << args.origin() << endl;
    return bisect(env);
}
