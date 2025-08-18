#include <git.h>
#include <version.h>

#include <iostream>
#include <string>
#include <vector>

using namespace std;

int main(void)
{
    string local = "/mnt/sdd/jtbursey/SyzInspector/wd-meerkat-1/kernel/";
    string remote = "https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git";
    string branch = "master";
    Git git(local, remote, branch);
    cout << "Created local repository\n" << flush;

    cout << "Current Commit: " << git.get_current_commit() << endl << flush;
    git.checkout("master");
    cout << "Current Commit: " << git.get_current_commit() << endl << flush;
    cout << "Current Date: " << git.get_commit_date("").get_date() << endl << flush;
    //cout << "v6.1 Date: " << git.get_tag_date("v6.1").get_date() << endl << flush;
    //cout << "2017-02-12 Commit: " << git.get_commit_by_date_raw(Date(2017, 2, 12)) << endl << flush;
    Version version = git.get_version_by_date_raw(Date(2020, 10, 6));
    cout << version.id << " - " << version.date.get_date() << endl << flush;
    return 0;
}
