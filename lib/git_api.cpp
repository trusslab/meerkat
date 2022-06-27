#include <git_api.h>
#include <exec_api.h>
#include <shell_api.h>
#include <file_api.h>
#include <bug_info.h>
#include <date.h>
#include <consts.h>

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <string.h>

using namespace std;

Date git_get_commit_date(const string &wd, const string &local_repo, const string &hash)
{
    string outfile = wd + "/tmp_commit_date.txt";
    cd(local_repo);

    // git show -s --date=format:'%Y-%m-%d' --format=%cd hash
    char command[] = "git";
    char arg1[] = "show";
    char arg2[] = "-s";
    char arg3[] = "--date=format:%Y-%m-%d";
    char arg4[] = "--format=%cd";
    char * arg5 = new char[hash.size() + 1];
    strcpy(arg5, hash.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, arg4, arg5, nullptr};
    int ret = exec_and_wait("git", arg_list, outfile);
    if (ret != 0)
    {
        cerr << "Error: git show " << hash << " failed.\n";
        return Date();
    }

    cd(wd);

    Date date;
    ifstream inf;
    inf.open(outfile);
    if(!inf)
    {
        cerr << "Error: Failed to open temp file " << outfile << ".\n";
        return Date();
    }

    // Need to take into account the timezone offset
    string line;
    getline(inf, line);

    delete[] arg5;
    inf.close();
    remove_file(outfile);
    return Date(line);
}

bool git_check_modified_file(const string &wd, const string &local_repo, const string &hash, const string &file)
{
    string outfile = wd + "/tmp_git_check_file.txt";
    cd(local_repo);

    char command[] = "git";
    char arg1[] = "diff-tree";
    char arg2[] = "--name-only";
    char arg3[] = "--no-commit-id";
    char arg4[] = "-r";
    char *arg5 = new char[hash.size() + 1];
    strcpy(arg5, hash.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, arg4, arg5, nullptr};
    int ret = exec_and_wait("git", arg_list, outfile);

    cd(wd);
    delete[] arg5;
    
    bool r = grep_to_find(file, outfile);

    remove_file(outfile);
    return r;
}

int git_rev_list(const string &local_repo, const string &old_hash, const string &new_hash, const string &outfile)
{
    string old_dir = pwd();
    cd(local_repo);
    // git rev-list --ancestry-path old_hash..new_hash
    char command[] = "git";
    char arg1[] = "rev-list";
    char arg2[] = "--ancestry-path";
    string hash_path = old_hash + ".." + new_hash;
    char * arg3 = new char[hash_path.size() + 1];
    strcpy(arg3, hash_path.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, nullptr};
    int ret = exec_and_wait("git", arg_list, outfile);
    if (ret != 0)
    {
        cerr << "Error: git rev-list " << hash_path << " failed.\n";
        return ret;
    }

    cd(old_dir);
    delete[] arg3;
    return 0;
}

vector<Version> get_kernel_versions(const Bug_Info &bug, const string &old_hash, const string &new_hash)
{
    string outfile = bug.get_wd() + "/tmp_kernel_versions.txt";
    
    git_rev_list(bug.get_kerneldir(), old_hash, new_hash, outfile);

    ifstream inf;
    inf.open(outfile);
    if (!inf)
    {
        cerr << "Error: Failed to open temp file " << outfile << ".\n";
        return vector<Version>();
    }

    string line;
    vector<Version> kernel_versions;
    Version v;
    while(getline(inf, line))
    {
        v.name = line;
        v.date = git_get_commit_date(bug.get_wd(), bug.get_kerneldir(), line);
        kernel_versions.push_back(v);
    }

    v.name = old_hash;
    v.date = git_get_commit_date(bug.get_wd(), bug.get_kerneldir(), old_hash);
    kernel_versions.push_back(v);

    inf.close();
    remove_file(outfile);
    return kernel_versions;
}

vector<Version> get_syzkaller_versions(const Bug_Info &bug)
{
    string outfile = bug.get_wd() + "/tmp_template_changes.txt";

    git_rev_list(bug.get_syzdir(), OLDEST_SYZKALLER_HASH, LATEST_SYZKALLER_HASH, outfile);

    ifstream inf;
    inf.open(outfile);
    if (!inf)
    {
        cerr << "Error: Failed to open temp file " << outfile << ".\n";
        return vector<Version>();
    }

    string line;
    vector<Version> syzkaller_versions;
    Version v;
    while(getline(inf, line))
    {
        v.name = line;
        v.date = git_get_commit_date(bug.get_wd(), bug.get_syzdir(), line);
        syzkaller_versions.push_back(v);
    }

    inf.close();
    remove_file(outfile);
    return syzkaller_versions;
}

vector<Version> get_template_changes(const Bug_Info &bug, const Date &old_date, const Date &new_date, const vector<Version> &syz_versions)
{
    vector<Version> template_changes;
    Version save;

    // traverse going back in time
    for (Version v : syz_versions)
    {
        // skip until we get to the date range
        if (v.date > new_date)
            continue;

        // stop once we are before the date range
        if (v.date < old_date)
        {
            save.name = v.name;
            save.date = v.date;
            break;
        }

        // keep only modifications to the linux template
        // older versions used sys/ rather than sys/linux/
        if (git_check_modified_file(bug.get_wd(), bug.get_syzdir(), v.name, "sys/linux") ||
            (v.date < Date(2017,9,15) && git_check_modified_file(bug.get_wd(), bug.get_syzdir(), v.name, "sys")))
        {
            template_changes.push_back(v);
            continue;
        }
    }

    // keep the latest template change before the date range
    template_changes.push_back(save);

    return template_changes;
}

int git_fetch_and_checkout(const string &local_repo, const string &repo, const string &hash)
{
    string old_dir = pwd();
    cd(local_repo);
    // git fetch repo hash
    char command[] = "git";
    char arg1[] = "fetch";
    char * arg2 = new char[repo.size() + 1];
    strcpy(arg2, repo.c_str());
    char * arg3 = new char[hash.size() + 1];
    strcpy(arg3, hash.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, nullptr};

    int ret = exec_and_wait("git", arg_list);
    if (ret != 0)
    {
        cerr << "Error: fetch " << repo << " " << hash << " failed.\n";
        cd(old_dir);
        return ret;
    }

    // git checkout -f FETCH_HEAD
    char arg4[] = "checkout";
    char arg5[] = "-f";
    char arg6[] = "FETCH_HEAD";

    char * arg_list2[] = {command, arg4, arg5, arg6, nullptr};

    ret = exec_and_wait("git", arg_list2);
    if (ret != 0)
    {
        cerr << "Error: checkout FETCH_HEAD failed.\n";
        cd(old_dir);
        return ret;
    }

    cd(old_dir);
    delete[] arg2;
    delete[] arg3;
    return 0;
}