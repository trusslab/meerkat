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

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char ** argv)
{
    bool useclang = false, use_poc = true, find_only = false, no_merge = false, safe_mode = false;
    int max_time = 30, fuzztimes = 3, id, session_count = 0, r, l, m, err = 0, k;
    string find_hash, guilty_hash, 
           linux_repo_remote, logfilename,
           compiler, tmp_path,
           revealing_factor = "",
           reveal_name = "";

    VMConfig vmc;
    Port_Info port;
    Date high_date, low_date, find_date;
    InspectorConfig inspector;
    Bug_Info bug;
    Argparse args;

    Version linux_version, syzkaller_version, prev_syzkaller_version,
            current_version, bisect_version, merge_commit,
            guilty_version, finding_version, reveal_version;
    Session this_session;
    Test_Result result, result_before, result_after;
    
    int git_err;

    ofstream logfile;
    vector<string> duplicates;
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
        return 0;
    }

    set_timezone("UTC0");

    if (args.is_set('F'))
        find_hash = args.get_arg_as_string('F');
    else
    {
        cout << "Error: No finding commit given. Please use -F [hash].\n";
        goto finish;
    }

    if (args.is_set('G'))
        guilty_hash = args.get_arg_as_string('G');
    else
    {
        cout << "Error: No guilty commit given. Please use -G [hash].\n";
        goto finish;
    }

    if (args.is_set('m'))
        max_time = args.get_arg_as_int('m');

    if (args.is_set('i'))
        id = args.get_arg_as_int('i');
    else
    {
        cout << "Error: No id given. Please use -i [id]\n";
        err = -1;
        goto finish;
    }

    if(args.is_set("safe-mode"))
        set_safe_mode(safe_mode, max_time, fuzztimes);

    if (args.is_set('f'))
        find_date = Date(args.get_arg_as_string('f'));

    // get config for how to run
    cout << SPACER
         << "Parsing configs.\n";
    inspector.parse_config_file("parameters/config.cfg");

    // get information about the bug
    bug.parse_config_file("wd-inspector-" + to_string(id) + "/" + "bug.cfg");

    export_go(inspector);
    gcc_versions = grab_compiler_versions(inspector.get_gcc_dir() + "/gccVersions.csv");
    clang_versions = grab_compiler_versions(inspector.get_gcc_dir() + "/clangVersions.csv");
    tmp_path = get_path();

    // make sure all of the needed files are here.
    cout << SPACER
        << "Checking files.\n";
    if (!check_file(bug.get_wd() + "/log"))
    {
        cout << "Creating log directory.\n";
        make_dir(bug.get_wd() + "/log");
    }
    else
        cout << "Found log directory.\n";

    if (!check_file(bug.get_allrepro()))
    {
        cerr << "Error: No reproducer file " << bug.get_allrepro() << " exists.\n";
        err = -1;
        goto finish;
    }
    else
        cout << "Found reproducer.\n";

    if (!check_file(bug.get_kconfig()))
    {
        cerr << "Error: No kernel config file " << bug.get_kconfig() << " exists.\n";
        err = -1;
        goto finish;
    }
    else
        cout << "Found kernel config.\n";

    if (!check_file("image/stretch/stretch.img"))
    {
        cerr << "Error: No image file for stretch.\n";
        err = -1;
        goto finish;
    }
    else
        cout << "Found stretch image.\n";

    if (!check_file("image/wheezy/wheezy.img"))
    {
        cerr << "Error: No image file for wheezy.\n";
        err = -1;
        goto finish;
    }
    else
        cout << "Found wheezy image.\n";

    // Set up syzkaller repository locally
    if (!check_file(bug.get_syzdir()))
    {
        cout << "Creating Syzkaller directory.\n";
        make_dir(bug.get_syzdir());
    }
    else
        cout << "Found Syzkaller directory.\n";

    if (!check_file(bug.get_syzdir() + "/.git"))
    {
        cout << "Cloning Syzkaller repository...\n";
        if (git_clone(SYZKALLER_REPO_REMOTE, bug.get_syzdir()) < 0)
        {
            cerr << "Error: Git clone failed.\n";
            err = -1;
            goto finish;
        }
    }
    else
        cout << "Found Syzkaller local repository.\n";

    // Set up linux repository locally
    linux_repo_remote = LINUX_REPO_REMOTE + bug.get_repo();

    if (!check_file(bug.get_kerneldir()))
    {
        cout << "Creating kernel directory.\n";
        make_dir(bug.get_kerneldir());
    }
    else
        cout << "Found kernel directory.\n";

    // this may cause issues when the repository opened is not the one we want
    if (!check_file(bug.get_kerneldir() + "/.git"))
    {
        cout << "Cloning Linux repository...\n";
        if (git_clone(linux_repo_remote, bug.get_kerneldir()) < 0)
        {
            cerr << "Error: Git clone failed.\n";
            err = -1;
            goto finish;
        }
    }
    else
        cout << "Found Linux local repository.\n";

    // begin logging
    logfilename = bug.get_wd() + "/log/bug" + to_string(bug.get_number()) + ".log";
    logfile.open(logfilename);
    if(!logfile)
    {
        cerr << "Error: Failed to open log file " << logfilename << ".\n";
        err = -1;
        goto finish;
    }

    logfile << bug.get_name() << "," << bug.get_buglink() << endl
            << "Repository: " << bug.get_kpref() << endl
            << "Arch: " << bug.get_arch() << endl
            << "Finding: " << git_get_commit_date(bug.get_wd(), bug.get_kerneldir(), find_hash).get_date() << " - " << find_hash << endl
            << "Guilty:  " << git_get_commit_date(bug.get_wd(), bug.get_kerneldir(), guilty_hash).get_date() << " - " << guilty_hash << endl << flush;

    // Parse Syzbot for duplicate bugs
    cout << SPACER
        << "Gathering bug fixes from Syzbot.\n";

    duplicates = gather_duplicates(bug);

    if (duplicates.size() > 1)
    {
        cout << "Duplicate Bugs:\n";
        logfile << "Duplicate Names:\n";
        for (string s : duplicates)
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

    port.init(START_PORT, id, 5);

    vmc = determine_threadedness(inspector, bug, logfile);

    // allow for fuzzing without the poc
    if (args.is_set("no-poc"))
    {
        logfile << "no-poc is set.\n";
        use_poc = false;
    }

    // allow for fuzzing only at the finding commit
    if (args.is_set("find-only"))
    {
        logfile << "find-only is set.\n";
        find_only = true;
    }

    // allow for ignoring the merge as a revealing factor
    if (args.is_set("no-merge"))
    {
        logfile << "no-merge is set.\n";
        no_merge = true;
    }

    logfile << "Max time:" << max_time << endl 
            << "Max attempts:" << fuzztimes << endl << flush;
    
    // ======================================================================================================
    // Begin Inspection
    // ======================================================================================================

    session_count = 0;

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

    finding_version.name = find_hash;
    guilty_version.name = guilty_hash;
    finding_version.date = kernel_versions.front().date;
    if (args.is_set('f'))
    {
        find_date.set_delim('-');
        high_date = find_date;
    }
    else
        high_date = find_date = finding_version.date;

    low_date = guilty_version.date = kernel_versions.back().date;
    if (low_date < SYZBOT_BEGIN_DATE)
        low_date = SYZBOT_BEGIN_DATE;

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
    syzkaller_version = get_version_by_date(syzkaller_versions, high_date);

    this_session = Session(linux_version, syzkaller_version, syzkaller_version, false);
    log_session_info(logfile, this_session, ++session_count);

    cout << "Making the kernel.\n";
    compiler = export_compiler(gcc_versions, clang_versions, linux_version.date, inspector, useclang);
    log_session_compiler(logfile, compiler);
    err = prep_kernel(bug, inspector, linux_version, linux_repo_remote);
    clean_path(tmp_path);
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
        write_syzkaller_config(bug, inspector, vmc, port, syzkaller_version.date);
        reset_kaller_wd(bug.get_kallerwd());
        logfile << "Setup-only complete.\n" << flush;
        cout << "Setup complete.\n";
        goto setup_only_finish;
    }

    result = fuzz_loop_finding(logfile, bug, inspector, duplicates, max_time, fuzztimes, vmc, port, syzkaller_version.date, use_poc, find_only);
    max_time = (safe_mode ? max_time : result.suggest_ttf);
    log_session_result(logfile, result, duplicates);
    blocking_bugs.count_blocking_bugs(result);

    if (find_only)
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
        logfile << "New Max Time: " << max_time << ".\n" << flush;
    }

    this_session.found = result.found;
    this_session.stable = result.stable;
    kernel_versions.at(k).skipped = !result.stable && !result.found;
    fuzz_sessions.push_back(this_session);
    
    //  ======================================================================================================
    // Find Merge Commit

    if (!no_merge)
    {
        cout << SPACER
             << "Looking for Merge Commit...\n";

        merge_commit = git_find_merge_commit(bug.get_kerneldir(), kernel_versions, guilty_hash);

        if (!merge_commit.name.empty())
        {
            cout << "Merge commit found: " << merge_commit.name << ".\n";
            logfile << "Merge Commit: " << merge_commit.date.get_date() << " - " << merge_commit.name << ".\n" << flush;
            low_date = merge_commit.date > SYZBOT_BEGIN_DATE ? merge_commit.date : SYZBOT_BEGIN_DATE;
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
    // Begin Template Inspection

    // Gather Template Changes
    cout << SPACER
         << "Gathering template_changes...\n";
    template_changes = get_template_changes(bug, low_date, high_date, syzkaller_versions);
    cout << "Found " << template_changes.size() << " template_changes.\n";

    cout << SPACER
         << "Checking templates for relevance...\n";
    if (template_changes.size() > 1)
    {
        relevant_template_changes = get_relevant_template_changes(bug, template_changes);
    }
    else
        cout << "No template changes in the date range [" << low_date.get_date() << ", " << high_date.get_date() << "].\n";

    // Inspect Each Template Change
    if (relevant_template_changes.size() > 1) {
        cout << SPACER
             << "Inspecting " << relevant_template_changes.size() - 1 << " template changes.\n";
        logfile << "Inspecting " << relevant_template_changes.size() - 1 << " template changes in ["
                << low_date.get_date() << ", " << high_date.get_date() << "].\n" << flush;

        // maybe binary search here
        for (int i = 0; i < relevant_template_changes.size() - 1; i++)
        {
            // fuzz before
            current_version = relevant_template_changes.at(i);
            linux_version = get_version_by_date(kernel_versions, current_version.date);
            // previous (older) means incrementing the index
            prev_syzkaller_version = relevant_template_changes.at(i + 1);

            this_session = Session(linux_version, current_version, prev_syzkaller_version, false);
            log_session_info(logfile, this_session, ++session_count);

            if (!already_fuzzed(fuzz_sessions, this_session))
            {
                cout << SPACER
                    << "Making the kernel\n";
                compiler = export_compiler(gcc_versions, clang_versions, linux_version.date, inspector, useclang);
                log_session_compiler(logfile, compiler);
                err = prep_kernel(bug, inspector, linux_version, linux_repo_remote);
                clean_path(tmp_path);
                if (err < 0)
                {
                    log_kernel_build_error(logfile);
                    goto finish;
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
                err = prep_syzkaller(bug, inspector, current_version, bug.get_wd() + "/my_template.txt");
                if (err < 0)
                {
                    log_syzkaller_build_error(logfile);
                    goto finish;
                }

                cout << SPACER;
                result_before = fuzz_loop(logfile, bug, inspector, duplicates, max_time, fuzztimes, vmc, port, current_version.date, use_poc);
                log_session_result(logfile, result_before, duplicates);
                blocking_bugs.count_blocking_bugs(result_before);
                if (check_safe_mode(result_before, safe_mode, max_time, fuzztimes))
                    log_safe_mode(logfile, max_time, fuzztimes);

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

            if (result_before.found)
            {
                cout << "The bug can be found before this day. Moving on.\n";
                high_date = linux_version.date;
                continue;
            }
            else if (!result_before.stable)
                continue;

            this_session = Session(linux_version, current_version, current_version, false);
            log_session_info(logfile, this_session, ++session_count);

            if (!already_fuzzed(fuzz_sessions, this_session))
            {
                // no need to rebuild the kernel.
                cout << SPACER
                    << "Making Syzkaller\n";
                err = prep_syzkaller(bug, inspector, current_version);
                if (err < 0)
                {
                    log_kernel_build_error(logfile);
                    goto finish;
                }

                cout << SPACER;
                result_after = fuzz_loop(logfile, bug, inspector, duplicates, max_time, fuzztimes, vmc, port, current_version.date, use_poc);
                log_session_result(logfile, result_after, duplicates);
                blocking_bugs.count_blocking_bugs(result_after);
                if (check_safe_mode(result_after, safe_mode, max_time, fuzztimes))
                    log_safe_mode(logfile, max_time, fuzztimes);

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

            if (!result_before.found && result_after.found)
            {
                revealing_factor = "Template Update";
                reveal_version = Version(current_version.name, current_version.date);
                reveal_name = get_commit_name(bug.get_syzdir(), current_version.name);

                cout << SPACER
                     << "Revealing Factor Found!\n"
                     << "Template update " << reveal_version.name << " on " << reveal_version.date.get_date() << " is the reason.\n";

                goto report;
            }

            if (result_after.found)
                high_date = linux_version.date;
            if (!result_after.stable)
                break;
            else
            {
                low_date = linux_version.date;
                break;
            }
        }
    }
    else
    {
        cout << SPACER
             << "No template updates to inspect. Skipping.\n";
        logfile << "No template updates to inspect.\n" << flush;
    }

    // ======================================================================================================
    // Begin Kernel Inspection

    // r is the starting date. older date (lower). higher index
    // l is the ending date. recent date (higher). lower index
    r = get_starting_index(kernel_versions, low_date);
    l = get_ending_index(kernel_versions, high_date);
    m;

    cout << SPACER
         << "Inspecting " << r - l << " kernel versions.\n";
    logfile << "Inspecting " << r - l << " kernel versions in the range [" << low_date.get_date() << ", " << high_date.get_date() << "].\n" << flush;

    while (l <= r)
    {
        // find the next stable version to test. If there are none, end the search and report a date range.
        m = get_next_commit_binary(r, l, kernel_versions);
        if (m < 0)
        {
            cout << "There are no more stable commits to test.\n";
            revealing_factor = "Unstable Commits";
            reveal_version = kernel_versions.at(l);
            reveal_name = get_commit_name(bug.get_kerneldir(), kernel_versions.at(l).name);
            goto report;
        }
        linux_version = kernel_versions.at(m);
        syzkaller_version = get_version_by_date(syzkaller_versions, linux_version.date);
        this_session = Session(linux_version, syzkaller_version, syzkaller_version, false);
        log_session_info(logfile, this_session, ++session_count);

        if (!already_fuzzed(fuzz_sessions, this_session))
        {
            cout << SPACER
                 << "Making the kernel\n";
            compiler = export_compiler(gcc_versions, clang_versions, linux_version.date, inspector, useclang);
            log_session_compiler(logfile, compiler);
            err = prep_kernel(bug, inspector, linux_version, linux_repo_remote);
            clean_path(tmp_path);
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
            result = fuzz_loop(logfile, bug, inspector, duplicates, max_time, fuzztimes, vmc, port, syzkaller_version.date, use_poc);
            log_session_result(logfile, result, duplicates);
            blocking_bugs.count_blocking_bugs(result);
            if (check_safe_mode(result, safe_mode, max_time, fuzztimes))
                log_safe_mode(logfile, max_time, fuzztimes);

            this_session.found = result.found;
            this_session.stable = result.stable;
            kernel_versions.at(m).skipped = !result.stable && !result.found;
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
            l = m + 1;
            bisect_version = linux_version;
        }
        else if (!result.stable)
            continue;
        else
            r = m - 1;
    }

    // fuzz before and after the linux version to confirm
    cout << SPACER
         << "Checking if the kernel is the revealing factor.\n";
    logfile << "Confirming the bisected kernel commit.\n" << flush;
    syzkaller_version = get_version_by_date(syzkaller_versions, bisect_version.date);
    this_session = Session(bisect_version, syzkaller_version, syzkaller_version, false);
    log_session_info(logfile, this_session, ++session_count);

    if (!already_fuzzed(fuzz_sessions, this_session))
    {
        // fuzz after first
        // check if we need to fetch anything
        if (bisect_version != linux_version)
        {
            cout << SPACER
                << "Making the kernel\n";
            compiler = export_compiler(gcc_versions, clang_versions, bisect_version.date, inspector, useclang);
            log_session_compiler(logfile, compiler);
            err = prep_kernel(bug, inspector, bisect_version, linux_repo_remote);
            clean_path(tmp_path);
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
        result_after = fuzz_loop(logfile, bug, inspector, duplicates, max_time, fuzztimes, vmc, port, syzkaller_version.date, use_poc);
        log_session_result(logfile, result_after, duplicates);
        blocking_bugs.count_blocking_bugs(result_after);
        if (check_safe_mode(result_after, safe_mode, max_time, fuzztimes))
            log_safe_mode(logfile, max_time, fuzztimes);

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
        reveal_name = get_commit_name(bug.get_kerneldir(), kernel_versions.at(k - 1).name);

        goto report;
    }

    linux_version = kernel_versions.at(k);
    this_session = Session(linux_version, syzkaller_version, syzkaller_version, false);
    log_session_info(logfile, this_session, ++session_count);
    
    if (!already_fuzzed(fuzz_sessions, this_session))
    {
        cout << SPACER
             << "Making the kernel\n";
        compiler = export_compiler(gcc_versions, clang_versions, linux_version.date, inspector, useclang);
        log_session_compiler(logfile, compiler);
        err = prep_kernel(bug, inspector, linux_version, linux_repo_remote);
        clean_path(tmp_path);
        if (err < 0)
        {
            log_kernel_build_error(logfile);
            goto finish;
        }

        cout << SPACER;
        result_before = fuzz_loop(logfile, bug, inspector, duplicates, max_time, fuzztimes, vmc, port, syzkaller_version.date, use_poc);
        log_session_result(logfile, result_before, duplicates);
        blocking_bugs.count_blocking_bugs(result_before);
        if (check_safe_mode(result_before, safe_mode, max_time, fuzztimes))
            log_safe_mode(logfile, max_time, fuzztimes);

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
        reveal_name = get_commit_name(bug.get_kerneldir(), bisect_version.name);

        cout << SPACER
             << "Revealing Factor Found!\n"
             << "Kernel commit " << reveal_version.name << " on " << reveal_version.date.get_date() << " is the reason.\n";

        goto report;
    }
    
    high_date = low_date = bisect_version.date;

    // ======================================================================================================
    // Begin Syzkaller Inspection

    // start with the newest and go back
    r = get_starting_index(syzkaller_versions, low_date);
    l = get_ending_index(syzkaller_versions, high_date);

    cout << SPACER
         << "Inspecting " << r - l + 1 << " syzkaller version(s).\n";
    logfile << "Inspecting " << r - l + 1 << " syzkaller version(s) from " << high_date.get_date() << ".\n" << flush;

    // we only need one kernel version
    cout << SPACER
         << "Making the kernel\n";
    compiler = export_compiler(gcc_versions, clang_versions, bisect_version.date, inspector, useclang);
    err = prep_kernel(bug, inspector, bisect_version, linux_repo_remote);
    clean_path(tmp_path);
    if (err < 0)
    {
        log_kernel_build_error(logfile);
        goto finish;
    }

    for (int i = l; i <= r; i++)
    {
        syzkaller_version = syzkaller_versions.at(i);
        this_session = Session(bisect_version, syzkaller_version, syzkaller_version, false);
        log_session_info(logfile, this_session, ++session_count);

        if (!already_fuzzed(fuzz_sessions, this_session))
        {
            cout << SPACER
                << "Prepping Syzkaller\n";
            err = prep_syzkaller(bug, inspector, syzkaller_version);
            if (err < 0)
            {
                log_syzkaller_build_error(logfile);
                goto finish;
            }

            // run without the poc
            cout << SPACER;
            result = fuzz_loop(logfile, bug, inspector, duplicates, (max_time > 60 ? max_time : 60), fuzztimes, vmc, port, syzkaller_version.date, false);
            log_session_result(logfile, result, duplicates);
            blocking_bugs.count_blocking_bugs(result);

            this_session.stable = result.stable;
            this_session.found = result.found;
            fuzz_sessions.push_back(this_session);
        }
        else
        {
            cout << "This session has already been fuzzed. Skipping.\n";
            result.found = session_get_result(fuzz_sessions, this_session) == 1 ? true : false;
            result.stable = session_get_stable(fuzz_sessions, this_session) == 1 ? true : false;
            logfile << "The bug was " << (result.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
        }

        if (!result.found)
        {
            syzkaller_version = syzkaller_versions.at(i - 1);
            break;
        }
    }

    if (get_index_by_name(syzkaller_versions, syzkaller_version.name) > r)
    {
        revealing_factor = "Syzkaller Update";
        reveal_version = Version("Unknown", syzkaller_version.date);
        reveal_name = "Unknown";

        cout << SPACER
             << "Revealing Factor Found!\n"
             << "Exact Syzkaller commit unknown.\n";
    }
    else
    {
        revealing_factor = "Syzkaller Update";
        reveal_version = Version(syzkaller_version.name, syzkaller_version.date);
        reveal_name = get_commit_name(bug.get_syzdir(), syzkaller_version.name);

        cout << SPACER
             << "Revealing Factor Found!\n"
             << "Syzkaller update from " << reveal_version.name << " on " << reveal_version.date.get_date() << " is the reason.\n";
    }

    // ======================================================================================================
    // Finish
    // ======================================================================================================

report:
    logfile << "\n" << SPACER
            << "Revealing Factor:   " << revealing_factor << "\n" << flush;

    if (revealing_factor == "Unstable Commits")
        logfile << "Unstable Range:     " << "[" << kernel_versions.at(r).date.get_date() << ", " << kernel_versions.at(l).date.get_date() << "]\n";
    
    logfile << "Version:            " << reveal_version.date.get_date() << " - " << reveal_version.name << "\n"
            << "Commit Name:        " << reveal_name << "\n\n"
            << "Bug Name:           " << bug.get_name() << "\n"
            << "Bug Link:           " << bug.get_buglink() << "\n"
            << "Arch:               " << bug.get_arch() << "\n"
            << "Finding Date:       " << find_date.get_date() << "\n"
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
    if (check_faulty_result(bug))
    {
        cout << "Revealing factor marked as faulty.\n";
        logfile << "Warning: Revealing factor may be faulty.\n" << flush;
    }

    cout << SPACER
        << "Cleaning up...";

    // clean up reproducer and config
    if (!check_file(bug.get_wd() + "/old"))
        make_dir(bug.get_wd() + "/old");

    if (check_file(bug.get_kconfig()))
        move(bug.get_kconfig(), bug.get_wd() + "/old");
    
    if (check_file(bug.get_allrepro()))
        move(bug.get_allrepro(), bug.get_wd() + "/old");

    remove_files_in_dir(bug.get_repro());

setup_only_finish:
    if (logfile)
    {
        logfile << flush;
        logfile.close();
    }
    
    cout << "Done.\n" << flush;
    return err;
}
