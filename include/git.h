#ifndef GIT_H
#define GIT_H

#include <version.h>
#include <date.h>

#include <string>
#include <vector>

class Git
{
private:
    int err;
    std::string local;      // kernel/
    std::string link;       // https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
    std::string branch;     // master

    int git(const std::vector<std::string> &, const std::string & = "", bool = false);
    std::string git_read(const std::vector<std::string> &);

    int bisect(const std::vector<std::string> &, const std::string & = "");
    std::string bisect_read(const std::vector<std::string> &);

    int create_repo();

public:
    Git(const std::string &, const std::string &, const std::string &);
    int error() const
    { return err; }

    void set_local(const std::string &);
    void set_remote(const std::string &);
    void set_branch(const std::string &);

    int setup_new_local();
    int setup();

    int init();
    int add_remote();
    int pull();
    int fetch(const std::string &);
    int checkout(const std::string &);

    int fetch_and_checkout(const std::string &);

    int reset_hard();
    int clean();
    int cleanup();

    int bisect_start(const std::string &, const std::string &);
    int bisect_good();
    int bisect_bad();
    int bisect_skip();
    int bisect_reset();

    std::string get_url();
    int dump_tags(const std::string &);

    std::string get_current_commit();
    Version get_current_version();
    std::string get_commit_by_date_raw(const Date &);
    Version get_version_by_date_raw(const Date &);

    Date get_commit_date(const std::string &);
    std::string get_commit_name(const std::string &);
    std::string get_tag_name(const std::string &);
    Date get_tag_date(const std::string &);
    std::string get_tag_hash(const std::string &);

    int revlist_topo(const std::string &, const std::string &, const std::string &);
    int revlist(const std::string &, const std::string &, const std::string &);

    bool is_ancestor(const std::string &, const std::string &);
};

#endif
