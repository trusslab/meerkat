#include <date.h>
#include <exec_api.h>
#include <file_api.h>
#include <git.h>
#include <my_string.h>
#include <shell_api.h>
#include <version.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

#include <string.h>

int Git::git(const std::vector<std::string> &args, const std::string &outfile, bool quiet)
{
    std::string old_dir = pwd();
    cd (local);
    std::vector<std::string> cmd = {"git"};
    
    for (std::string o : args)
        if (!o.empty())
            cmd.push_back(o);
    
    const char ** arg_list = new const char*[cmd.size()+1];
    for (int i = 0; i < cmd.size(); i++)
        arg_list[i] = cmd.at(i).c_str();

    arg_list[cmd.size()] = nullptr;

    err = exec_and_wait("git", (char **)arg_list, outfile, outfile, quiet);
    if (!quiet && err != 0)
        std::cerr << "Warning: git exited with error status 0x" << std::hex << err << std::endl << std::dec << std::flush;

    delete[] arg_list;
    cd(old_dir);
    return err;
}

std::string Git::git_read(const std::vector<std::string> &args)
{
    std::string old_dir = pwd();
    cd(local);
    std::vector<std::string> cmd = {"git"};
    
    for (std::string o : args)
        if (!o.empty())
            cmd.push_back(o);
    
    const char ** arg_list = new const char*[cmd.size()+1];
    for (int i = 0; i < cmd.size(); i++)
        arg_list[i] = cmd.at(i).c_str();

    arg_list[cmd.size()] = nullptr;

    std::string ret = exec_and_read("git", (char **)arg_list);

    delete[] arg_list;
    cd(old_dir);
    return ret;
}

int Git::bisect(const std::vector<std::string> &args, const std::string & outfile)
{
    std::vector<std::string> newargs({"bisect"});
    for (std::string a : args)
        newargs.push_back(a);
    
    return git(newargs, outfile) == 0 ? 0 : -1;
}

std::string Git::bisect_read(const std::vector<std::string> &args)
{
    std::vector<std::string> newargs({"bisect"});
    for (std::string a : args)
        newargs.push_back(a);
    
    return chomp(git_read(newargs));
}

int Git::create_repo()
{
    if (!check_file(local))
        err = make_dir(local);
    return err;
}

Git::Git(const std::string &local_repo, const std::string &remote, const std::string &br)
{
    err = 0;
    local = local_repo;
    link = remote;
    branch = br;
    setup();
}

void Git::set_local(const std::string &local_repo)
{
    if (local_repo.empty())
        return;
    local = local_repo;
}

void Git::set_remote(const std::string &remote)
{
    if (remote.empty())
        return;
    link = remote;
}

void Git::set_branch(const std::string &br)
{
    if (br.empty())
        return;
    branch = br;
}

int Git::setup_new_local()
{
    if (create_repo() < 0)
    {
        std::cerr << "Failed to create local directory: " << local << "\n" << std::flush;
        return -1;
    }
    if (init() < 0)
    {
        std::cerr << "Failed to init local git repository: " << local << "\n" << std::flush;
        return -1;
    }
    if (add_remote() < 0)
    {
        std::cerr << "Failed to add remote repository: " << link << "\n" << std::flush;
        return -1;
    }
    if (pull() < 0)
    {
        std::cerr << "Failed to pull from remote: " << link << "\n" << std::flush;
        return -1;
    }
    return 0;
}

int Git::setup()
{
    if (!check_file(local))
        return setup_new_local();
    
    if (!check_file(local + ".git"))
    {
        remove_dir(local);
        return setup_new_local();
    }

    if (get_url() != link)
    {
        git({"remote", "remove", "origin"}, "/dev/null");
        err = add_remote();
    }

    cleanup();
    return pull();
}

int Git::init()
{
    err = git({"init"}, "/dev/null") == 0 ? 0 : -1;
    return err;
}

int Git::add_remote()
{
    err = git({"remote", "add", "origin", link}, "/dev/null") == 0 ? 0 : -1;
    return err;
}

// git pull --force --tags origin branch
int Git::pull()
{
    err = git({"pull", "--force", "--tags", "origin", branch}, "/dev/null") == 0 ? 0 : -1;
    return err;
}

// git fetch --force origin hash
int Git::fetch(const std::string &hash)
{
    err = git({"fetch", "--force", "origin", hash}, "/dev/null") == 0 ? 0 : -1;
    return err;
}

// git checkout --force commit
int Git::checkout(const std::string &commit)
{
    err = git({"checkout", "--force", commit}, "/dev/null") == 0 ? 0 : -1;
    return err;
}

int Git::fetch_and_checkout(const std::string &hash)
{
    err = fetch(hash);
    if (err != 0)
    {
        std::cerr << "Error: Failed to fetch " << hash << " from " << link << "\n" << std::flush;
        return -1;
    }
    
    err = checkout("FETCH_HEAD");
    if (err != 0)
    {
        std::cerr << "Error: Failed to checkout FETCH_HEAD\n" << std::flush;
        return -1;
    }
    return 0;
}

int Git::reset_hard()
{
    err = git({"reset", "--hard"}, "/dev/null") == 0 ? 0 : -1;
    return err;
}

int Git::clean()
{
    err = git({"clean", "-fdx"}, "/dev/null") == 0 ? 0 : -1;
    return err;
}

int Git::cleanup()
{
    if (reset_hard() < 0)
    {
        std::cerr << "Failed to git reset during cleanup\n" << std::flush;
        return -1;
    }
    if (clean() < 0)
    {
        std::cerr << "Failed to git clean during cleanup\n" << std::flush;
        return -1;
    }
    return 0;
}

// git bisect start bad good
// returns the number of remaining commits
int Git::bisect_start(const std::string &bad, const std::string &good)
{
    int rem = -1;
    std::string output = bisect_read({"start", bad, good});
    std::vector<std::string> spl = split(output, ' ');
    for (int i = 0; i < spl.size(); i++)
    {
        if (spl.at(i).find("Bisecting:") != std::string::npos && i + 1 < spl.size())
        {
            rem = stoi(spl.at(i + 1));
            break;
        }
    }
    
    return rem;
}

// returns number of remaining commits, -1 on error, -2 on stop, -3 on multiple results
int Git::handle_bisect(const std::string &output)
{
    int rem = -1;
    std::vector<std::string> spl = split(output, ' ');

    for (int i = 0; i < spl.size(); i++)
    {
        if (spl.at(i).find("Bisecting:") != std::string::npos && i + 1 < spl.size())
        {
            rem = stoi(spl.at(i + 1));
            break;
        }
    }
    if (rem == -1 && spl.size() > 1 && output.find("is the first bad commit") != std::string::npos)
    {
        rem = -2;
    }
    else if (rem == -1 && output.find("There are only 'skip'ped commits") != std::string::npos)
    {
        rem = -3;
    }
    else if (rem == -1)
        err = -1;
    
    return rem;
}

// git bisect good
int Git::bisect_good()
{
    std::string output = bisect_read({"good"});
    return handle_bisect(output);
}

int Git::bisect_bad()
{
    std::string output = bisect_read({"bad"});
    return handle_bisect(output);
}

int Git::bisect_skip()
{
    std::string output = bisect_read({"skip"});
    return handle_bisect(output);
}

int Git::bisect_remaining(const std::string &bad, const std::string &good)
{
    std::string ret = git_read({"rev-list", "--count", good + ".." + bad});
    int count = std::stoi(ret);
    if (count >= 0)
        return count + 1;
    
    return -1;
}

int Git::bisect_reset()
{
    err = bisect({"reset"}, "/dev/null");
    return err;
}

std::string Git::get_url()
{
    std::string ret = git_read({"remote", "get-url", "origin"});
    if (ret.empty())
        err = -1;
    return chomp(ret);
}


 // git tag -l v[0-9].[0-9] v[0-9].[0-9][0-9] --sort=-taggerdate
int Git::dump_tags(const std::string &filename)
{
    // globs are the worst
    err = git({"tag", "-l", "v[0-9].[0-9]", "v[0-9].[0-9][0-9]", "--sort=-taggerdate"}, filename) == 0 ? 0 : -1;
    return err;
}

 // git tag -l --no-contains hash --merged hash v[0-9].[0-9] v[0-9].[0-9][0-9] --sort=-taggerdate
int Git::dump_commit_past_tags(const std::string &commit, const std::string &filename)
{
    // globs are the worst
    err = git({"tag", "-l", "--no-contains", commit, "--merged", commit, "v[0-9].[0-9]", "v[0-9].[0-9][0-9]", "--sort=-taggerdate"}, filename) == 0 ? 0 : -1;
    return err;
}

std::string Git::commit_tag(const std::string &commit)
{
    return git_read({"tag", "-l", "--points-at", commit, "--merged", commit, "v[0-9].[0-9]", "v[0-9].[0-9][0-9]", "--sort=-taggerdate"});
}

std::string Git::get_current_commit()
{
    std::string ret = git_read({"show", "-s", "--format=%H"});
    if (ret.empty())
        err = -1;
    return chomp(ret);
}

// git show -s --date=format:%Y-%m-%d --format='%H %cd'
Version Git::get_current_version()
{
    std::string ret = git_read({"show", "-s", "--date=format:%Y-%m-%d", "--format=%H %cd"});
    if (ret.empty())
        err = -1;
    std::vector<std::string> spl = split(ret, ' ');
    if (spl.size() != 2)
    {
        err = -1;
        return Version();
    }
    return Version(spl.at(0), spl.at(1));
}

// git log -1 -s --until='date' --date=format:%Y-%m-%d --format=%H 
std::string Git::get_commit_by_date_raw(const Date &date)
{
    std::string ret = git_read({"log", "-1", "--until="+date.get_date(), "-s", "--format=%H"});
    if (ret.empty())
        err = -1;
    return chomp(ret);
}

// git log -1 -s --until='date' --date=format:%Y-%m-%d --format='%H %cd'
Version Git::get_version_by_date_raw(const Date &date)
{
    Version vret;
    std::string ret = git_read({"log", "-1", "--until="+date.get_date(), "-s", "--date=format:%Y-%m-%d", "--format=%H %cd"});
    if (ret.empty())
    {
        err = -1;
        return vret;
    }
    std::vector<std::string> spl = split(ret, ' ');
    if (spl.size() != 2)
    {
        err = -1;
        return vret;
    }
    vret.id = spl.at(0);
    vret.date = Date(spl.at(1));
    return vret;
}

// git show -s --date=format:%Y-%m-%d --format=%cd hash
Date Git::get_commit_date(const std::string &hash)
{
    std::string ret = git_read({"show", "-s", "--date=format:%Y-%m-%d", "--format=%cd", hash});
    if (ret.empty())
    {
        std::cerr << "Failed to read commit date: " << hash << "\n" << std::flush;
        err = -1;
        return Date();
    }
    return Date(ret);
}

// git show -s --format=%s hash
std::string Git::get_commit_name(const std::string &hash)
{
    std::string ret = git_read({"show", "-s", "--format=%s", hash});
    if (ret.empty())
    {
        std::cerr << "Failed to read commit name: " << hash << "\n" << std::flush;
        err = -1;
    }
    return chomp(ret);
}

// git log -1 --format=%s tag
std::string Git::get_tag_name(const std::string &tag)
{
    std::string ret = git_read({"log", "-1", "--format=%s", tag});
    if (ret.empty())
    {
        std::cerr << "Failed to read commit name: " << tag << "\n" << std::flush;
        err = -1;
    }
    return chomp(ret);
}

// git log -1 --date=format:%Y-%m-%d --format=%cd tag
Date Git::get_tag_date(const std::string &tag)
{
    std::string ret = git_read({"log", "-1", "--date=format:%Y-%m-%d", "--format=%cd", tag});
    if (ret.empty())
    {
        std::cerr << "Failed to read commit date: " << tag << "\n" << std::flush;
        err = -1;
        return Date();
    }
    return Date(ret);
}

// git log -1 --format=%H tag
std::string Git::get_tag_hash(const std::string &tag)
{
    std::string ret = git_read({"log", "-1", "--format=%H", tag});
    if (ret.empty())
    {
        std::cerr << "Failed to read tag hash: " << tag << "\n" << std::flush;
        err = -1;
    }
    return chomp(ret);
}

// git rev-list --ancestry-path --topo-order --date=format:'%Y-%m-%d' --format='%cd %P' old_hash..new_hash
int Git::revlist_topo(const std::string &old_hash, const std::string &new_hash, const std::string &outfile)
{
    err = git({"rev-list", "--ancestry-path", "--topo-order", "--date=format-local:%Y-%m-%d", "--format=%cd %P", old_hash+".."+new_hash}, outfile);
    if (err != 0)
    {
        std::cerr << "Error: git rev-list " << old_hash+".."+new_hash << " failed.\n" << std::flush;
        err = -1;
    }

    return err;
}

// git rev-list --ancestry-path --topo-order --date=format:'%Y-%m-%d' --format='%cd %P' old_hash..new_hash
int Git::revlist(const std::string &old_hash, const std::string &new_hash, const std::string &outfile)
{
    err = git({"rev-list", "--ancestry-path", "--date=format-local:%Y-%m-%d", "--format=%cd %P", old_hash+".."+new_hash}, outfile) == 0 ? 0 : -1;
    if (err != 0)
        std::cerr << "Error: git rev-list " << old_hash+".."+new_hash << " failed.\n" << std::flush;
    return err;
}

std::string Git::get_first_parent(const std::string &hash)
{
    // git show -s --oneline --format=%P
    std::string res = git_read({"show", "-s", "--oneline", "--format=%P", hash});
    res = split(res, ' ').at(0);
    res = split(res, '\n').at(0);
    chomp(res);
    return res;
}

bool Git::is_ancestor(const std::string &child, const std::string &maybe_parent)
{
    err = git({"merge-base", child, "--is-ancestor", maybe_parent}, "/dev/null", true);
    return (err == 1);
}
