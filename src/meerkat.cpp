#include <argparse.h>
#include <bisect.h>
#include <consts.h>
#include <date.h>
#include <environment.h>
#include <file_api.h>
#include <linux.h>
#include <fuzz.h>
#include <git.h>
#include <my_string.h>
#include <dedup.h>
#include <shell_api.h>
#include <syzkaller.h>
#include <template_parse.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>

using namespace std;

// TODO:
// Check for broken programs when Syzkaller launches.
// automatically verify that the compilers work
// check that procs # is not affecting hanging tasks
// Use older Syzkaller?
// Use this:
    /*
    SYZ_DISABLE_SANDBOXING=yes
	KBUILD_BUILD_VERSION=0
	KBUILD_BUILD_TIMESTAMP=now
	KBUILD_BUILD_USER=syzkaller
	KBUILD_BUILD_HOST=syzkaller
	KERNELVERSION=syzkaller
	LOCALVERSION=-syzkaller
    */

// check for a patch of v4.19
// check remaining disabled syscall cases
// Fix bug where bisection continues with more PoCs after reaching OTR

vector<string> order_pocs(const Environment &env)
{
    vector<string> ret;
    // PoCs can go in an arbitrary order, but leave out the primary poc.
    // This is for after the primary poc has been tested.
    for (string file : list_dir(env.reprodir))
        if (file != env.primary_repro)
            ret.push_back(file);

    return ret;
}

Bisect_Return do_bisection(Environment &env, Bisect &bisector, Git &linux_git)
{
    Bisect_Return err = BIS_NORMAL;
    Test_Result result;

    // ======================================================================================================
    // Fuzz at the anchor commit

    if (bisector.next_phase(Bisect_Anchor, env, linux_git) == BIS_ERR)
    {
        cerr << "Failed to advance to anchor commit phase.\n" << flush;
        return BIS_ERR;
    }

    cout << "\n==== Anchor Commit ====\n" << flush;

    // Begin with Syzkaller from finding date. This emulates Syzbot for any algorithm
    if (bisector.next_session(env, linux_git) == BIS_ERR)
    {
        cerr << "Error: Failed to go to anchor commit\n" << flush;
        return BIS_ERR;
    }

    if (env.feats.setup_only)
    {
        write_syzkaller_config(env);
        cout << "Setup-only complete.\n" << flush;
        return BIS_STOP;
    }

    result = bisector.test_current(env, linux_git);

    if (env.feats.find_only)
    {
        if (bisector.mode() == Mode_FF)
            cout << "Average TTF: " << result.suggest_ttf << endl;
        cout << "Find-only complete.\n";
        return BIS_STOP;
    }

    if (!result.found)
    {
        cout << "\nFailure: This bug cannot be found at the anchor commit.\n" << flush;
        return BIS_ANCHOR;
    }
    else if (bisector.mode() == Mode_FF)
    {
        cout << "\nNew Max Time: " << env.max_time << "\n" << flush;
    }

    bisector.record(result, linux_git);

    // ======================================================================================================
    // Major Release Search

    if ((err = bisector.next_phase(Bisect_Releases, env, linux_git)) == BIS_ERR)
    {
        cerr << "Error: Failed to advance to Release search phase.\n" << flush;
        return BIS_ERR;
    }

    cout << "\n==== Major Release Search ====\n" 
         << bisector.remaining(linux_git) << " Release" << (bisector.remaining(linux_git) == 1 ? "" : "s") << "\n" << flush;

    while ((err = bisector.next_session(env, linux_git)) == BIS_NORMAL)
    {
        result = bisector.test_current(env, linux_git);
        bisector.record(result, linux_git);
        cout << "About " << bisector.remaining(linux_git) << " releases remaining\n" << flush;
    }
    if (err == BIS_ERR)
    {
        cerr << "Error: Failed to get or build next session.\n" << flush;
        return BIS_ERR;
    }
    else if (err == BIS_OTR)
    {
        cout << "This bug was found on the oldest tested release.\n" << flush;
        return BIS_OTR;
    }

    // Result: a bisect session and index into the releases.
    // Bisect session is the oldest release where the bug reproduced. Index points to it in the releases array.

    // ======================================================================================================
    // Kernel Bisection

    if ((err = bisector.next_phase(Bisect_Kernel, env, linux_git)) == BIS_ERR)
    {
        cerr << "Failed to advance to kernel bisection phase.\n" << flush;
        return BIS_ERR;
    }

    cout << "\n==== Kernel Bisection ====\n"
         << bisector.remaining(linux_git) << " Linux commit" << (bisector.remaining(linux_git) == 1 ? "" : "s") << endl << flush;

    while ((err = bisector.next_session(env, linux_git)) == BIS_NORMAL)
    {
        result = bisector.test_current(env, linux_git);
        err = bisector.record(result, linux_git);
        cout << "About " << bisector.remaining(linux_git) << " commits remaining\n" << flush;
        switch (err) {
        case BIS_ERR:
            cerr << "Error: Git failure while recording session result.\n" << flush;
            return BIS_ERR;
        case BIS_COMPLETE:
        case BIS_MULT:
            return err;
        default:
            break;
        }
    }
    if (err == BIS_ERR)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return BIS_ERR;
    }

    // set bisector.good_version to bisector.bisect_version.first_parent()
    bisector.set_good_version(linux_git);

    return err;
}

int bisect(Environment &env)
{
    bool found = false;
    int err = 0;
    string stage_title;
    Bisect bisector;
    Test_Result result;
    chrono::steady_clock::time_point starttime, stagetime;

    Git linux_git = prep_kernel_local_repo(env);
    if (linux_git.error() < 0)
        goto finish;

    starttime = chrono::steady_clock::now();
    cout << left << setw(CONFW) << "Bisecting:" << env.name << endl;

    env.port.init(START_PORT, env.id*env.vmst.numVM, PORT_RANGE);

    if (bisector.init(env, linux_git) < 0)
    {
        err = -1;
        goto finish;
    }

    if (check_syzkaller(env) < 0)
        goto finish;

    if (uniqify_reproducers(env) < 0)
        goto finish;

    determine_threadedness(env);

    if (env.feats.ff_test)
        bisector.set_mode(Mode_FF);
    else if (env.feats.poc_test)
        bisector.set_mode(Mode_PoC);
    else
    {
        cerr << "Error: Invalid bisection mode state.\n" << flush;
        goto finish;
    }

    if (bisector.mode() == Mode_FF)
        env.required_syscalls = get_reproducer_syscall_descriptions(env);

    env.print();

    // ======================================================================================================
    // Break off here by mode
    
    if (bisector.mode() == Mode_FF)
    {
        stagetime = chrono::steady_clock::now();
        stage_title = "Focused Fuzzing";

        err = do_bisection(env, bisector, linux_git);
        switch (err) {
        case BIS_ERR:
        case BIS_STOP:
            goto finish;
            break;

        case BIS_ANCHOR:
            cout << bisector.print_anchor_fail(env, starttime, stagetime, stage_title) << flush;
            if (env.feats.ff_no_find_backup || !env.feats.poc_test)
                goto finish;
            break;

        case BIS_OTR:
            goto print_result;
            break;

        case BIS_MULT:
            cout << "Git bisect reported multiple guilty commits\n" << flush;
        case BIS_NORMAL:
        case BIS_COMPLETE:
            found = true;
            if (env.feats.poc_test)
                cout << bisector.print_partial_result(env, linux_git, starttime, stagetime, stage_title, env.primary_repro) << flush;
            goto print_result;
            break;

        default:
            cerr << "Unhandled return value " << err << " at end of mutation phase\n" << flush;
            break;
        }
    }

    if (env.feats.poc_test)
        bisector.set_mode(Mode_PoC);

    if (bisector.mode() == Mode_PoC)
    {
        // Make a Primary PoC
        if (env.primary_repro.empty())
            env.primary_repro = list_dir(env.reprodir).front();

        // Get the rest of the PoCs
        vector<string> ordered_pocs = order_pocs(env);
        int numPoCs = ordered_pocs.size() + 1;
        stage_title = "Primary PoC Test";

        // Use more vms at 2 cpus during poc bisection. Simlar to SB.
        env.vmc = env.vmst;

redo_poc:
        stagetime = chrono::steady_clock::now();
        err = do_bisection(env, bisector, linux_git);
        if (err == BIS_ERR || err == BIS_STOP)
            goto finish;

        // We just finished a bisection run. Do we consider other PoCs?
        if (env.feats.poc_all_pocs)
        {
            switch (err) {
            case BIS_OTR: // Does the bug reproduce on OTR?
                goto print_result;

            case BIS_ANCHOR:
                cout << bisector.print_anchor_fail(env, starttime, stagetime, stage_title, env.primary_repro) << flush;

                // If there was only one PoC to begin with and this was the only test done, don't print the final result.
                if (numPoCs <= 1 && !env.feats.ff_test)
                    goto finish;
                break;

            case BIS_MULT:
                cout << "Git bisect reported multiple guilty commits\n" << flush;
            case BIS_COMPLETE:
                cout << bisector.print_partial_result(env, linux_git, starttime, stagetime, stage_title, env.primary_repro) << flush;
                found = true;
                break;

            default:
                cerr << "Unhandled return value " << err << " at end of PoC phase\n" << flush;
                break;
            }

            if (ordered_pocs.empty() && found)
                goto print_result;
            else if (ordered_pocs.empty())
                goto finish;

            env.primary_repro = ordered_pocs.back();
            ordered_pocs.pop_back();
            stage_title = "Subsequent PoC Test";
            goto redo_poc;
        }
    }

    // ======================================================================================================
    // Finish
    // ======================================================================================================

print_result:
    bisector.next_phase(Bisect_Done, env, linux_git);

    cout << "\n" << SPACER
         << bisector.print_result(env, linux_git, starttime) << flush;

finish:
    return err;
}

void print_help()
{
    cout << "Usage: ./bin/" << PROJECT_NAME << " -c [config] -i [id]\n"
        << "    -m [max time]: the maximum time (minutes) allowed when fuzzing (default " << DEFAULT_MAX_TIME << ").\n"
        << "    -i [id]: REQUIRED. The id of the bisector (i.e. 1).\n"
        << "    --config (c) [config.cfg]: [config]: REQUIRED. The config file containing the bug information.\n"
        << "    --anchor (a) [hash]: REQUIRED. the hash of the commit where the bug was found.\n"
        << "    --feature (F) [feature list]: features to use.\n"
        << "         [ default, poc-test, ff-test, setup-only, find-only, poc-all-pocs, ff-no-find-backup, stateful-corpus, no-patch-kernel, obselete-patches, old-syzkaller ]\n"
        << "    --debug: print certain debug information during bisection.\n"
        << "    --version: print the version number and exit.\n"
        << flush;
}

void print_version()
{
    cout << PROJECT_NAME << " " << REVISION << endl << flush;
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
    args.expect(vector<string>({ "help", "config", "feature", "anchor", "debug", "version" }));
    args.parse(argc, argv);
    if (args.is_set('h') || args.is_set("help"))
    {
        print_help();
        return 0;
    }

    if (args.is_set("version"))
    {
        print_version();
        return 0;
    }

    env.init();

    if (args.is_set('m'))
        env.max_time = args.get_int('m');
    else
        env.max_time = DEFAULT_MAX_TIME;

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

    if (args.is_set("debug"))
        env.debug = true;

    // get information about the bug
    if (handle_config(env, args) < 0)
        return -1;

    if (handle_features(args, env) < 0)
        return -1;

    if (env.parse_aliases() < 0)
        return -1;

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

    if (!check_file(env.image))
    {
        cerr << "Error: No image file found.\n";
        return -1;
    }

    if (!check_file(env.image_key))
    {
        cerr << "Error: No image key file found.\n";
        return -1;
    }

    cout << args.origin() << endl;
    return bisect(env);
}
