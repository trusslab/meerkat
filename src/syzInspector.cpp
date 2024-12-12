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
#include <git_api.h>
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

int prep_syzkaller_local_repo(const Environment &env)
{
    if (!check_file(env.syzdir))
    {
        cout << "Creating Syzkaller directory.\n";
        make_dir(env.syzdir);
    }
    else
        cout << "Found Syzkaller directory.\n";

    if (!check_file(env.syzdir + "/.git"))
    {
        cout << "Cloning Syzkaller repository...\n";
        if (git_clone(SYZKALLER_REPO_REMOTE, env.syzdir) < 0)
        {
            cerr << "Error: Git clone failed.\n";
            return -1;
        }
    }
    else
        cout << "Found Syzkaller local repository.\n";
    
    return 0;
}

int prep_kernel_local_repo(Environment &env, const Bug_Info &bug)
{
    env.linux_repo_remote = LINUX_REPO_REMOTE + bug.repository;

    if (!check_file(env.kerneldir))
    {
        cout << "Creating kernel directory.\n";
        make_dir(env.kerneldir);
    }
    else
        cout << "Found kernel directory.\n";

    // this may cause issues when the repository opened is not the one we want
    if (!check_file(env.kerneldir + "/.git"))
    {
        cout << "Cloning Linux repository...\n";
        if (git_clone(env.linux_repo_remote, env.kerneldir) < 0)
        {
            cerr << "Error: Git clone failed.\n";
            return -1;
        }
    }
    else
        cout << "Found Linux local repository.\n";
    
    return 0;
}

int bisect(Argparse &args, Environment &env, Bug_Info &bug)
{
    int err = 0, git_err = 0;
    string starttime = date("%Y-%m-%d %T");
    ofstream logfile;
    Bisect bisector;
    Test_Result result;

    env.logfilename = env.wd + "/log/" + bug.numName + ".log";
    logfile.open(env.logfilename);
    if(!logfile)
    {
        cerr << "Error: Failed to open log file " << env.logfilename << ".\n";
        return -1;
    }

    // Set up syzkaller repository locally
    err = prep_syzkaller_local_repo(env);
    if (err < 0)
        goto finish;

    // Set up linux repository locally
    err = prep_kernel_local_repo(env, bug);
    if (err < 0)
        goto finish;

    // begin logging
    logfile << "Bisecting: " << bug.name << "," << bug.buglink << endl
            << args.origin() << "\n\n"
            << "Repository:      " << bug.kpreface << endl
            << "Arch:            " << bug.arch << endl
            << "Finding:         " << git_get_commit_date(env.wd, env.kerneldir, bug.find_hash).get_date() << " - " << bug.find_hash << endl
            << "Guilty:          " << git_get_commit_date(env.wd, env.kerneldir, bug.guilty_hash).get_date() << " - " << bug.guilty_hash << endl 
            << "Using Syzkaller: " << git_get_commit_date(env.wd, env.syzdir, args.get_arg_as_string("known-syz")).get_date() << " - " << args.get_arg_as_string("known-syz") << endl << flush;

    // Parse Syzbot for duplicate bugs
    cout << "Gathering bug fixes from Syzbot.\n";
    gather_duplicates(env, bug);

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
    if (args.is_set("known-syz"))
    {
        bisector.init(env, bug, args.is_set('f'), args.get_arg_as_string("known-syz"));
    }
    else
    {
        bisector.init(env, bug, args.is_set('f'));
    }
    if (bisector.kernel_versions.size() == 0)
    {
        logfile << "Error: Failed to gather kernel versions.\n" << flush;
        goto finish;
    }
    else if (bisector.syzkaller_versions.size() == 0)
    {
        logfile << "Error: Failed to gather syzkaller versions.\n" << flush;
        goto finish;
    }
    cout << "Found " << bisector.kernel_versions.size() << " kernel commits.\n";
    cout << "Found " << bisector.syzkaller_versions.size() << " Syzkaller commits.\n";

    // ======================================================================================================
    // Fuzz at the finding commit
    
    reset_kaller_wd(env);
    bisector.next_phase(Bisect_Finding);
    if (err < 0)
    {
        cerr << "Failed to advance to finding commit phase.\n" << flush;
        goto finish;
    }

    logfile << "\n==== Finding Commit ====\n" << flush;

    bisector.next_session(logfile, env, bug);

    cout << SPACER;
    if (args.is_set("setup-only"))
    {
        write_syzkaller_config(env, bug, bisector.this_session().syzkaller.date);
        logfile << "Setup-only complete.\n" << flush;
        cout << "Setup complete.\n";
        goto setup_only_finish;
    }

    result = bisector.test_current(logfile, env, bug);

    if (env.find_only)
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

    bisector.record(result);
    
    //  ======================================================================================================
    // Find Merge Commit

    if (!env.no_merge)
    {
        cout << "Looking for Merge Commit...\n";
        Version merge_commit = bisector.find_merge_commit(env, bug);
        if (!merge_commit.name.empty())
        {
            cout << "Merge commit found: " << merge_commit.name << ".\n";
            logfile << "Merge Commit: " << merge_commit.date.get_date() << " - " << merge_commit.name << "\n" << flush;
        }
        else
        {
            cout << "No merge commit found.\n";
            logfile << "No Merge Commit.\n" << flush;
        }
    }

    // ======================================================================================================
    // Syzkaller Bisection

    if (args.is_set("known-syz"))
    {
        if ((err = bisector.skip_syzkaller()) < 0)
        {
            cerr << "Could not advance to syzkaller phase via skip.\n" << flush;
            goto finish;
        }
        goto skip_syzkaller;
    }

    err = bisector.next_phase(Bisect_Syzkaller);
    if (err < 0)
    {
        cerr << "Failed to advance to Syzkaller bisection phase.\n" << flush;
        goto finish;
    }

    logfile << "\n==== Syzkaller Bisection ====\n" 
            << bisector.remaining() << " Syzkaller commit" << (bisector.syzkaller_versions.size() == 1 ? "" : "s") << " in ["
            << bisector.low_date_str() << ", " << bisector.high_date_str() << "].\n" << flush;

    while ((err = bisector.next_session(logfile, env, bug)) == 0)
    {
        result = bisector.test_current(logfile, env, bug);
        bisector.record(result);
        logfile << "About " << bisector.remaining() << " commits remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        goto finish;
    }

    // ======================================================================================================
    // Kernel Bisection

    // We will need to test the remainder of the kernel commits between the bisect date
    // (whichever commit was tested alongside), and the earliest kernel commit testable.

skip_syzkaller:
    err = bisector.next_phase(Bisect_Kernel);
    if (err < 0)
    {
        cerr << "Failed to advance to kernel bisection phase.\n" << flush;
        goto finish;
    }

    logfile << "\n==== Kernel Bisection ====\n"
            << bisector.remaining() << " Linux commit" << (bisector.remaining() == 1 ? "" : "s") << " in [" << bisector.low_date_str() << ", " << bisector.high_date_str() << "].\n"
            << "Syzkaller Version: " << bisector.this_session().syzkaller.date.get_date() << " - " << bisector.this_session().syzkaller.name << endl << flush;

    while ((err = bisector.next_session(logfile, env, bug)) == 0)
    {
        result = bisector.test_current(logfile, env, bug);
        bisector.record(result);
        logfile << "About " << bisector.stable_remaining() << " commits remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        goto finish;
    }

    // ======================================================================================================
    // Finish
    // ======================================================================================================

    bisector.next_phase(Bisect_Done);

    logfile << "\n" << SPACER
            << bisector.print_result(env, bug, starttime) << flush;

    logfile << "\nPossible Blocking Bugs:\n";
    for (string b : bug.blocking_bugs.list_blocking_bugs())
        logfile << "    " << b << endl;

finish:
    cout << SPACER
        << "Cleaning up...";

    // clean up reproducer and config
    if (!check_file(env.wd + "/old"))
        make_dir(env.wd + "/old");

    if (check_file(bug.kconfig))
        move(bug.kconfig, env.wd + "/old");
    
    if (check_file(bug.allreproducer))
        move(bug.allreproducer, env.wd + "/old");

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
    cout << "Help:\n"
        << "Short Ticks:\n"
        << "    -F [find_hash]: the hash of the finding commit.\n"
        << "    -G [guilty_hash]: the hash of the guilty commit.\n"
        << "    -f [find_date]: the date the bug was found on.\n"
        << "    -m [max_time]: the maximum time allowed when fuzzing.\n"
        << "    -i [id]: REQUIRED. The id of the inspector.\n"
        << "Long Ticks:\n"
        << "    --setup-only: download and build all the parts, but don't actually fuzz.\n"
        << "    --no-merge: don't use the merge commit as a revealing factor.\n"
        << "    --no-poc: fuzz without the poc.\n"
        << "    --stateful-corpus: keep the syzkaller corpus between runs\n"
        << "    --prune-corpus: keep only useful test cases in corpus between runs\n"
        << "    --find-only: only fuzz at the finding commit.\n"
        << "    --safe-mode: use safe mode.\n"
        << "    --known-syz: use a specific syzkaller hash.\n"
        << endl << flush;
}

void handle_bug_config(Environment &env, Bug_Info &bug)
{
    string filename = "wd-inspector-" + to_string(env.id) + "/" + "bug.cfg";
    bug.parse_config_file(filename);
    env.parse_config_file(bug, filename);
}

int main(int argc, char ** argv)
{
    Argparse args;
    Environment env;
    Bug_Info bug;

    args.expect("FGfmih");
    args.expect(vector<string>({ "setup-only", "help", "no-merge", "no-poc", "find-only", "safe-mode", "known-syz", "stateful-corpus", "prune-corpus" }));
    args.parse(argc, argv);
    if (args.is_set('h') || args.is_set("help"))
    {
        print_help();
        return 0;
    }

    // This clarifies some nonsense with commit times
    set_timezone("UTC0");

    env.fuzztimes = 3;

    // TODO: Pass a config file rather than this mess
    // TODO: Put this stuff in the bug config file, and make that file a json
    if (args.is_set('F'))
        bug.find_hash = args.get_arg_as_string('F');
    else
    {
        cout << "Error: No finding commit given. Please use -F [hash].\n";
        return -1;
    }

    if (args.is_set('G'))
        bug.guilty_hash = args.get_arg_as_string('G');
    else
    {
        cout << "Error: No guilty commit given. Please use -G [hash].\n";
        return -1;
    }

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

    if (args.is_set('f'))
    {
        bug.find_date = Date(args.get_arg_as_string('f'));
        bug.find_date.set_delim('-');
    }

    // get config for how to run
    cout << SPACER
         << "Parsing configs.\n";
    env.parse_parameters_file("parameters/config.cfg");

    // get information about the bug
    handle_bug_config(env, bug);

    if (bug.name.find("KMSAN") != string::npos)
        env.compiler_setting = COMPILER_CLANG_14;
    else
        env.compiler_setting = COMPILER_GCC;

    export_go(env);
    env.origin_path = get_path();

    // Corpus options
    env.use_poc = !args.is_set("no-poc");
    env.stateful_corpus = args.is_set("stateful-corpus") || args.is_set("prune-corpus");
    env.prune_corpus = args.is_set("prune-corpus");

    // allow for fuzzing only at the finding commit
    env.find_only = args.is_set("find-only");

    // allow for ignoring the merge as a revealing factor
    env.no_merge = args.is_set("no-merge");
    
    if(args.is_set("safe-mode") || bug.name.substr(0, 11) == "memory leak")
        set_safe_mode(env.safe_mode, env.max_time, env.fuzztimes);

    // make sure all of the needed files are here.
    cout << "Checking files.\n";
    if (!check_file(env.wd + "/log"))
    {
        cout << "Creating log directory.\n";
        make_dir(env.wd + "/log");
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

    if (!check_file(env.image_dir + "/stretch/stretch.img"))
    {
        cerr << "Error: No image file for stretch.\n";
        return -1;
    }
    else
        cout << "Found stretch image.\n";

    if (!check_file(env.image_dir + "/wheezy/wheezy.img"))
    {
        cerr << "Error: No image file for wheezy.\n";
        return -1;
    }
    else
        cout << "Found wheezy image.\n";

    return bisect(args, env, bug);
}
