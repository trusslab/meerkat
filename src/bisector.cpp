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
            cerr << "Error: Syzkaller binary " << env.syzdir + "bin/" + bin << " was not found.\n" << flush;
            return -1;
        }
    }
    return 0;
}

vector<string> order_pocs(const Environment &env)
{
    vector<string> ret;
    // PoCs can go in an arbitrary order, but leave out the primary poc
    for (string file : list_dir(env.reprodir))
        if (file != env.primary_repro)
            ret.push_back(file);

    return ret;
}

// thought I would have needed this, but I guess not.
int make_repro_opts(const Environment &env)
{
    vector<string> inlines, outlines;
    load_file(env.primary_repro, inlines);
    for (string l : inlines)
    {
        if (starts_with(l, "#{"))
        {
            outlines.push_back(l.substr(1));
            break;
        }
    }

    write_file(env.repro_opts_file(), outlines);
    return 0;
}

int do_bisection(Environment &env, Bisect &bisector, Git &linux_git)
{
    int err = 0;
    Test_Result result;

    // ======================================================================================================
    // Fuzz at the anchor commit

    if (bisector.next_phase(Bisect_Anchor, env, linux_git) < 0)
    {
        cerr << "Failed to advance to anchor commit phase.\n" << flush;
        return -1;
    }

    cout << "\n==== Anchor Commit ====\n" << flush;

    // Begin with Syzkaller from finding date. This emulates Syzbot for any algorithm
    if (bisector.next_session(env, linux_git) < 0)
    {
        cerr << "Error: Failed to go to anchor commit\n" << flush;
        return -1;
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
        if (bisector.mode() == Mode_FF)
            cout << "Average TTF: " << result.suggest_ttf << endl;
        cout << "Find-only complete.\n";
        return 1;
    }

    if (!result.found)
    {
        cout << "\nFailure: This bug cannot be found at the anchor commit.\n" << flush;
        return 1;
    }
    else if (bisector.mode() == Mode_FF)
    {
        cout << "\nNew Max Time: " << env.max_time << "\n" << flush;
    }

    bisector.record(result, linux_git);

    // ======================================================================================================
    // Major Release Search

    err = bisector.next_phase(Bisect_Releases, env, linux_git);
    if (err < 0)
    {
        cerr << "Failed to advance to Release search phase.\n" << flush;
        return -1;
    }

    cout << "\n==== Major Release Search ====\n" 
         << bisector.remaining(linux_git) << " Release" << (bisector.remaining(linux_git) == 1 ? "" : "s") << "\n" << flush;

    while ((err = bisector.next_session(env, linux_git)) == 0)
    {
        result = bisector.test_current(env, linux_git);
        bisector.record(result, linux_git);
        cout << "About " << bisector.remaining(linux_git) << " releases remaining\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    // Result: a bisect session and index into the releases.
    // Bisect session is the oldest release where the bug reproduced. Index points to it in the releases array.

    // ======================================================================================================
    // Kernel Bisection

    err = bisector.next_phase(Bisect_Kernel, env, linux_git);
    if (err < 0)
    {
        cerr << "Failed to advance to kernel bisection phase.\n" << flush;
        return -1;
    }

    cout << "\n==== Kernel Bisection ====\n"
         << bisector.remaining(linux_git) << " Linux commit" << (bisector.remaining(linux_git) == 1 ? "" : "s") << endl << flush;

    while ((err = bisector.next_session(env, linux_git)) == 0)
    {
        result = bisector.test_current(env, linux_git);
        err = bisector.record(result, linux_git);
        cout << "About " << bisector.remaining(linux_git) << " commits remaining\n" << flush;
        if (err == -3)
            cout << "Git bisect reported multiple guilty commits\n" << flush;
    }
    if (err < 0)
    {
        cerr << "Failed to get or build next session.\n" << flush;
        return -1;
    }

    // set bisector.good_version to bisector.bisect_version.first_parent()
    bisector.set_good_version(linux_git);

    return 0;
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

    env.duplicates.clear();
    env.duplicates.push_back(env.name);

    env.port.init(START_PORT, env.id*env.vmst.numVM, PORT_RANGE);

    bisector.init(env, linux_git);

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
        env.required_syscalls = get_reproduer_syscall_descriptions(env);

    env.print();

    // ======================================================================================================
    // Break off here by mode
    
    if (bisector.mode() == Mode_FF)
    {
        stagetime = chrono::steady_clock::now();
        stage_title = "Focused Fuzzing";

        err = do_bisection(env, bisector, linux_git);
        if (err < 0 || env.feats.setup_only || env.feats.find_only)
            goto finish;
        if (err == 1 && env.feats.ff_no_find_backup)
            goto finish;
        
        if (err == 0)
            found = true;
        if (env.feats.poc_test)
            cout << bisector.print_partial_result(env, linux_git, starttime, stagetime, stage_title, env.primary_repro) << flush;
    }

    if (env.feats.poc_test)
        bisector.set_mode(Mode_PoC);

    if (bisector.mode() == Mode_PoC)
    {
        if (env.primary_repro.empty())
            env.primary_repro = list_dir(env.reprodir).front();

        vector<string> ordered_pocs = order_pocs(env);
        int numPoCs = ordered_pocs.size() + 1;
        stage_title = "Primary PoC Test";

        // Use more vms at 2 cpus during poc bisection. Simlar to SB.
        env.vmc = env.vmst;

redo_poc:
        stagetime = chrono::steady_clock::now();
        err = do_bisection(env, bisector, linux_git);
        if (err < 0 || env.feats.setup_only || env.feats.find_only)
            goto finish;

        if (env.feats.poc_all_pocs)
        {
            if (numPoCs <= 1)
            {
                if (err != 1)
                    goto print_result;
                
                cout << bisector.print_anchor_fail(env, starttime, stagetime, stage_title, env.primary_repro) << flush;
                goto finish;
            }

            if (err == 1)
                cout << bisector.print_anchor_fail(env, starttime, stagetime, stage_title, env.primary_repro) << flush;
            else
            {
                cout << bisector.print_partial_result(env, linux_git, starttime, stagetime, stage_title, env.primary_repro) << flush;
                found = true;
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
        << "         [ all, default, poc-test, ff-test, setup-only, find-only, poc-all-pocs, ff-no-find-backup, stateful-corpus, patch-kernel ]"
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

    // get information about the bug
    if (handle_config(env, args) < 0)
        return -1;

    if (handle_features(args, env) < 0)
        return -1;

    // TODO: set compiler mode here if needed

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
