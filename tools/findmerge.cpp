#include <date.h>
#include <argparse.h>
#include <bug_info.h>
#include <file_api.h>
#include <consts.h>
#include <git_api.h>
#include <version.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <git2.h>

using namespace std;

int main(int argc, char ** argv)
{
    int id, err = 0, k;
    string find_hash, guilty_hash, 
           linux_repo_remote, logfilename,
           kernel_commit_name = "";
    Bug_Info bug;
    Argparse args;
    Version merge_commit;
    git_repository *linux_repo = nullptr;
    int git_err;

    ofstream logfile;
    vector<Version> kernel_versions;

    args.expect("FGih");
    args.expect(vector<string>({ "help" }));
    args.parse(argc, argv);
    if (args.is_set('h') || args.is_set("help"))
    {
        cout << "Help:\n"
            << "    -F [find_hash]: the hash of the finding commit.\n"
            << "    -G [guilty_hash]: the hash of the guilty commit.\n"
            << "    -i [id]: REQUIRED. The id of the inspector.\n"
            << endl;
        return 0;
    }

    git_libgit2_init();

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

    if (args.is_set('i'))
        id = args.get_arg_as_int('i');
    else
    {
        cout << "Error: No id given. Please use -i [id]\n";
        err = -1;
        goto finish;
    }

    // get information about the bug
    bug.parse_config_file("../wd-inspector-" + to_string(id) + "/" + "bug.cfg");

    if (!check_file(bug.wd + "/log"))
        make_dir(bug.wd + "/log");

    // Set up linux repository locally
    linux_repo_remote = LINUX_REPO_REMOTE + bug.repository;

    if (!check_file(bug.kerneldir))
        make_dir(bug.kerneldir);

    // this may cause issues when the repository opened is not the one we want
    git_err = git_repository_open(&linux_repo, bug.kerneldir.c_str());
    if (git_err < 0)
    {
        cout << "Cloning Linux repository...\n";
        git_err = git_clone(&linux_repo, linux_repo_remote.c_str(), bug.kerneldir.c_str(), nullptr);
        if (git_err < 0)
        {
            cerr << "Error: Git clone failed.\n";
            err = -1;
            goto finish;
        }
    }

    // begin logging
    logfilename = bug.wd + "/log/merges.log";
    logfile.open(logfilename, ios_base::app);
    if(!logfile)
    {
        cerr << "Error: Failed to open log file " << logfilename << ".\n";
        err = -1;
        goto finish;
    }

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
    
    //  ======================================================================================================
    // Find Merge Commit

    cout << SPACER
         << "Looking for Merge Commit...\n";

    merge_commit = git_find_merge_commit(bug.kerneldir, kernel_versions, guilty_hash);

    if (!merge_commit.name.empty())
    {
        cout << "Merge commit found: " << merge_commit.name << ".\n";
        logfile << "Bug " << bug.number << ": " << merge_commit.date.get_date() << " - " << merge_commit.name << ".\n" << flush;
    }
    else
    {
        cout << "No merge commit found.\n";
        logfile << "Bug " << bug.number << ": No Merge Commit.\n" << flush;
    }

    // ======================================================================================================
    // Finish
    // ======================================================================================================

finish:
    if (logfile)
    {
        logfile << flush;
        logfile.close();
    }

    if (linux_repo)
        git_repository_free(linux_repo);
    
    git_libgit2_shutdown();
    cout << "Done.\n" << flush;
    return err;
}