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
    Argparse args;
    args.expect("sefFGmidh");
    args.expect(vector<string>({ "setup-only", "help", "recover" }));
    args.parse(argc, argv);

    int max_time = 2, id;
    Port_Info port;
    port.start_port = START_PORT;
    port.port_count = 0;
    port.port = 0;

    Date start_date, end_date, find_date, kernel_date, syz_date;
    string find_hash, guilty_hash;

    if (args.is_set('h') || args.is_set("help"))
    {
        cout << "Help:\n"
            << "Short Ticks:\n"
            << "    -s [start_date]: the date to start inspecting from (usually guilty date).\n"
            << "    -e [end_date]: the date to stop inspecting on (usually finding date).\n"
            << "    -f [find_date]: the date the bug is known to be found on.\n"
            << "    -F [find_hash]: the hash of the finding commit.\n"
            << "    -G [guilty_hash]: the hash of the guilty commit.\n"
            << "    -m [max_time]: the maximum time allowed when fuzzing.\n"
            << "    -i [id]: REQUIRED. The id of the inspector.\n"
            << "Long Ticks:\n"
            << "    --setup-only: download and build all the parts, but don't actually fuzz.\n"
            << "    --recover: enter recovery mode.\n"
            << endl;
        return 0;
    }

    if (args.is_set('s'))
        start_date.set_date(args.get_arg_as_string('s'));
    
    if (args.is_set('e'))
        end_date.set_date(args.get_arg_as_string('e'));

    if (args.is_set('f'))
        find_date.set_date(args.get_arg_as_string('f'));

    if (args.is_set('F'))
        find_hash = args.get_arg_as_string('F');

    if (args.is_set('G'))
        guilty_hash = args.get_arg_as_string('G');

    if (args.is_set('m'))
        max_time = args.get_arg_as_int('m');

    if (args.is_set('i'))
        id = args.get_arg_as_int('i');
    else
    {
        cout << "Warning: No id given. Please use -i [id]\n";
        return -1;
    }

    // get config for how to run
    cout << "Parsing configs.\n";
    InspectorConfig inspector;
    inspector.parse_config_file("inspector-config/parameters.cfg");

    // get information about the bug
    Bug_Info bug;
    bug.parse_config_file("wd-inspector-" + to_string(id) + "/" + "bug.cfg");

    export_go(inspector);
    vector<Version> gcc_versions = grab_gcc_versions(inspector.get_gcc_dir() + "/gccVersions.csv");
    string tmp_path = get_path();

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
        return -1;
    }
    else
        cout << "Found reproducer.\n";

    if (!check_file(bug.get_kconfig()))
    {
        cerr << "Error: No kernel config file " << bug.get_kconfig() << " exists.\n";
        return -1;
    }
    else
        cout << "Found kernel config.\n";

    if (!check_file("image/stretch/stretch.img"))
    {
        cerr << "Error: No image file for stretch.\n";
        return -1;
    }
    else
        cout << "Found stretch image.\n";

    if (!check_file("image/wheezy/wheezy.img"))
    {
        cerr << "Error: No image file for wheezy.\n";
        return -1;
    }
    else
        cout << "Found wheezy image.\n";

    git_libgit2_init();

    // Set up syzkaller repository locally
    git_repository *syzkaller_repo = nullptr;
    int git_err;

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
            return -1;
        }
    }
    else
        cout << "Found Syzkaller local repository.\n";

    // Set up linux repository locally
    git_repository *linux_repo = nullptr;
    string linux_repo_remote = "https://git.kernel.org/pub/scm/linux/kernel/git/" + bug.get_repo();

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
            return -1;
        }
    }
    else
        cout << "Found Linux local repository.\n";

    // begin logging
    string logfilename = bug.get_wd() + "/log/bug" + to_string(bug.get_number()) + ".log";
    ofstream logfile;
    logfile.open(logfilename);
    if(!logfile)
    {
        cerr << "Error: Failed to open log file " << logfilename << ".\n";
        return -1;
    }

    logfile << bug.get_name() << "," << bug.get_buglink() << endl;

    // Parse Syzbot for duplicate bugs
    cout << SPACER
        << "Gathering bug fixes from Syzbot.\n";

    string tmp_snapshotfile = bug.get_wd() + "/snapshot";

    lynx_dump(SYZBOT_FIXED_LINK, tmp_snapshotfile);
    trim_syzbot_fixes(tmp_snapshotfile);
    vector<string> duplicates = parse_syzbot_fixes(tmp_snapshotfile, bug.get_name());

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

    port.start_port = port.start_port + id * (FUZZTIMES + 1);

    VMConfig vmc = determine_threadedness(inspector, bug, logfile);

    logfile << "Max time:" << max_time << endl;
    
    // ======================================================================================================
    // Begin Inspection
    // ======================================================================================================

    vector<Session> fuzz_sessions;
    int session_count = 0;

    // commits are arranged newest (low index) to oldest (high index)
    cout << SPACER
         << "Gathering kernel versions...\n";
    vector<Version> kernel_versions = get_kernel_versions(bug, guilty_hash, find_hash);
    cout << "Found " << kernel_versions.size() << " kernel commits.\n";

    Date high_date = kernel_versions.front().date, low_date = kernel_versions.back().date;

    // commits are arranged newest (low index) to oldest (high index)
    cout << SPACER
         << "Gathering Syzkaller versions...\n";
    vector<Version> syzkaller_versions = get_syzkaller_versions(bug);
    cout << "Found " << syzkaller_versions.size() << " Syzkaller commits.\n";

    // ======================================================================================================
    // Begin Template Inspection
    cout << SPACER
         << "Gathering template_changes...\n";
    vector<Version> template_changes = get_template_changes(bug, kernel_versions.back().date, kernel_versions.front().date, syzkaller_versions);
    cout << "Found " << template_changes.size() << " template_changes.\n";

    vector<Version> relevant_template_changes = {template_changes.back()};

    cout << SPACER
         << "Checking templates for relevance...\n";
    if (template_changes.size() > 1)
    {
        // git fetch first commit (older)
        cout << "Fetching version " << template_changes.back().name << ".\n";
        clean_syzkaller(bug);
        git_fetch_and_checkout(bug.get_syzdir(), SYZKALLER_REPO_REMOTE, template_changes.back().name);
        
        // compare each consecutive pair of templates
        cout << "Slimming version " << template_changes.back().name << ".\n";
        string template1 = bug.get_wd() + "/template1.txt", template2 = bug.get_wd() + "/template2.txt";
        string template_dir = template_changes.back().date < Date(2017,9,15) ? bug.get_syzdir() + "/sys" : bug.get_syzdir() + "/sys/linux";
        slim_template(bug.get_repro(), template1, list_template_files(template_dir));
        for (int i = template_changes.size() - 2; i > 0; i--)
        {
            cout << SPACER
                 << "Fetching version " << template_changes.at(i).name << ".\n";
            clean_syzkaller(bug);
            git_fetch_and_checkout(bug.get_syzdir(), SYZKALLER_REPO_REMOTE, template_changes.at(i).name);

            cout << "Slimming version " << template_changes.at(i).name << ".\n";
            template_dir = template_changes.back().date < Date(2017,9,15) ? bug.get_syzdir() + "/sys" : bug.get_syzdir() + "/sys/linux";
            slim_template(bug.get_repro(), template2, list_template_files(template_dir));
            if (!compare_templates(template1, template2))
            {
                cout << "This template differs from the one before.\n";
                relevant_template_changes.insert(relevant_template_changes.begin(), template_changes.at(i));
            }

            move(template2, template1);
        }
        remove_file(template1);
    }
    else
        cout << "No template changes in the date range " << kernel_versions.back().date.get_date() << " to " << kernel_versions.front().date.get_date() << ".\n";

    if (relevant_template_changes.size() > 1) {
        cout << SPACER
             << "Inspecting " << relevant_template_changes.size() - 1 << " template changes.\n";
        logfile << "Inspecting " << relevant_template_changes.size() - 1 << " template changes in [" 
                << kernel_versions.back().date.get_date() << ", " << kernel_versions.front().date.get_date() << "].\n";

        // maybe binary search here
        string template_dir;
        Version linux_version, prev_syzkaller_version;
        Session this_session;
        Syzkaller_Result result_before, result_after;
        for (Version current_version : relevant_template_changes)
        {
            // found_before = fuzz before
            linux_version = get_version_by_date(kernel_versions, current_version.date);
            prev_syzkaller_version = get_version_by_date(relevant_template_changes, current_version.date.dec());

            this_session = Session(linux_version, current_version, prev_syzkaller_version, false);

            session_count++;
            logfile << "Session " << session_count << " before:\n"
                    << "    Template:  " << prev_syzkaller_version.date.get_date() << " - " << prev_syzkaller_version.name << "\n"
                    << "    Syzkaller: " << current_version.date.get_date() << " - " << current_version.name << "\n"
                    << "    Kernel:    " << linux_version.date.get_date() << " - " << linux_version.name << "\n";

            if (!already_fuzzed(fuzz_sessions, this_session))
            {
                cout << SPACER
                    << "Making the kernel\n";
                export_gcc(gcc_versions, current_version.date, inspector);
                prep_kernel(bug, inspector, linux_version, linux_repo_remote);
                clean_gcc(tmp_path);

                // pull the previous template
                cout << SPACER
                    << "Prepping the old template\n";
                prep_syzkaller(bug, inspector, prev_syzkaller_version);

                // now compile the current syzkaller using the old template
                cout << SPACER
                    << "Making Syzkaller\n";
                prep_syzkaller(bug, inspector, current_version, bug.get_wd() + "/my_template.txt");

                result_before = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, current_version.date);

                logfile << "    The bug was " << (result_before.found ? "found in " : "not found and timed out at ") << result_before.ttf << " minutes\n";
                for (string b : result_before.bugsfound)
                    logfile << "        " << b << "\n";

                this_session.found = result_before.found;
                fuzz_sessions.push_back(this_session);
            }
            else
            {
                cout << "This session has already been fuzzed. Skipping.\n";
                result_before.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
                logfile << "The bug was " << (result_before.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n";
            }

            if (result_before.found)
            {
                cout << "The bug can be found before this day. Moving on.\n";
                continue;
            }

            // found_after = fuzz after
            logfile << "Session " << session_count << " after:\n"
                    << "    Template:  " << current_version.date.get_date() << " - " << current_version.name << "\n"
                    << "    Syzkaller: " << current_version.date.get_date() << " - " << current_version.name << "\n"
                    << "    Kernel:    " << linux_version.date.get_date() << " - " << linux_version.name << "\n";

            this_session = Session(linux_version, current_version, current_version, false);

            if (!already_fuzzed(fuzz_sessions, this_session))
            {
                // no need to rebuild the kernel.
                cout << SPACER
                    << "Making Syzkaller\n";
                prep_syzkaller(bug, inspector, current_version);

                result_after = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, current_version.date);

                logfile << "    The bug was " << (result_after.found ? "found in " : "not found and timed out at ") << result_after.ttf << " minutes\n";
                for (string b : result_after.bugsfound)
                    logfile << "        " << b << "\n";

                this_session.found = result_before.found;
                fuzz_sessions.push_back(this_session);
            }
            else
            {
                cout << "This session has already been fuzzed. Skipping.\n";
                result_after.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
                logfile << "The bug was " << (result_after.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n";
            }

            if (!result_before.found && result_after.found)
            {
                cout << SPACER
                     << "Revealing Factor Found!\n"
                     << "Template update from " << current_version.name << " on " << current_version.date.get_date() << " is the reason.\n";
                
                logfile << "\nSuccess\n"
                        << "Revealing factor: Template Update\n"
                        << "Template Version: " << current_version.date.get_date() << " - " << current_version.name << endl;

                // clean up
                return 0;
            }

            if (result_after.found)
                high_date = current_version.date;
            else
                low_date = current_version.date;
        }
    }
    else
        cout << SPACER
             << "No template updates to inspect. Skipping.\n";

    // ======================================================================================================
    // Begin Kernel Inspection

    // r is the starting date. older date (lower). higher index
    // l is the ending date. recent date (higher). lower index
    int r = get_starting_index(kernel_versions, low_date);
    int l = get_ending_index(kernel_versions, high_date);
    int m;

    cout << SPACER
         << "Inspecting " << r - l << " kernel versions.\n";
    logfile << "Inspecting " << r - l << " kernel versions in the range [" << low_date.get_date() << ", " << high_date.get_date() << "].\n";

    Syzkaller_Result result;
    Version linux_version, syzkaller_version, bisect_version;
    Session this_session;
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
                    << "    Kernel:    " << linux_version.date.get_date() << " - " << linux_version.name << "\n";

        if (!already_fuzzed(fuzz_sessions, this_session))
        {
            cout << SPACER
                 << "Making the kernel\n";
            export_gcc(gcc_versions, linux_version.date, inspector);
            prep_kernel(bug, inspector, linux_version, linux_repo_remote);
            clean_gcc(tmp_path);

            cout << SPACER
                 << "Prepping Syzkaller\n";
            prep_syzkaller(bug, inspector, syzkaller_version);

            result = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, syzkaller_version.date);

            logfile << "    The bug was " << (result.found ? "found in " : "not found and timed out at ") << result.ttf << " minutes\n";
            for (string b : result.bugsfound)
                logfile << "        " << b << "\n";

            this_session.found = result.found;
            fuzz_sessions.push_back(this_session);
        }
        else
        {
            cout << "This session has already been fuzzed. Skipping.\n";
            result.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
            logfile << "The bug was " << (result.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n";
        }

        if (result.found)
        {
            l = m;
            bisect_version = linux_version;
        }
        else
            r = m;
    }

    // fuzz before and after the linux version to confirm
    cout << "Checking if the kernel is the revealing factor.\n";
    Syzkaller_Result result_after, result_before;
    syzkaller_version = get_version_by_date(syzkaller_versions, bisect_version.date);
    this_session = Session(bisect_version, syzkaller_version, syzkaller_version, false);

    session_count++;
    logfile << "Session " << session_count << ":\n"
            << "    Template:  " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
            << "    Syzkaller: " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
            << "    Kernel:    " << bisect_version.date.get_date() << " - " << bisect_version.name << "\n";

    if (!already_fuzzed(fuzz_sessions, this_session))
    {
        // fuzz after first
        // check if we need to fetch anything
        if (bisect_version != linux_version)
        {
            cout << SPACER
                << "Making the kernel\n";
            export_gcc(gcc_versions, bisect_version.date, inspector);
            prep_kernel(bug, inspector, bisect_version, linux_repo_remote);
            clean_gcc(tmp_path);

            cout << SPACER
                << "Prepping Syzkaller\n";
            prep_syzkaller(bug, inspector, syzkaller_version);
        }

        result_after = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, syzkaller_version.date);

        logfile << "    The bug was " << (result_after.found ? "found in " : "not found and timed out at ") << result_after.ttf << " minutes\n";
        for (string b : result_after.bugsfound)
            logfile << "        " << b << "\n";

        this_session.found = result_after.found;
        fuzz_sessions.push_back(this_session);
    }
    else
    {
        cout << "This session has already been fuzzed. Skipping.\n";
        result_after.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
        logfile << "The bug was " << (result_after.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n";
    }
    

    // Remember to do corner case range checking in cases like this!!
    linux_version = kernel_versions.at(get_index_by_name(kernel_versions, bisect_version.name) - 1);
    this_session = Session(linux_version, syzkaller_version, syzkaller_version, false);

    session_count++;
    logfile << "Session " << session_count << ":\n"
            << "    Template:  " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
            << "    Syzkaller: " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
            << "    Kernel:    " << linux_version.date.get_date() << " - " << linux_version.name << "\n";
    
    if (!already_fuzzed(fuzz_sessions, this_session))
    {
        cout << SPACER
             << "Making the kernel\n";
        export_gcc(gcc_versions, linux_version.date, inspector);
        prep_kernel(bug, inspector, linux_version, linux_repo_remote);
        clean_gcc(tmp_path);

        result_before = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, syzkaller_version.date);

        logfile << "    The bug was " << (result_before.found ? "found in " : "not found and timed out at ") << result_before.ttf << " minutes\n";
        for (string b : result_before.bugsfound)
            logfile << "        " << b << "\n";

        this_session.found = result_before.found;
        fuzz_sessions.push_back(this_session);
    }
    else
    {
        cout << "This session has already been fuzzed. Skipping.\n";
        result_before.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
        logfile << "The bug was " << (result_before.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n";
    }

    // check the results
    if (result_after.found && !result_before.found)
    {
        cout << SPACER
             << "Revealing Factor Found!\n"
             << "Kernel code change from " << linux_version.name << " on " << linux_version.date.get_date() << " is the reason.\n";
                
        logfile << "\nSuccess\n"
                << "Revealing factor: Kernel Code Change\n"
                << "Kernel Version: " << linux_version.date.get_date() << " - " << linux_version.name << endl;
        
        // clean up
        return 0;
    }
    
    high_date = low_date = bisect_version.date;

    // ======================================================================================================
    // Begin Syzkaller Inspection

    // start with the newest and go back
    r = get_starting_index(kernel_versions, low_date);
    l = get_ending_index(kernel_versions, high_date);

    cout << SPACER
         << "Inspecting " << r - l << " syzkaller versions.\n";
    logfile << "Inspecting " << r - l << " syzkaller versions from " << high_date.get_date() << ".\n";

    // we only need one kernel version
    cout << SPACER
         << "Making the kernel\n";
    export_gcc(gcc_versions, bisect_version.date, inspector);
    prep_kernel(bug, inspector, bisect_version, linux_repo_remote);
    clean_gcc(tmp_path);

    for (int i = l; i >= r; i++)
    {
        syzkaller_version = syzkaller_versions.at(i);
        this_session = Session(bisect_version, syzkaller_version, syzkaller_version, false);

        session_count++;
        logfile << "Session " << session_count << ":\n"
                << "    Template:  " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
                << "    Syzkaller: " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << "\n"
                << "    Kernel:    " << bisect_version.date.get_date() << " - " << bisect_version.name << "\n";

        if (!already_fuzzed(fuzz_sessions, this_session))
        {
            cout << SPACER
                << "Prepping Syzkaller\n";
            prep_syzkaller(bug, inspector, syzkaller_version);

            // run without the poc
            result = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port, syzkaller_version.date, false);

            logfile << "    The bug was " << (result.found ? "found in " : "not found and timed out at ") << result.ttf << " minutes\n";
            for (string b : result.bugsfound)
                logfile << "        " << b << "\n";

            this_session.found = result.found;
            fuzz_sessions.push_back(this_session);
        }
        else
        {
            cout << "This session has already been fuzzed. Skipping.\n";
            result.found = get_result(fuzz_sessions, this_session) == 1 ? true : false;
            logfile << "The bug was " << (result.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n";
        }

        if (!result.found)
        {
            syzkaller_version = syzkaller_versions.at(i - 1);
            break;
        }
    }

    if (get_index_by_name(syzkaller_versions, syzkaller_version.name) < l)
    {
        cout << SPACER
             << "Revealing Factor Found!\n"
             << "Exact Syzkaller commit unknown.\n";
                
        logfile << "\nSuccess\n"
                << "Revealing factor: Syzkaller Update\n"
                << "Syzkaller Version: Unknown.\n";
    }
    else
    {
        cout << SPACER
             << "Revealing Factor Found!\n"
             << "Syzkaller update from " << syzkaller_version.name << " on " << syzkaller_version.date.get_date() << " is the reason.\n";
                
        logfile << "\nSuccess\n"
                << "Revealing factor: Syzkaller Update\n"
                << "Syzkaller Version: " << syzkaller_version.date.get_date() << " - " << syzkaller_version.name << endl;
    }

    // ======================================================================================================
    // Finish
    // ======================================================================================================

    cout << SPACER
        << "Cleaning up...";
    logfile.close();
    git_repository_free(syzkaller_repo);
    git_repository_free(linux_repo);
    git_libgit2_shutdown();
    cout << "Done.\n";
    return 0;
}
