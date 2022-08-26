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
#include <unordered_map>

#include <string.h>

using namespace std;

int set_timezone(const string &tz)
{
    string env = "TZ=" + tz;
    return (export_env(env) == 0 ? 0 : -1);
}

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
        cerr << "Error: git show " << hash << " failed.\n" << flush;
        return Date();
    }

    cd(wd);

    Date date;
    ifstream inf;
    inf.open(outfile);
    if(!inf)
    {
        cerr << "Error: Failed to open temp file " << outfile << ".\n" << flush;
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

vector<string> git_get_parents(const string &local_repo, const string &hash)
{
    string wd = pwd();
    cd(local_repo);

    char command[] = "git";
    char arg1[] = "show";
    char arg2[] = "-s";
    char arg3[] = "--format=%P";
    char *arg4 = new char[hash.size() + 1];
    strcpy(arg4, hash.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, arg4, nullptr};
    string ret = exec_and_read("git", arg_list);

    if (ret == "")
    {
        cerr << "Error: git show parents " << hash << " failed.\n" << flush;
        exit(-1);
    }

    cd(wd);
    delete[] arg4;

    vector<string> parents;
    int pos0 = 0, pos1 = ret.find_first_of(" \n");
    while (pos1 != string::npos)
    {
        parents.push_back(ret.substr(pos0, pos1 - pos0));
        pos0 = pos1 + 1;
        pos1 = ret.find_first_of(" \n", pos0);
    }

    return parents;
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
    // git rev-list --ancestry-path --topo-order --date=format:'%Y-%m-%d' --format='%cd %P' old_hash..new_hash
    char command[] = "git";
    char arg1[] = "rev-list";
    char arg2[] = "--ancestry-path";
    char arg3[] = "--date=format-local:%Y-%m-%d";
    char arg4[] = "--format=%cd";
    string hash_path = old_hash + ".." + new_hash;
    char * arg5 = new char[hash_path.size() + 1];
    strcpy(arg5, hash_path.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, arg4, arg5, nullptr};
    int ret = exec_and_wait("git", arg_list, outfile);
    if (ret != 0)
        cerr << "Error: git rev-list " << hash_path << " failed.\n" << flush;

    cd(old_dir);
    delete[] arg5;
    return (ret != 0 ? -1 : 0);
}


int git_rev_list_topo(const string &local_repo, const string &old_hash, const string &new_hash, const string &outfile)
{
    string old_dir = pwd();
    cd(local_repo);
    // git rev-list --ancestry-path --topo-order --date=format:'%Y-%m-%d' --format='%cd %P' old_hash..new_hash
    char command[] = "git";
    char arg1[] = "rev-list";
    char arg2[] = "--ancestry-path";
    char arg3[] = "--topo-order";
    char arg4[] = "--date=format-local:%Y-%m-%d";
    char arg5[] = "--format=%cd %P";
    string hash_path = old_hash + ".." + new_hash;
    char * arg6 = new char[hash_path.size() + 1];
    strcpy(arg6, hash_path.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, arg4, arg5, arg6, nullptr};
    int ret = exec_and_wait("git", arg_list, outfile);
    if (ret != 0)
        cerr << "Error: git rev-list " << hash_path << " failed.\n" << flush;

    cd(old_dir);
    delete[] arg6;
    return (ret != 0 ? -1 : 0);
}

vector<Version> get_kernel_versions(const Bug_Info &bug, const string &old_hash, const string &new_hash)
{
    string outfile = bug.get_wd() + "/tmp_kernel_versions.txt";
    
    cout << "Listing...\n";
    if (git_rev_list_topo(bug.get_kerneldir(), old_hash, new_hash, outfile) < 0)
    {
        return vector<Version>();
    }

    ifstream inf;
    inf.open(outfile);
    if (!inf)
    {
        cerr << "Error: Failed to open temp file " << outfile << ".\n" << flush;
        return vector<Version>();
    }

    string line;
    unordered_map<string, Version_p> kernel_versions_raw;
    vector<Version> kernel_versions;
    Version_p vp;
    int pos0 = 0, pos1 = 0, count = 0;
    cout << "Reading...\n";
    while(getline(inf, line))
    {
        vp.parents.clear();
        vp.v.name = line.substr(7);
        getline(inf, line);
        pos0 = 0;
        pos1 = line.find_first_of(" ");
        vp.v.date = Date(line.substr(pos0, pos1 - pos0));

        pos0 = pos1 + 1;
        pos1 = line.find_first_of(" ", pos0);
        while (pos1 != string::npos)
        {
            vp.parents.push_back(line.substr(pos0, pos1 - pos0));
            pos0 = pos1 + 1;
            pos1 = line.find_first_of(" ", pos0);
        }
        vp.parents.push_back(line.substr(pos0));

        kernel_versions_raw.insert(pair<string,Version_p>(vp.v.name, vp));
    }

    vp.v.name = old_hash;
    vp.v.date = git_get_commit_date(bug.get_wd(), bug.get_kerneldir(), old_hash);
    kernel_versions_raw.insert(pair<string,Version_p>(vp.v.name, vp));

    inf.close();
    remove_file(outfile);

    // now draw a single line using first-parent (except for the merge including the guilty commit)
    string cur_hash = new_hash;
    cout << "Parsing...\n";
    kernel_versions.push_back(kernel_versions_raw.at(cur_hash).v);
    while (kernel_versions.back().name != old_hash)
    {
        // starting with the first parent, if it exists, push back
        for (int j = 0; j < kernel_versions_raw.at(cur_hash).parents.size(); j++)
        {
            if (kernel_versions_raw.find(kernel_versions_raw.at(cur_hash).parents.at(j)) != kernel_versions_raw.end())
            {
                kernel_versions.push_back(kernel_versions_raw.at(kernel_versions_raw.at(cur_hash).parents.at(j)).v);
                cur_hash = kernel_versions_raw.at(cur_hash).parents.at(j);
                break;
            }
        }
    }

    return kernel_versions;
}

vector<Version> get_syzkaller_versions(const Bug_Info &bug)
{
    int k;
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
        v.name = line.substr(7);
        getline(inf, line);
        v.date = Date(line);
        syzkaller_versions.push_back(v);
    }

    v.name = OLDEST_SYZKALLER_HASH;
    v.date = git_get_commit_date(bug.get_wd(), bug.get_syzdir(), OLDEST_SYZKALLER_HASH);
    syzkaller_versions.push_back(v);

    // remove bad versions of syzkaller
    for (string h : SYZKALLER_BROKEN_VERSONS)
    {
        k = get_index_by_name(syzkaller_versions, h);
        if (k >= 0)
            syzkaller_versions.erase(syzkaller_versions.begin() + k);
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
            // keep one commit before the range
            template_changes.push_back(v);
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

    return template_changes;
}

vector<string> git_show_commits_merged(const string &repo, const string &mergehash)
{
    int pos0, pos1;
    vector<string> commits;
    string old_dir = pwd();
    cd(repo);

    // git log --format=%H hash^..hash
    char command[] = "git";
    char arg1[] = "log";
    char arg2[] = "--format=%H";
    string range = mergehash + "^.." + mergehash;
    char * arg3 = new char[range.size() + 1];
    strcpy(arg3, range.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, nullptr};

    string ret = exec_and_read("git", arg_list);

    delete[] arg3;
    cd(old_dir);

    if (!ret.empty())
    {
        pos0 = 0;
        pos1 = ret.find("\n");
        while (pos1 != string::npos)
        {
            commits.push_back(ret.substr(pos0, pos1 - pos0));
            pos0 = pos1 + 1;
            pos1 = ret.find("\n", pos0);
        }
        commits.push_back(ret.substr(pos0));
    }

    return commits;
}

Version git_find_merge_commit(const string &repo, const vector<Version> &commits, const string &hash_to_find)
{
    vector<string> commits_merged;
    // search linearly for the merge commit. Max out at 10,000 commits
    for (int i = commits.size() - 1; i >= 0 && commits.size() - i <= 10,000; i--)
    {
        if (commits.at(i).name == hash_to_find)
            continue;

        commits_merged.clear();
        commits_merged = git_show_commits_merged(repo, commits.at(i).name);
        for (string c : commits_merged)
            if (hash_to_find == c)
                return commits.at(i);
    }

    return Version("", Date(0,0,0));
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

    int err = exec_and_wait("git", arg_list);
    delete[] arg2;
    delete[] arg3;
    if (err != 0)
    {
        cerr << "Error: fetch " << repo << " " << hash << " failed.\n";
        cd(old_dir);
        return -1;
    }

    // git checkout -f FETCH_HEAD
    char arg4[] = "checkout";
    char arg5[] = "-f";
    char arg6[] = "FETCH_HEAD";

    char * arg_list2[] = {command, arg4, arg5, arg6, nullptr};

    err = exec_and_wait("git", arg_list2);
    if (err != 0)
        cerr << "Error: checkout FETCH_HEAD failed.\n";

    cd(old_dir);
    return (err != 0 ? -1 : 0);
}

string get_commit_name(const string &repo, const string &hash)
{
    int index;
    string old_dir = pwd();
    cd(repo);
    char command[] = "git";
    char arg1[] = "show";
    char arg2[] = "-s";
    char arg3[] = "--format=%s";
    char * arg4 = new char[hash.size() + 1];
    strcpy(arg4, hash.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, arg4, nullptr};

    string ret = exec_and_read("git", arg_list);
    if (ret == "")
        cerr << "Warning: Failed to read name for commit " << hash << ".\n";

    delete[] arg4;
    cd(old_dir);
    
    index = ret.find("\n");
    if (index != string::npos)
        ret = ret.substr(0, index);

    return ret;
}