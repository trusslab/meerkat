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

vector<string> git_show_commits_merged(const string &repo, const string &mergehash)
{
    int pos0, pos1;
    vector<string> commits;
    string old_dir = pwd();
    cd(repo);

    // git log --format=%H hash^..hash
    char command[] = "git";
    char arg1[] = "log";
    char arg2[] = "-100";
    char arg3[] = "--format=%H";
    string range = mergehash + "^.." + mergehash;
    char * arg4 = new char[range.size() + 1];
    strcpy(arg4, range.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, arg4, nullptr};

    string ret = exec_and_read("git", arg_list);

    delete[] arg4;
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

int git_init()
{
    char command[] = "git";
    char arg1[] = "init";
    char * arg_list[] = {command, arg1, nullptr};
    return (exec_and_wait("git", arg_list) != 0 ? -1 : 0);
}

int git_pull(const string &remote)
{
    char command[] = "git";
    char arg1[] = "pull";
    char * arg2 = new char[remote.size() + 1];
    strcpy(arg2, remote.c_str());

    char * arg_list[] = {command, arg1, arg2, nullptr};
    int err = exec_and_wait("git", arg_list);

    delete[] arg2;
    return (err != 0 ? -1 : 0);
}

int git_clone(const string &remote, const string &local_dir)
{
    int err;
    string old_dir = pwd();
    cd(local_dir);

    git_init();
    err = git_pull(remote);

    cd(old_dir);
    return err;
}

int git_fetch(const string &repo, const string &hash)
{
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
    return (err != 0 ? -1 : 0);
}

int git_checkout(const string &branch)
{
    char command[] = "git";
    char arg1[] = "checkout";
    char arg2[] = "-f";
    char * arg3 = new char[branch.size() + 1];
    strcpy(arg3, branch.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, nullptr};

    int err =  exec_and_wait("git", arg_list);
    delete[] arg3;
    return (err != 0 ? -1 : 0);
}

int git_fetch_and_checkout(const string &local_repo, const string &repo, const string &hash)
{
    string old_dir = pwd();
    cd(local_repo);

    if (git_fetch(repo, hash) != 0)
    {
        cerr << "Error: fetch " << repo << " " << hash << " failed.\n";
        cd(old_dir);
        return -1;
    }
    
    if (git_checkout("FETCH_HEAD") != 0)
    {
        cerr << "Error: checkout FETCH_HEAD failed.\n";
        cd(old_dir);
        return -1;
    }

    cd(old_dir);
    return 0;
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

// Returns the full hash of the current commit. repo is the local directory
// where the repository is (i.e. /path/to/syzkaller)
string get_current_commit_hash(const string &repo)
{
    int index;
    string old_dir = pwd();
    cd(repo);
    char command[] = "git";
    char arg1[] = "show";
    char arg2[] = "-s";
    char arg3[] = "--format=%H";

    char * arg_list[] = {command, arg1, arg2, arg3, nullptr};

    string ret = exec_and_read("git", arg_list);
    if (ret == "")
        cerr << "Warning: Failed to read hash in " << repo << ".\n";

    cd(old_dir);
    
    index = ret.find("\n");
    if (index != string::npos)
        ret = ret.substr(0, index);

    return ret;
}
