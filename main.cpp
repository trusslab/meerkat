#include <date.h>
#include <argparse.h>
#include <bug_info.h>
#include <inspector_config.h>
#include <file_api.h>
#include <shell_api.h>
#include <psf.h>
#include <fuzz_prep.h>
#include <fuzz.h>
#include <consts.h>
#include <template_parse.h>
#include <git_api.h>
#include <session.h>
#include <git_traverse.h>
#include <retrospect.h>
#include <result.h>
#include <blocking_bugs.h>
#include <environment.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>

using namespace std;

int prep_syzkaller_local_repo(const Bug_Info &bug)
{
    if (!check_file(bug.syzdir))
    {
        cout << "Creating Syzkaller directory.\n";
        make_dir(bug.syzdir);
    }
    else
        cout << "Found Syzkaller directory.\n";

    if (!check_file(bug.syzdir + "/.git"))
    {
        cout << "Cloning Syzkaller repository...\n";
        if (git_clone(SYZKALLER_REPO_REMOTE, bug.syzdir) < 0)
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

    if (!check_file(bug.kerneldir))
    {
        cout << "Creating kernel directory.\n";
        make_dir(bug.kerneldir);
    }
    else
        cout << "Found kernel directory.\n";

    // this may cause issues when the repository opened is not the one we want
    if (!check_file(bug.kerneldir + "/.git"))
    {
        cout << "Cloning Linux repository...\n";
        if (git_clone(env.linux_repo_remote, bug.kerneldir) < 0)
        {
            cerr << "Error: Git clone failed.\n";
            return -1;
        }
    }
    else
        cout << "Found Linux local repository.\n";
    
    return 0;
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
        << "    --recover: enter recovery mode.\n"
        << "    --no-merge: don't use the merge commit as a revealing factor.\n"
        << "    --no-poc: fuzz without the poc.\n"
        << "    --find-only: only fuzz at the finding commit.\n"
        << "    --safe-mode: use safe mode.\n"
        << endl;
}

int bisect(Argparse &args, Environment &env, InspectorConfig &inspector, Bug_Info &bug)
{
    int err = 0, git_err = 0;
    ofstream logfile;
    Bisect bisector;
    bisector.session_count = 0; // bisector.init()
    
    
    
    
    int k;
    string find_hash, guilty_hash,
           compiler,
           revealing_factor = "",
           reveal_name = "";


    Version linux_version, syzkaller_version, prev_syzkaller_version,
            current_version, bisect_version, merge_commit,
            guilty_version, finding_version, reveal_version;
    Session this_session;
    Test_Result result, result_before, result_after;


    Blocking_Bugs blocking_bugs;
    vector<Version> gcc_versions, clang_versions,
                    kernel_versions, syzkaller_versions;
    vector<Session> fuzz_sessions;





    env.logfilename = bug.wd + "/log/bug" + to_string(bug.number) + ".log";
    logfile.open(env.logfilename);
    if(!logfile)
    {
        cerr << "Error: Failed to open log file " << env.logfilename << ".\n";
        return -1;
    }

    // Set up syzkaller repository locally
    err = prep_syzkaller_local_repo(bug);
    if (err < 0)
        goto finish;

    // Set up linux repository locally
    err = prep_kernel_local_repo(env, bug);
    if (err < 0)
        goto finish;

    // begin logging
    logfile << bug.name << "," << bug.buglink << endl
            << "Repository: " << bug.kpreface << endl
            << "Arch: " << bug.arch << endl
            << "Finding: " << git_get_commit_date(bug.wd, bug.kerneldir, find_hash).get_date() << " - " << find_hash << endl
            << "Guilty:  " << git_get_commit_date(bug.wd, bug.kerneldir, guilty_hash).get_date() << " - " << guilty_hash << endl << flush;

    // Parse Syzbot for duplicate bugs
    cout << SPACER
        << "Gathering bug fixes from Syzbot.\n";

    gather_duplicates(bug, inspector);

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
    cout << SPACER;

    inspector.port.init(START_PORT, env.id, 5);

    determine_threadedness(inspector, bug, logfile);

    logfile << "Max time:" << env.max_time << endl 
            << "Max attempts:" << env.fuzztimes << endl << flush;
    
    // ======================================================================================================
    // Begin Inspection
    // ======================================================================================================

    bisector.session_count = 0;

    // commits are arranged newest (low index) to oldest (high index)
    cout << SPACER
         << "Gathering kernel versions...\n";
    kernel_versions = get_kernel_versions(bug, guilty_hash, find_hash);
    cout << "Found " << kernel_versions.size() << " kernel commits.\n";
    if (kernel_versions.size() == 0)
    {
        logfile << "Error: Failed to gather kernel versions.\n" << flush;
        goto finish;
    }

    if (bug.name.find("KMSAN") != string::npos)
        inspector.compiler_setting = COMPILER_CLANG_14;
    else
        inspector.compiler_setting = COMPILER_GCC;

    finding_version.name = find_hash;
    guilty_version.name = guilty_hash;
    finding_version.date = kernel_versions.front().date;
    if (args.is_set('f'))
    {
        bisector.find_date.set_delim('-');
        bisector.high_date = bisector.find_date;
    }
    else
        bisector.high_date = bisector.find_date = finding_version.date;

    bisector.low_date = guilty_version.date = kernel_versions.back().date;
    if (bisector.low_date < SYZBOT_BEGIN_DATE)
        bisector.low_date = SYZBOT_BEGIN_DATE;

    // commits are arranged newest (low index) to oldest (high index)
    cout << SPACER
         << "Gathering Syzkaller versions...\n";
    syzkaller_versions = get_syzkaller_versions(bug);
    cout << "Found " << syzkaller_versions.size() << " Syzkaller commits.\n";

    // ======================================================================================================
    // Fuzz at the finding commit
    
    cout << SPACER
         << "Testing the finding commit.\n";
    logfile << "Testing the finding commit.\n" << flush;

    k = get_index_by_name(kernel_versions, find_hash);
    if (k < 0)
    {
        cerr << "This kernel version does not exist.\n";
        logfile << "Error: Could not find kernel version " << find_hash << ".\n" << flush;
        goto finish;
    }
    linux_version = kernel_versions.at(k);
    syzkaller_version = get_version_by_date(syzkaller_versions, bisector.high_date);

    this_session = Session(linux_version, syzkaller_version, false);
    log_session_info(logfile, this_session, bisector.inc_session());

    cout << "Making the kernel.\n";
    compiler = get_compiler(gcc_versions, clang_versions, linux_version.date, inspector);
    log_session_compiler(logfile, compiler);
    err = prep_kernel(bug, inspector, linux_version, env.linux_repo_remote, compiler);
    clean_path(env.origin_path);
    if (err < 0)
    {
        log_kernel_build_error(logfile);
        goto finish;
    }

    cout << SPACER
         << "Prepping Syzkaller\n";
    err = prep_syzkaller(bug, inspector, syzkaller_version);
    if (err < 0)
    {
        log_syzkaller_build_error(logfile);
        goto finish;
    }

    cout << SPACER;
    if (args.is_set("setup-only"))
    {
        write_syzkaller_config(bug, inspector, syzkaller_version.date);
        reset_kaller_wd(bug.syzwd);
        logfile << "Setup-only complete.\n" << flush;
        cout << "Setup complete.\n";
        goto setup_only_finish;
    }

    result = fuzz_loop_finding(logfile, bug, inspector, bug.duplicates, env.max_time, env.fuzztimes, syzkaller_version.date, env.use_poc, env.find_only);
    env.max_time = (env.safe_mode ? env.max_time : result.suggest_ttf);
    log_session_result(logfile, result, bug.duplicates);
    blocking_bugs.count_blocking_bugs(result);

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
        logfile << "\nNew Max Time: " << env.max_time << ".\n" << flush;
    }

    this_session.found = result.found;
    this_session.stable = result.stable;
    kernel_versions.at(k).skipped = !result.stable && !result.found;
    fuzz_sessions.push_back(this_session);
    
    //  ======================================================================================================
    // Find Merge Commit

    if (!env.no_merge)
    {
        cout << SPACER
             << "Looking for Merge Commit...\n";

        merge_commit = git_find_merge_commit(bug.kerneldir, kernel_versions, guilty_hash);

        if (!merge_commit.name.empty())
        {
            cout << "Merge commit found: " << merge_commit.name << ".\n";
            logfile << "Merge Commit: " << merge_commit.date.get_date() << " - " << merge_commit.name << "\n" << flush;
            bisector.low_date = merge_commit.date > SYZBOT_BEGIN_DATE ? merge_commit.date : SYZBOT_BEGIN_DATE;
            // cut the kernel_versions here. Find the merge commit, then erase everything after it.
            kernel_versions.erase(kernel_versions.begin() + get_index_by_name(kernel_versions, merge_commit.name) + 1, kernel_versions.end());
        }
        else
        {
            cout << "No merge commit found.\n";
            logfile << "No Merge Commit.\n" << flush;
        }
    }

    // ======================================================================================================
    // Syzkaller Bisection

    logfile << "\n==== Syzkaller Bisection ====\n" 
            << syzkaller_versions.size() << " Syzkaller commit" << (syzkaller_versions.size() == 1 ? "" : "s") << " in ["
            << bisector.low_date.get_date() << ", " << bisector.high_date.get_date() << "].\n" << flush;

    bisect_version = syzkaller_versions.back();

    // right is the older date (lower date). higher index
    // left is the recent date (higher date). lower index
    bisector.right = syzkaller_versions.size() - 2;
    bisector.left = 0;

    while (bisector.left <= bisector.right)
    {
        // TODO: handle in bisector
        bisector.middle = (bisector.left + bisector.right) / 2;
        current_version = syzkaller_versions.at(bisector.middle);
        linux_version = get_version_by_date(kernel_versions, current_version.date);
        this_session = Session(linux_version, current_version, false);
        log_session_info(logfile, this_session, bisector.inc_session());
    
        if (!already_fuzzed(fuzz_sessions, this_session))
        {
            cout << SPACER
                << "Making the kernel\n";
            compiler = get_compiler(gcc_versions, clang_versions, linux_version.date, inspector);
            log_session_compiler(logfile, compiler);
            err = prep_kernel(bug, inspector, linux_version, env.linux_repo_remote, compiler);
            clean_path(env.origin_path);
            if (err < 0)
            {
                log_kernel_build_error(logfile);
                goto finish;
            }

            cout << SPACER
                << "Making Syzkaller\n";
            err = prep_syzkaller(bug, inspector, current_version);
            if (err < 0)
            {
                log_syzkaller_build_error(logfile);
                goto finish;
            }

            cout << SPACER;
            result_after = fuzz_loop(logfile, bug, inspector, bug.duplicates, env.max_time, env.fuzztimes, current_version.date, env.use_poc);
            log_session_result(logfile, result_after, bug.duplicates);
            blocking_bugs.count_blocking_bugs(result_after);
            if (check_safe_mode(result_after, env.safe_mode, env.max_time, env.fuzztimes))
                log_safe_mode(logfile, env.max_time, env.fuzztimes);

            this_session.found = result_after.found;
            this_session.stable = result_after.stable;
            kernel_versions.at(k).skipped = !result_after.stable && !result_after.found;
            fuzz_sessions.push_back(this_session);
        }
        else
        {
            cout << "This session has already been fuzzed. Skipping.\n";
            result_after.found = session_get_result(fuzz_sessions, this_session) == 1 ? true : false;
            result_after.stable = session_get_stable(fuzz_sessions, this_session) == 1 ? true : false;
            logfile << "The bug was " << (result_after.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
        }

        if (result_after.found)
        {
            bisector.left = bisector.middle + 1;
            bisect_version = current_version;
            bisector.high_date = linux_version.date;
        }
        else
        {
            bisector.right = bisector.middle - 1;
            bisector.low_date = linux_version.date;
        }
    }

    // check before the bisected version here
    k = get_index_by_name(syzkaller_versions, bisect_version.name);
    if (k < syzkaller_versions.size() - 1)
    {
        linux_version = get_version_by_date(kernel_versions, bisect_version.date);
        // previous (older) means incrementing the index
        prev_syzkaller_version = syzkaller_versions.at(k + 1);

        this_session = Session(linux_version, prev_syzkaller_version, false);
        log_session_info(logfile, this_session, bisector.inc_session());

        if (!already_fuzzed(fuzz_sessions, this_session))
        {
            // only build the kernel if we have to
            if (bisect_version != current_version)
            {
                cout << SPACER
                    << "Making the kernel\n";
                compiler = get_compiler(gcc_versions, clang_versions, linux_version.date, inspector);
                log_session_compiler(logfile, compiler);
                err = prep_kernel(bug, inspector, linux_version, env.linux_repo_remote, compiler);
                clean_path(env.origin_path);
                if (err < 0)
                {
                    log_kernel_build_error(logfile);
                    goto finish;
                }
            }
            
            // pull the previous template
            cout << SPACER
                << "Prepping the old template\n";
            err = prep_syzkaller(bug, inspector, prev_syzkaller_version);
            if (err < 0)
            {
                log_syzkaller_build_error(logfile);
                goto finish;
            }

            // now compile the current syzkaller using the old template
            cout << SPACER
                << "Making Syzkaller\n";
            err = prep_syzkaller(bug, inspector, bisect_version);
            if (err < 0)
            {
                log_syzkaller_build_error(logfile);
                goto finish;
            }

            cout << SPACER;
            result_before = fuzz_loop(logfile, bug, inspector, bug.duplicates, env.max_time, env.fuzztimes, bisect_version.date, env.use_poc);
            log_session_result(logfile, result_before, bug.duplicates);
            blocking_bugs.count_blocking_bugs(result_before);
            if (check_safe_mode(result_before, env.safe_mode, env.max_time, env.fuzztimes))
                log_safe_mode(logfile, env.max_time, env.fuzztimes);

            this_session.found = result_before.found;
            this_session.stable = result_before.stable;
            kernel_versions.at(k).skipped = !result_before.stable && !result_before.found;
            fuzz_sessions.push_back(this_session);
        }
        else
        {
            cout << "This session has already been fuzzed. Skipping.\n";
            result_before.found = session_get_result(fuzz_sessions, this_session) == 1 ? true : false;
            result_before.stable = session_get_stable(fuzz_sessions, this_session) == 1 ? true : false;
            logfile << "The bug was " << (result_before.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
        }

        // if we get to here, the bug must have been found on the bisect version
        if (!result_before.found) // && result_after.found
        {
            revealing_factor = "Template Update";
            reveal_version = Version(bisect_version.name, bisect_version.date);
            reveal_name = get_commit_name(bug.syzdir, bisect_version.name);

            cout << SPACER
                    << "Revealing Factor Found!\n"
                    << "Template update " << reveal_version.name << " on " << reveal_version.date.get_date() << " is the reason.\n";
        }
    }
    
    syzkaller_version = Version(bisect_version.name, bisect_version.date);

    // ======================================================================================================
    // Kernel Bisection

    // r is the starting date. older date (lower date). higher index
    // l is the ending date. recent date (higher date). lower index
    bisector.right = get_starting_index(kernel_versions, bisector.low_date);
    bisector.left = get_ending_index(kernel_versions, bisector.high_date);
    bisect_version = kernel_versions.at(bisector.left);

    logfile << "\n==== Kernel Bisection ====\n"
            << bisector.right - bisector.left << " Linux commit" << (bisector.right - bisector.left == 1 ? "" : "s") << " in [" << bisector.low_date.get_date() << ", " << bisector.high_date.get_date() << "].\n" << flush;

    while (bisector.left <= bisector.right)
    {
        // TODO: handle in bisector
        // find the next stable version to test. If there are none, end the search and report a date range.
        bisector.middle = get_next_commit_binary(bisector.right, bisector.left, kernel_versions);
        if (bisector.middle < 0)
        {
            cout << "There are no more stable commits to test.\n";
            revealing_factor = "Unstable Commits";
            reveal_version = kernel_versions.at(bisector.left);
            reveal_name = get_commit_name(bug.kerneldir, kernel_versions.at(bisector.left).name);
            goto report;
        }
        linux_version = kernel_versions.at(bisector.middle);
        this_session = Session(linux_version, syzkaller_version, false);
        log_session_info(logfile, this_session, bisector.inc_session());

        if (!already_fuzzed(fuzz_sessions, this_session))
        {
            cout << SPACER
                 << "Making the kernel\n";
            compiler = get_compiler(gcc_versions, clang_versions, linux_version.date, inspector);
            log_session_compiler(logfile, compiler);
            err = prep_kernel(bug, inspector, linux_version, env.linux_repo_remote, compiler);
            clean_path(env.origin_path);
            if (err < 0)
            {
                log_kernel_build_error(logfile);
                goto finish;
            }

            cout << SPACER
                 << "Prepping Syzkaller\n";
            err = prep_syzkaller(bug, inspector, syzkaller_version);
            if (err < 0)
            {
                log_syzkaller_build_error(logfile);
                goto finish;
            }

            cout << SPACER;
            result = fuzz_loop(logfile, bug, inspector, bug.duplicates, env.max_time, env.fuzztimes, syzkaller_version.date, env.use_poc);
            log_session_result(logfile, result, bug.duplicates);
            blocking_bugs.count_blocking_bugs(result);
            if (check_safe_mode(result, env.safe_mode, env.max_time, env.fuzztimes))
                log_safe_mode(logfile, env.max_time, env.fuzztimes);

            this_session.found = result.found;
            this_session.stable = result.stable;
            kernel_versions.at(bisector.middle).skipped = !result.stable && !result.found;
            fuzz_sessions.push_back(this_session);
        }
        else
        {
            cout << "This session has already been fuzzed. Skipping.\n";
            result.found = session_get_result(fuzz_sessions, this_session) == 1 ? true : false;
            result.stable = session_get_stable(fuzz_sessions, this_session) == 1 ? true : false;
            logfile << "The bug was " << (result.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
        }

        if (result.found)
        {
            bisector.left = bisector.middle + 1;
            bisect_version = linux_version;
        }
        else if (!result.stable) // It looks like this caused an infinite loop somewhere. (mixed with choosing a commit to skip to)
            continue;
        else
            bisector.right = bisector.middle - 1;
    }

    // fuzz before and after the linux version to confirm
    cout << SPACER
         << "Checking if the kernel is the revealing factor.\n";
    logfile << "\n== Confirming the bisected commit ==\n" << flush;
    syzkaller_version = get_version_by_date(syzkaller_versions, bisect_version.date);
    this_session = Session(bisect_version, syzkaller_version, false);
    log_session_info(logfile, this_session, bisector.inc_session());

    if (!already_fuzzed(fuzz_sessions, this_session))
    {
        // fuzz after first
        // check if we need to fetch anything
        if (bisect_version != linux_version)
        {
            cout << SPACER
                << "Making the kernel\n";
            compiler = get_compiler(gcc_versions, clang_versions, bisect_version.date, inspector);
            log_session_compiler(logfile, compiler);
            err = prep_kernel(bug, inspector, bisect_version, env.linux_repo_remote, compiler);
            clean_path(env.origin_path);
            if (err < 0)
            {
                log_kernel_build_error(logfile);
                goto finish;
            }

            cout << SPACER
                << "Prepping Syzkaller\n";
            err = prep_syzkaller(bug, inspector, syzkaller_version);
            if (err < 0)
            {
                log_syzkaller_build_error(logfile);
                goto finish;
            }
        }

        cout << SPACER;
        result_after = fuzz_loop(logfile, bug, inspector, bug.duplicates, env.max_time, env.fuzztimes, syzkaller_version.date, env.use_poc);
        log_session_result(logfile, result_after, bug.duplicates);
        blocking_bugs.count_blocking_bugs(result_after);
        if (check_safe_mode(result_after, env.safe_mode, env.max_time, env.fuzztimes))
            log_safe_mode(logfile, env.max_time, env.fuzztimes);

        this_session.stable = result_after.stable;
        this_session.found = result_after.found;
        kernel_versions.at(get_index_by_name(kernel_versions, bisect_version.name)).skipped = !result_after.stable && !result_after.found;
        fuzz_sessions.push_back(this_session);
    }
    else
    {
        cout << "This session has already been fuzzed. Skipping.\n";
        result_after.found = session_get_result(fuzz_sessions, this_session) == 1 ? true : false;
        result_after.stable = session_get_stable(fuzz_sessions, this_session) == 1 ? true : false;
        logfile << "The bug was " << (result_after.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
    }
    
    k = get_index_by_name(kernel_versions, bisect_version.name) + 1;
    if (k >= kernel_versions.size() && result_after.found)
    {
        // make case for guilty merge here
        if (kernel_versions.at(k - 1).name == merge_commit.name)
        {
            cout << "This bug is findable at the guilty merge commit.\n";
            revealing_factor = "Guilty Merge";
        }
        else
        {
            cout << "This bug is findable at the guilty commit.\n";
            revealing_factor = "Guilty Commit";
        }
        reveal_version = Version(kernel_versions.at(k - 1).name, kernel_versions.at(k - 1).date);
        reveal_name = get_commit_name(bug.kerneldir, kernel_versions.at(k - 1).name);

        goto report;
    }

    linux_version = kernel_versions.at(k);
    this_session = Session(linux_version, syzkaller_version, false);
    log_session_info(logfile, this_session, bisector.inc_session());
    
    if (!already_fuzzed(fuzz_sessions, this_session))
    {
        cout << SPACER
             << "Making the kernel\n";
        compiler = get_compiler(gcc_versions, clang_versions, linux_version.date, inspector);
        log_session_compiler(logfile, compiler);
        err = prep_kernel(bug, inspector, linux_version, env.linux_repo_remote, compiler);
        clean_path(env.origin_path);
        if (err < 0)
        {
            log_kernel_build_error(logfile);
            goto finish;
        }

        cout << SPACER
            << "Prepping Syzkaller\n";
        err = prep_syzkaller(bug, inspector, syzkaller_version);
        if (err < 0)
        {
            log_syzkaller_build_error(logfile);
            goto finish;
        }

        cout << SPACER;
        result_before = fuzz_loop(logfile, bug, inspector, bug.duplicates, env.max_time, env.fuzztimes, syzkaller_version.date, env.use_poc);
        log_session_result(logfile, result_before, bug.duplicates);
        blocking_bugs.count_blocking_bugs(result_before);
        if (check_safe_mode(result_before, env.safe_mode, env.max_time, env.fuzztimes))
            log_safe_mode(logfile, env.max_time, env.fuzztimes);

        this_session.stable = result_before.stable;
        this_session.found = result_before.found;
        kernel_versions.at(k).skipped = !result_before.stable && !result_before.found;
        fuzz_sessions.push_back(this_session);
    }
    else
    {
        cout << "This session has already been fuzzed. Skipping.\n";
        result_before.found = session_get_result(fuzz_sessions, this_session) == 1 ? true : false;
        result_before.stable = session_get_stable(fuzz_sessions, this_session) == 1 ? true : false;
        logfile << "The bug was " << (result_before.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
    }

    // check the results
    if (result_after.found && !result_before.found)
    {
        revealing_factor = "Kernel Commit";
        reveal_version = Version(bisect_version.name, bisect_version.date);
        reveal_name = get_commit_name(bug.kerneldir, bisect_version.name);

        cout << SPACER
             << "Revealing Factor Found!\n"
             << "Kernel commit " << reveal_version.name << " on " << reveal_version.date.get_date() << " is the reason.\n";

        goto report;
    }

    // ======================================================================================================
    // Finish
    // ======================================================================================================

report:
    logfile << "\n" << SPACER
            << "Revealing Factor:   " << revealing_factor << "\n" << flush;

    if (revealing_factor == "Unstable Commits")
        logfile << "Unstable Range:     " << "[" << kernel_versions.at(bisector.right).date.get_date() << ", " << kernel_versions.at(bisector.left).date.get_date() << "]\n";
    
    logfile << "Version:            " << reveal_version.date.get_date() << " - " << reveal_version.name << "\n"
            << "Commit Name:        " << reveal_name << "\n\n"
            << "Bug Name:           " << bug.name << "\n"
            << "Bug Link:           " << bug.buglink << "\n"
            << "Arch:               " << bug.arch << "\n"
            << "Finding Date:       " << bisector.find_date.get_date() << "\n"
            << "Finding Commit:     " << finding_version.date.get_date() << " - " << finding_version.name << "\n";
    
    if (!merge_commit.name.empty())
        logfile << "Guilty Merge:       " << merge_commit.date.get_date() << " - " << merge_commit.name << "\n";
    else
        logfile << "Guilty Merge:       " << "None\n";
        
    logfile << "Guilty Commit:      " << guilty_version.date.get_date() << " - " << guilty_version.name << "\n" << flush;

    logfile << "\nPossible Blocking Bugs:\n";
    for (string b : blocking_bugs.list_blocking_bugs())
        logfile << "    " << b << endl;

finish:
    cout << SPACER
        << "Cleaning up...";

    // clean up reproducer and config
    if (!check_file(bug.wd + "/old"))
        make_dir(bug.wd + "/old");

    if (check_file(bug.kconfig))
        move(bug.kconfig, bug.wd + "/old");
    
    if (check_file(bug.allreproducer))
        move(bug.allreproducer, bug.wd + "/old");

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

int main(int argc, char ** argv)
{
    Argparse args;
    Environment env;
    InspectorConfig inspector;
    Bug_Info bug;


    string find_hash, guilty_hash, 
           linux_repo_remote,
           compiler,
           revealing_factor = "",
           reveal_name = "";

    VMConfig vmc;
    Port_Info port;
    Date high_date, low_date, find_date;

    Version linux_version, syzkaller_version, prev_syzkaller_version,
            current_version, bisect_version, merge_commit,
            guilty_version, finding_version, reveal_version;
    Session this_session;
    Test_Result result, result_before, result_after;

    Blocking_Bugs blocking_bugs;
    vector<Version> gcc_versions, clang_versions,
                    kernel_versions, syzkaller_versions,
                    template_changes, relevant_template_changes;
    vector<Session> fuzz_sessions;

    

    args.expect("FGfmidh");
    args.expect(vector<string>({ "setup-only", "help", "recover", "no-merge", "no-poc", "find-only", "safe-mode" }));
    args.parse(argc, argv);
    if (args.is_set('h') || args.is_set("help"))
    {
        print_help();
        return 0;
    }

    set_timezone("UTC0");

    if (args.is_set('F'))
        find_hash = args.get_arg_as_string('F');
    else
    {
        cout << "Error: No finding commit given. Please use -F [hash].\n";
        return -1;
    }

    if (args.is_set('G'))
        guilty_hash = args.get_arg_as_string('G');
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
        find_date = Date(args.get_arg_as_string('f'));

    // get config for how to run
    cout << SPACER
         << "Parsing configs.\n";
    inspector.parse_config_file("parameters/config.cfg");

    // get information about the bug
    bug.parse_config_file("wd-inspector-" + to_string(env.id) + "/" + "bug.cfg");

    export_go(inspector);
    gcc_versions = grab_compiler_versions(inspector.get_gcc_dir() + "/gccVersions.csv");
    clang_versions = grab_compiler_versions(inspector.get_gcc_dir() + "/clangVersions.csv");
    env.origin_path = get_path();

    // allow for fuzzing without the poc
    env.use_poc = !args.is_set("no-poc");

    // allow for fuzzing only at the finding commit
    env.find_only = args.is_set("find-only");

    // allow for ignoring the merge as a revealing factor
    env.no_merge = args.is_set("no-merge");
    
    if(args.is_set("safe-mode") || bug.name.substr(0, 11) == "memory leak")
        set_safe_mode(env.safe_mode, env.max_time, env.fuzztimes);

    // make sure all of the needed files are here.
    cout << SPACER
        << "Checking files.\n";
    if (!check_file(bug.wd + "/log"))
    {
        cout << "Creating log directory.\n";
        make_dir(bug.wd + "/log");
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

    if (!check_file(inspector.get_image_dir() + "/stretch/stretch.img"))
    {
        cerr << "Error: No image file for stretch.\n";
        return -1;
    }
    else
        cout << "Found stretch image.\n";

    if (!check_file(inspector.get_image_dir() + "/wheezy/wheezy.img"))
    {
        cerr << "Error: No image file for wheezy.\n";
        return -1;
    }
    else
        cout << "Found wheezy image.\n";

    return bisect(args, env, inspector, bug);
}
