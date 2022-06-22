#include <date.h>
#include <argparse.h>
#include <bug_info.h>
#include <inspector_config.h>
#include <file_api.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <git2.h>

using namespace std;

const string SYZKALLER_REPO_REMOTE = "https://github.com/google/syzkaller";
const string SYZBOT_FIXED = "https://syzkaller.appspot.com/upstream/fixed";
const string SPACER = "====================================================================================================================================================\n";

int main(int argc, char ** argv)
{
    Argparse args;
    args.expect("sefFmidh");
    args.expect(vector<string>({ "setup-only", "help", "recover" }));
    args.parse(argc, argv);

    int max_time = 720, id;
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

    if (!check_file(bug.get_wd() + "/kernels"))
    {
        cout << "Creating kernels directory.\n";
        make_dir(bug.get_wd() + "/kernels");
    }
    else
        cout << "Found kernels directory.\n";

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

    // Parse Syzbot for duplicate bugs
    cout << SPACER
        << "Gathering bug fixes from Syzbot.\n";

    string tmp_snapshotfile = bug.get_wd() + "/snapshot";
    // vector<> knownfixes;
    // vector<> duplicate_bugs;

    // lynx_dump(https://syzkaller.appspot.com/upstream/fixed, tmp_snapshotfile);
    // lynx -dump -dont_wrap_pre -width=1000 https://syzkaller.appspot.com/upstream/fixed)

    // trim_syzbot_fixes()
    // sed -i '1,/^[ ]*\[[0-9]*\]Title/ d' $snapshotfile
    // sed -i '/^$/q' $snapshotfile

    // parse_syzbot_fixes(tmp_snapshotfile, knownfixes);

    // rm tmp_snapshotfile;

    cout << SPACER;


    cout << "Cleaning up...\n";
    git_repository_free(syzkaller_repo);
    git_libgit2_shutdown();
    return 0;
}