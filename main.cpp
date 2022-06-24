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
    args.expect("sefFmidh");
    args.expect(vector<string>({ "setup-only", "help", "recover" }));
    args.parse(argc, argv);

    int max_time = 2, id;
    Port_Info port;
    port.start_port = 12000;
    port.port_count = 0;
    port.port = 0;

    Date start_date, end_date, find_date;
    string find_hash;

    if (args.is_set('h') || args.is_set("help"))
    {
        cout << "Help:\n"
            << "Short Ticks:\n"
            << "    -s [start_date]: the date to start inspecting from (usually guilty date).\n"
            << "    -e [end_date]: the date to stop inspecting on (usually finding date).\n"
            << "    -f [find_date]: the date the bug is known to be found on.\n"
            << "    -F [find_hash]: the hash of the finding commit.\n"
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

    /*
    For now:
    Get fuzzing to work
        all helpers needed
        all makes
        template slimming
    */

    //gcc_versions = grab_gcc_versions();        // parse the gccVersions file

    //export_gcc(gcc_versions, kernel_date);                  // set environment variable for correct gcc
    cout << SPACER
        << "Making the kernel\n";
    prep_kernel(bug, inspector);
    //clean_gcc();                // remove gcc from path

    cout << SPACER
        << "Making Syzkaller.\n";
    prep_syzkaller(bug, inspector);
    //slim_template();            // slim the template
    //calc_bloat();               // calculate how much bloat is in the slimmed template

    cout << SPACER;
    fuzz_loop(bug, inspector, duplicates, max_time, vmc, port);

    /*
    Inspect template changes
        gather all template changes
        fuzz before and after each
        collect new range
    Bisect on kernel changes
        daily for now
        use git bisect (or at least same strat)
        use syzkaller for the same day as commit
        once commit is decided, fuzz before and after with everything else constant
    Inspect Syzkaller (only if kernel is inconclusive)
        long fuzz time
        only need to fuzz once
    */
    

    cout << SPACER
        << "Cleaning up...";
    logfile.close();
    git_repository_free(syzkaller_repo);
    git_libgit2_shutdown();
    cout << "Done.\n";
    return 0;
}