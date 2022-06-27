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
    
    // ======================================================================================================
    // Begin Inspection
    // ======================================================================================================

    /*
    cout << SPACER
        << "Making the kernel\n";
    export_gcc(gcc_versions, Date(2022,6,24), inspector);
    prep_kernel(bug, inspector);
    clean_gcc(tmp_path);

    cout << SPACER;
    prep_syzkaller(bug, inspector);
    //calc_bloat();               // calculate how much bloat is in the slimmed template

    cout << SPACER;
    // don't forget to log everything
    fuzz_loop(bug, inspector, duplicates, max_time, vmc, port);
    */

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
        string template1 = bug.get_wd() + "template1.txt", template2 = bug.get_wd() + "template2.txt";
        string template_dir = template_changes.back().date < Date(2017,9,15) ? bug.get_syzdir() + "/sys" : bug.get_syzdir() + "/sys/linux";
        slim_template(bug.get_repro(), template1, list_template_files(template_dir));
        for (int i = template_changes.size() - 2; i > 0; i--)
        {
            cout << "Fetching version " << template_changes.back().name << ".\n";
            clean_syzkaller(bug);
            git_fetch_and_checkout(bug.get_syzdir(), SYZKALLER_REPO_REMOTE, template_changes.at(i).name);

            cout << "Slimming version " << template_changes.back().name << ".\n";
            template_dir = template_changes.back().date < Date(2017,9,15) ? bug.get_syzdir() + "/sys" : bug.get_syzdir() + "/sys/linux";
            slim_template(bug.get_repro(), template2, list_template_files(template_dir));
            if (!compare_templates(template1, template2))
                relevant_template_changes.insert(relevant_template_changes.begin(), template_changes.at(i));

            move(template2, template1);
        }
    }
    else
        cout << "No template changes in the date range " << kernel_versions.back().date.get_date() << " to " << kernel_versions.front().date.get_date() << ".\n";

    if (relevant_template_changes.size() > 1) {
        cout << "Inspecting " << relevant_template_changes.size() << " template changes.\n";

        string template_dir;
        string slimmed_template = "";
        Version linux_version, prev_syzkaller_version;
        Syzkaller_Result result_before, result_after;
        for (Version current_version : relevant_template_changes)
        {
            // found_before = fuzz before
            linux_version = get_version_by_date(kernel_versions, current_version.date);
            export_gcc(gcc_versions, current_version.date, inspector);
            prep_kernel(bug, inspector, linux_version, linux_repo_remote);
            clean_gcc(tmp_path);

            prev_syzkaller_version = get_version_by_date(relevant_template_changes, current_version.date.dec());
            prep_syzkaller(bug, inspector, prev_syzkaller_version);

            result_before = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port);

            if (result_before.found)
            {
                // log
                continue;
            }

            // found_after = fuzz after
            export_gcc(gcc_versions, current_version.date, inspector);
            prep_kernel(bug, inspector, linux_version, linux_repo_remote);
            clean_gcc(tmp_path);

            prep_syzkaller(bug, inspector, current_version);

            result_after = fuzz_loop(bug, inspector, duplicates, max_time, vmc, port);

            if (!result_before.found && result_after.found)
            {
                // finish logging
                return 0;
            }

            if (result_after.found)
                high_date = current_version.date;
            else
                low_date = current_version.date;
        }
    }
    else
        cout << "No template updates to inspect. Skipping.\n";

    // Begin Kernel Inspection
    // int start_index = get_starting_index();
    // int end_index = get_ending_index();

    // r is the starting date. older. higher index
    // l is the ending date. recent. lower index
    // int r = start_index;
    // int l = end_index;
    // int m = (r + l) / 2;
    // while (l < r) {
        // fetch kernel commit
        // fuzz
        // if (found)
            // l = m
        // else
            // r = m
        // m = (r + l) / 2;
    // }

    // m is the index of the bisected commit
    // MAKE SURE THIS IS THE CASE

    // fetch if needed
    // found_after = fuzz after (chance fetch/build is not needed)
    // fetch before
    // found_before = fuzz before
    // if (found_after && !found_before)
        // finish
    
    // update date range

    // Begin Syzkaller Inspection
    // vector<Version> syzkaller_updates = get_syzkaller_commits();

    // start with the newest and go back
    // for (each syzkaller_update) {
        // fetch syzkaller
        // fuzz
        // if (!found)
            // break
    // }

    // if (first syzkaller commit)
        // too hard to find
    // else
        // report previous syzkaller commit and finish

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