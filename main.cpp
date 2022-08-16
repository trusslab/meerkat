#include <date.h>
#include <argparse.h>
#include <bug_info.h>
#include <inspector_config.h>
#include <file_api.h>
#include <shell_api.h>
#include <psf.h>
#include <fuzz_prep.h>
#include <inspect.h>
#include <consts.h>
#include <template_parse.h>
#include <git_api.h>
#include <session.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <git2.h>

using namespace std;

int main(int argc, char ** argv)
{
    bool useclang = false, use_poc = true;
    int max_time = 30, id, session_count = 0, r, l, m, err = 0, k;
    string find_hash, guilty_hash, 
           linux_repo_remote, logfilename,
           compiler,
           tmp_path, tmp_snapshotfile,
           template1, template2, template3, template_dir,
           kernel_commit_name = "";

    VMConfig vmc;
    Port_Info port;
    Date high_date, low_date;
    InspectorConfig inspector;
    Bug_Info bug;
    Argparse args;

    Version linux_version, syzkaller_version, prev_syzkaller_version,
            current_version, bisect_version, merge_commit;
    Session this_session;
    Syzkaller_Result result, result_before, result_after;
    
    git_repository *syzkaller_repo = nullptr;
    git_repository *linux_repo = nullptr;
    int git_err;

    ofstream logfile;
    vector<int> ttfs;
    vector<string> duplicates;
    vector<Version> gcc_versions, clang_versions,
                    kernel_versions, syzkaller_versions,
                    template_changes, relevant_template_changes;
    vector<Session> fuzz_sessions;

    port.start_port = START_PORT;
    port.port_count = 0;
    port.port = 0;

    args.expect("FGmidh");
    args.expect(vector<string>({ "setup-only", "help", "recover", "no-merge", "no-poc" }));
    args.parse(argc, argv);
    if (args.is_set('h') || args.is_set("help"))
    {
        cout << "Help:\n"
            << "Short Ticks:\n"
            << "    -F [find_hash]: the hash of the finding commit.\n"
            << "    -G [guilty_hash]: the hash of the guilty commit.\n"
            << "    -m [max_time]: the maximum time allowed when fuzzing.\n"
            << "    -i [id]: REQUIRED. The id of the inspector.\n"
            << "Long Ticks:\n"
            << "    --setup-only: download and build all the parts, but don't actually fuzz.\n"
            << "    --recover: enter recovery mode.\n"
            << "    --no-merge: don't use the merge commit as a revealing factor.\n"
            << endl;
        return 0;
    }

    git_libgit2_init();
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

    // allow for fuzzing without the poc
    if (args.is_set("no-poc"))
        use_poc = false;

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

    if (!check_file(bug.get_repro()))
    {
        cerr << "Error: No reproducer file " << bug.get_repro() << " exists.\n";
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

    git_err = git_repository_open(&syzkaller_repo, bug.get_syzdir().c_str());
    if (git_err < 0)
    {
        cout << "Cloning Syzkaller repository...\n";
        git_err = git_clone(&syzkaller_repo, SYZKALLER_REPO_REMOTE.c_str(), bug.get_syzdir().c_str(), nullptr);
        if (git_err < 0)
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
    git_err = git_repository_open(&linux_repo, bug.get_kerneldir().c_str());
    if (git_err < 0)
    {
        cout << "Cloning Linux repository...\n";
        git_err = git_clone(&linux_repo, linux_repo_remote.c_str(), bug.get_kerneldir().c_str(), nullptr);
        if (git_err < 0)
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
            << "Finding: " << find_hash << endl
            << "Guilty: " << guilty_hash << endl << flush;

    // Parse Syzbot for duplicate bugs
    cout << SPACER
        << "Gathering bug fixes from Syzbot.\n";

    tmp_snapshotfile = bug.get_wd() + "/snapshot";

    lynx_dump(SYZBOT_FIXED_LINK, tmp_snapshotfile);
    trim_syzbot_fixes(tmp_snapshotfile);
    parse_syzbot_fixes(tmp_snapshotfile, bug.get_name(), duplicates);
    parse_manual_duplicates("parameters/manual_duplicates.txt", bug.get_name(), duplicates);

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

    remove_file(tmp_snapshotfile);
    cout << SPACER;

    port.init(id);

    vmc = determine_threadedness(inspector, bug, logfile);

    logfile << "Max time:" << max_time << endl << flush;
    
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

    high_date = kernel_versions.front().date;
    low_date = kernel_versions.back().date;
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
    logfile << "Inspecting the finding commit.\n" << flush;

    k = get_index_by_name(kernel_versions, find_hash);
    if (k < 0)
    {
        cerr << "This kernel version does not exist.\n";
        logfile << "Error: Could not find kernel version " << find_hash << ".\n" << flush;
        goto finish;
    }
    linux_version = kernel_versions.at(k);
    syzkaller_version = get_version_by_date(syzkaller_versions, linux_version.date);
    this_session = Session(linux_version, syzkaller_version, syzkaller_version, false);

    session_count++;
    logfile << "Session " << session_count << ":\n"
            << "    Template:  " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
            << "    Syzkaller: " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
            << "    Kernel:    " << linux_version.date.get_date() << " - " << linux_version.name << "\n" << flush;

    cout << "Making the kernel\n";
    compiler = export_compiler(gcc_versions, clang_versions, linux_version.date, inspector, useclang);
    logfile << "    Compiler:  " << compiler << "\n" << flush;
    err = prep_kernel(bug, inspector, linux_version, linux_repo_remote);
    clean_path(tmp_path);
    if (err < 0)
    {
        logfile << "Error: The kernel failed to make.\n" << flush;
        goto finish;
    }

    cout << SPACER
         << "Prepping Syzkaller\n";
    err = prep_syzkaller(bug, inspector, syzkaller_version);
    if (err < 0)
    {
        logfile << "Error: Syzkaller failed to make.\n" << flush;
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

    result = fuzz_loop_finding(bug, inspector, duplicates, max_time, vmc, port, syzkaller_version.date, use_poc);
    // set the max_time
    max_time = result.ttf;

    logfile << "    The bug was " << (result.found ? "" : "not ") << "found.\n" << flush;
    for (string b : result.bugsfound)
        logfile << "        " << b << "\n" << flush;

    if (!result.found)
    {
        cout << "This bug cannot be found at the finding commit. Ignoring this bug.\n";
        logfile << "\nFailure: This bug cannot be found at the finding commit.\n" << flush;
        goto finish;
    }
    else
    {
        logfile << "New Max Time: " << result.ttf << ".\n" << flush;
    }

    this_session.found = result.found;
    fuzz_sessions.push_back(this_session);
    
    //  ======================================================================================================
    // Find Merge Commit

    if (!args.is_set("no-merge"))
    {
        cout << SPACER
             << "Looking for Merge Commit...\n";

        merge_commit = git_find_merge_commit(bug.get_kerneldir(), kernel_versions, guilty_hash);

        if (!merge_commit.name.empty())
        {
            cout << "Merge commit found: " << merge_commit.name << ".\n";
            logfile << "Merge Commit: " << merge_commit.date.get_date() << " - " << merge_commit.name << ".\n" << flush;
            low_date = merge_commit.date;
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

    relevant_template_changes = {template_changes.back()};

    cout << SPACER
         << "Checking templates for relevance...\n";
    if (template_changes.size() > 1)
    {
        cout << "Fetching version " << template_changes.back().name << ".\n";
        clean_syzkaller(bug);
        git_fetch_and_checkout(bug.get_syzdir(), SYZKALLER_REPO_REMOTE, template_changes.back().name);
        cout << "Slimming version " << template_changes.back().name << ".\n";
        template1 = bug.get_wd() + "/template1.txt";
        template2 = bug.get_wd() + "/template2.txt";
        template3 = bug.get_wd() + "/template3.txt";
        template_dir = check_file(bug.get_syzdir() + "/sys/linux") ? bug.get_syzdir() + "/sys/linux" : bug.get_syzdir() + "/sys";
        slim_template(bug.get_repro(), template1, list_template_files(template_dir), template_changes.back().date < OLD_INOUT_DATE);

        int l = 0;                              // left is always 0 to start 
        int r = template_changes.size() - 2;    // right is one to the left of the commit we are comparing to
        int m = (l + r) / 2;
        bool same;
        while (m > 0)
        {
            while (l <= r)
            {
                m = (l + r) / 2;

                cout << SPACER
                    << "Fetching version " << template_changes.at(m).name << ".\n";
                clean_syzkaller(bug);
                git_fetch_and_checkout(bug.get_syzdir(), SYZKALLER_REPO_REMOTE, template_changes.at(m).name);

                cout << "Slimming version " << template_changes.at(m).name << ".\n";
                template_dir = check_file(bug.get_syzdir() + "/sys/linux") ? bug.get_syzdir() + "/sys/linux" : bug.get_syzdir() + "/sys";
                slim_template(bug.get_repro(), template2, list_template_files(template_dir), template_changes.at(m).date < OLD_INOUT_DATE);

                same = compare_templates(template1, template2);
                if (same)
                {
                    cout << "Moving left.\n";
                    r = m - 1;
                }
                else
                {
                    cout << "Moving right.\n";
                    move(template2, template3);
                    l = m + 1;
                }
            }
            
            // if the result has 2 same templates, the answer is either the commit before, or no more changes
            if (same && m > 0)
            {
                relevant_template_changes.insert(relevant_template_changes.begin(), template_changes.at(m - 1));
                l = 0;
                r = m > 2 ? m - 2: 0;
            }
            else if (!same)
            {
                relevant_template_changes.insert(relevant_template_changes.begin(), template_changes.at(m));
                l = 0;
                r = m > 1 ? m - 1: 0;
            }
            else if (same)
            {
                break;
            }

            cout << "Found a relevant template change.\n";
            move(template3, template1);
        }
        remove_file(template1);
        if (check_file(template2))
            remove_file(template2);
        if (check_file(template3))
            remove_file(template3);
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

            session_count++;
            logfile << "Session " << session_count << " before:\n"
                    << "    Template:  " << prev_syzkaller_version.date.get_date() << " - " << prev_syzkaller_version.name << "\n"
                    << "    Syzkaller: " << current_version.date.get_date() << " - " << current_version.name << "\n"
                    << "    Kernel:    " << linux_version.date.get_date() << " - " << linux_version.name << "\n" << flush;

            if (!already_fuzzed(fuzz_sessions, this_session))
            {
                cout << SPACER
                    << "Making the kernel\n";
                compiler = export_compiler(gcc_versions, clang_versions, linux_version.date, inspector, useclang);
                logfile << "    Compiler:  " << compiler << "\n" << flush;
                err = prep_kernel(bug, inspector, linux_version, linux_repo_remote);
                clean_path(tmp_path);
                if (err < 0)
                {
                    logfile << "Error: The kernel failed to make.\n" << flush;
                    goto finish;
                }

                // pull the previous template
                cout << SPACER
                    << "Prepping the old template\n";
                err = prep_syzkaller(bug, inspector, prev_syzkaller_version);
                if (err < 0)
                {
                    logfile << "Error: Syzkaller failed to make.\n" << flush;
                    goto finish;
                }

                // now compile the current syzkaller using the old template
                cout << SPACER
                    << "Making Syzkaller\n";
                err = prep_syzkaller(bug, inspector, current_version, bug.get_wd() + "/my_template.txt");
                if (err < 0)
                {
                    logfile << "Error: Syzkaller failed to make.\n" << flush;
                    goto finish;
                }

                cout << SPACER;
                result_before = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, current_version.date, use_poc);

                logfile << "    The bug was " << (result_before.found ? "found in " : "not found and timed out at ") << result_before.ttf << " minutes\n" << flush;
                for (string b : result_before.bugsfound)
                    logfile << "        " << b << "\n";

                this_session.found = result_before.found;
                fuzz_sessions.push_back(this_session);
                if (result_before.found)
                    ttfs.push_back(result_before.ttf);
            }
            else
            {
                cout << "This session has already been fuzzed. Skipping.\n";
                result_before.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
                logfile << "The bug was " << (result_before.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
            }

            if (result_before.found)
            {
                cout << "The bug can be found before this day. Moving on.\n";
                continue;
            }

            // fuzz after
            logfile << "Session " << session_count << " after:\n"
                    << "    Template:  " << current_version.date.get_date() << " - " << current_version.name << "\n"
                    << "    Syzkaller: " << current_version.date.get_date() << " - " << current_version.name << "\n"
                    << "    Kernel:    " << linux_version.date.get_date() << " - " << linux_version.name << "\n" << flush;

            this_session = Session(linux_version, current_version, current_version, false);

            if (!already_fuzzed(fuzz_sessions, this_session))
            {
                // no need to rebuild the kernel.
                cout << SPACER
                    << "Making Syzkaller\n";
                err = prep_syzkaller(bug, inspector, current_version);
                if (err < 0)
                {
                    logfile << "Error: The kernel failed to make.\n" << flush;
                    goto finish;
                }

                cout << SPACER;
                result_after = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, current_version.date, use_poc);

                logfile << "    The bug was " << (result_after.found ? "found in " : "not found and timed out at ") << result_after.ttf << " minutes\n" << flush;
                for (string b : result_after.bugsfound)
                    logfile << "        " << b << "\n";

                this_session.found = result_after.found;
                fuzz_sessions.push_back(this_session);
                if (result_after.found)
                    ttfs.push_back(result_after.ttf);
            }
            else
            {
                cout << "This session has already been fuzzed. Skipping.\n";
                result_after.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
                logfile << "The bug was " << (result_after.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
            }

            if (!result_before.found && result_after.found)
            {
                cout << SPACER
                     << "Revealing Factor Found!\n"
                     << "Template update from " << current_version.name << " on " << current_version.date.get_date() << " is the reason.\n";
                
                logfile << "\nSuccess\n"
                        << "Revealing factor: Template Update\n"
                        << "Template Version: " << current_version.date.get_date() << " - " << current_version.name << endl
                        << "Commit name: " << get_commit_name(bug.get_syzdir(), current_version.name) << endl << flush;

                goto finish;
            }

            if (result_after.found)
                high_date = current_version.date;
            else
            {
                low_date = current_version.date;
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
        m = (r + l) / 2;
        linux_version = kernel_versions.at(m);
        syzkaller_version = get_version_by_date(syzkaller_versions, linux_version.date);
        this_session = Session(linux_version, syzkaller_version, syzkaller_version, false);

        session_count++;
        logfile << "Session " << session_count << ":\n"
                    << "    Template:  " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
                    << "    Syzkaller: " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
                    << "    Kernel:    " << linux_version.date.get_date() << " - " << linux_version.name << "\n" << flush;

        if (!already_fuzzed(fuzz_sessions, this_session))
        {
            cout << SPACER
                 << "Making the kernel\n";
            compiler = export_compiler(gcc_versions, clang_versions, linux_version.date, inspector, useclang);
            logfile << "    Compiler:  " << compiler << "\n" << flush;
            err = prep_kernel(bug, inspector, linux_version, linux_repo_remote);
            clean_path(tmp_path);
            if (err < 0)
            {
                logfile << "Error: The kernel failed to make.\n" << flush;
                goto finish;
            }

            cout << SPACER
                 << "Prepping Syzkaller\n";
            err = prep_syzkaller(bug, inspector, syzkaller_version);
            if (err < 0)
            {
                logfile << "Error: Syzkaller failed to make.\n" << flush;
                goto finish;
            }

            cout << SPACER;
            result = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, syzkaller_version.date, use_poc);

            logfile << "    The bug was " << (result.found ? "found in " : "not found and timed out at ") << result.ttf << " minutes\n" << flush;
            for (string b : result.bugsfound)
                logfile << "        " << b << "\n";

            this_session.found = result.found;
            fuzz_sessions.push_back(this_session);
            if (result.found)
                ttfs.push_back(result.ttf);
        }
        else
        {
            cout << "This session has already been fuzzed. Skipping.\n";
            result.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
            logfile << "The bug was " << (result.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
        }

        if (result.found)
        {
            l = m + 1;
            bisect_version = linux_version;
        }
        else
            r = m - 1;
    }

    // fuzz before and after the linux version to confirm
    cout << SPACER
         << "Checking if the kernel is the revealing factor.\n";
    logfile << "Confirming the bisected kernel commit.\n" << flush;
    syzkaller_version = get_version_by_date(syzkaller_versions, bisect_version.date);
    this_session = Session(bisect_version, syzkaller_version, syzkaller_version, false);

    session_count++;
    logfile << "Session " << session_count << " after:\n"
            << "    Template:  " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
            << "    Syzkaller: " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
            << "    Kernel:    " << bisect_version.date.get_date() << " - " << bisect_version.name << "\n" << flush;

    if (!already_fuzzed(fuzz_sessions, this_session))
    {
        // fuzz after first
        // check if we need to fetch anything
        if (bisect_version != linux_version)
        {
            cout << SPACER
                << "Making the kernel\n";
            compiler = export_compiler(gcc_versions, clang_versions, bisect_version.date, inspector, useclang);
            logfile << "    Compiler:  " << compiler << "\n" << flush;
            err = prep_kernel(bug, inspector, bisect_version, linux_repo_remote);
            clean_path(tmp_path);
            if (err < 0)
            {
                logfile << "Error: The kernel failed to make.\n" << flush;
                goto finish;
            }

            cout << SPACER
                << "Prepping Syzkaller\n";
            err = prep_syzkaller(bug, inspector, syzkaller_version);
            if (err < 0)
            {
                logfile << "Error: Syzkaller failed to make.\n" << flush;
                goto finish;
            }
        }

        cout << SPACER;
        result_after = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, syzkaller_version.date, use_poc);

        logfile << "    The bug was " << (result_after.found ? "found in " : "not found and timed out at ") << result_after.ttf << " minutes\n" << flush;
        for (string b : result_after.bugsfound)
            logfile << "        " << b << "\n";

        this_session.found = result_after.found;
        fuzz_sessions.push_back(this_session);
        if (result_after.found)
            ttfs.push_back(result_after.ttf);
    }
    else
    {
        cout << "This session has already been fuzzed. Skipping.\n";
        result_after.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
        logfile << "The bug was " << (result_after.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
    }
    
    k = get_index_by_name(kernel_versions, bisect_version.name) + 1;
    if (k >= kernel_versions.size())
    {
        cout << "This bug is findable at the guilty commit.\n";
        logfile << "\nSuccess\n"
                << "Revealing factor: Guilty Commit\n"
                << "Kernel Version: " << kernel_versions.at(k - 1).date.get_date() << " - " << kernel_versions.at(k - 1).name << endl
                << "Commit name: " << get_commit_name(bug.get_kerneldir(), kernel_versions.at(k - 1).name) << endl << flush;
        goto finish;
    }

    linux_version = kernel_versions.at(k);
    this_session = Session(linux_version, syzkaller_version, syzkaller_version, false);

    session_count++;
    logfile << "Session " << session_count << " before:\n"
            << "    Template:  " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
            << "    Syzkaller: " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
            << "    Kernel:    " << linux_version.date.get_date() << " - " << linux_version.name << "\n" << flush;
    
    if (!already_fuzzed(fuzz_sessions, this_session))
    {
        cout << SPACER
             << "Making the kernel\n";
        compiler = export_compiler(gcc_versions, clang_versions, linux_version.date, inspector, useclang);
        logfile << "    Compiler:  " << compiler << "\n" << flush;
        err = prep_kernel(bug, inspector, linux_version, linux_repo_remote);
        clean_path(tmp_path);
        if (err < 0)
        {
            logfile << "Error: The kernel failed to make.\n" << flush;
            goto finish;
        }

        cout << SPACER;
        result_before = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, syzkaller_version.date, use_poc);

        logfile << "    The bug was " << (result_before.found ? "found in " : "not found and timed out at ") << result_before.ttf << " minutes\n" << flush;
        for (string b : result_before.bugsfound)
            logfile << "        " << b << "\n";

        this_session.found = result_before.found;
        fuzz_sessions.push_back(this_session);
        if (result_before.found)
            ttfs.push_back(result_before.ttf);
    }
    else
    {
        cout << "This session has already been fuzzed. Skipping.\n";
        result_before.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
        logfile << "The bug was " << (result_before.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
    }

    // check the results
    if (result_after.found && !result_before.found)
    {
        cout << SPACER
             << "Revealing Factor Found!\n"
             << "Kernel code change from " << bisect_version.name << " on " << bisect_version.date.get_date() << " is the reason.\n";
                
        logfile << "\nSuccess\n"
                << "Revealing factor: Kernel Code Change\n"
                << "Kernel Version: " << bisect_version.date.get_date() << " - " << bisect_version.name << endl
                << "Commit name: " << get_commit_name(bug.get_kerneldir(), bisect_version.name) << endl << flush;
        
        kernel_commit_name = get_commit_name(bug.get_kerneldir(), bisect_version.name);
        goto finish;
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
        logfile << "Error: The kernel failed to make.\n" << flush;
        goto finish;
    }

    for (int i = l; i <= r; i++)
    {
        syzkaller_version = syzkaller_versions.at(i);
        this_session = Session(bisect_version, syzkaller_version, syzkaller_version, false);

        session_count++;
        logfile << "Session " << session_count << ":\n"
                << "    Template:  " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
                << "    Syzkaller: " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
                << "    Kernel:    " << bisect_version.date.get_date() << " - " << bisect_version.name << "\n"
                << "    Compiler:  " << compiler << "\n" << flush;

        if (!already_fuzzed(fuzz_sessions, this_session))
        {
            cout << SPACER
                << "Prepping Syzkaller\n";
            err = prep_syzkaller(bug, inspector, syzkaller_version);
            if (err < 0)
            {
                logfile << "Error: Syzkaller failed to make.\n" << flush;
                goto finish;
            }

            // run without the poc
            cout << SPACER;
            result = fuzz_loop(bug, inspector, duplicates, 60, vmc, port, syzkaller_version.date, false);

            logfile << "    The bug was " << (result.found ? "found in " : "not found and timed out at ") << result.ttf << " minutes\n" << flush;
            for (string b : result.bugsfound)
                logfile << "        " << b << "\n";

            this_session.found = result.found;
            fuzz_sessions.push_back(this_session);
        }
        else
        {
            cout << "This session has already been fuzzed. Skipping.\n";
            result.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
            logfile << "The bug was " << (result.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
        }

        if (!result.found)
        {
            syzkaller_version = syzkaller_versions.at(i - 1);
            break;
        }
    }

    if (get_index_by_name(syzkaller_versions, syzkaller_version.name) < r)
    {
        cout << SPACER
             << "Revealing Factor Found!\n"
             << "Exact Syzkaller commit unknown.\n";
                
        logfile << "\nSuccess\n"
                << "Revealing factor: Syzkaller Update\n"
                << "Syzkaller Version: Unknown commit from " << syzkaller_version.date.get_date() << ".\n" << flush;
    }
    else
    {
        cout << SPACER
             << "Revealing Factor Found!\n"
             << "Syzkaller update from " << syzkaller_version.name << " on " << syzkaller_version.date.get_date() << " is the reason.\n";
                
        logfile << "\nSuccess\n"
                << "Revealing factor: Syzkaller Update\n"
                << "Syzkaller Version: " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << endl
                << "Commit name: " << get_commit_name(bug.get_syzdir(), syzkaller_version.name) << endl << flush;
    }

    // ======================================================================================================
    // Finish
    // ======================================================================================================

finish:
    if (check_faulty_result(bug, ttfs, max_time, kernel_commit_name))
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
    
    if (check_file(bug.get_repro()))
        move(bug.get_repro(), bug.get_wd() + "/old");

    // clean up syzkaller if syz-env was used
    if (bug.get_arch() == "i386")
        err = syz_env_clean(inspector.get_inspect_dir() + "/tools/syz-env");

setup_only_finish:
    if (logfile)
    {
        logfile << flush;
        logfile.close();
    }

    if (syzkaller_repo)
        git_repository_free(syzkaller_repo);

    if (linux_repo)
        git_repository_free(linux_repo);
    
    git_libgit2_shutdown();
    cout << "Done.\n" << flush;
    return err;
}
