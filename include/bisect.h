#ifndef BISECT_H
#define BISECT_H

#include <bug_info.h>
#include <date.h>
#include <environment.h>
#include <git.h>
#include <result.h>
#include <session.h>
#include <version.h>

#include <fstream>
#include <vector>
#include <set>

enum Bisect_Algorithm {ALG_FF_STATEFUL, ALG_FF_CLEAN, ALG_BISECT_FF, ALG_SYZ_BISECT, ALG_SETUP, ALG_FINDING};
enum Bisect_Phase {Bisect_Init, Bisect_Finding, Bisect_Releases, Bisect_Syzkaller, Bisect_Kernel, Bisect_Done};

class Bisect
{
private:
    Bisect_Algorithm alg;
    Bisect_Phase phase;

    unsigned int session_count;
    Session current_session;
    Session last_session;
    Session bisect_session;
    Session good_session;
    std::set<Session> past_sessions;

    Version finding_version;
    Version locked_syzkaller;

    bool lock_syz;
    int index;
    int bisect_index;

    // right is the older date (lower date). higher index
    // left is the recent date (higher date). lower index
    int left;
    int right;
    int middle;
    Date find_date;

    int git_remaining;
    bool git_stop;

    std::vector<Version> gcc_versions;
    std::vector<Version> clang_versions;

    int next_stable_binary_syzkaller();
    int next_stable_binary();

    int init_releases_phase_ff(Git &);
    int init_syzkaller_phase(const Environment &, Git &, Git &);
    int init_kernel_phase(Git &);

    int init_releases_phase_poc(Git &);

    int next_phase_ff(const Environment &, Git &, Git &);
    int next_phase_poc(const Environment &, Git &, Git &);

    bool already_fuzzed(const Session &) const;
    bool session_was_found(const Session &) const;
    bool session_was_stable(const Session &) const;

    int build_current_kernel(std::ofstream &, const Environment &, const Bug_Info &, Git &);
    int build_current_syzkaller(const Environment &, const Bug_Info &, Git &, bool = true);

    int goto_finding_session(std::ofstream &, const Environment &, const Bug_Info &, Git &, Git &);
    int goto_release_session_ff(std::ofstream &, const Environment &, const Bug_Info &, Git &, Git &);
    int goto_syzkaller_session_ff(std::ofstream &, const Environment &, const Bug_Info &, Git &, Git &);
    int goto_kernel_session_ff(std::ofstream &, const Environment &, const Bug_Info &, Git &, Git &);

    int goto_release_session_poc(std::ofstream &, const Environment &, const Bug_Info &, Git &, Git &);
    int goto_kernel_session_poc(std::ofstream &, const Environment &, const Bug_Info &, Git &, Git &);
    int goto_syzkaller_session_poc(std::ofstream &, const Environment &, const Bug_Info &, Git &, Git &);

    int next_session_ff(std::ofstream &, const Environment &, const Bug_Info &, Git &, Git &);
    int next_session_poc(std::ofstream &, const Environment &, const Bug_Info &, Git &, Git &);

    Test_Result test_finding_ff(std::ofstream &, Environment &, Bug_Info &);
    Test_Result test_syzkaller(std::ofstream &, Environment &, Bug_Info &);
    Test_Result test_kernel_ff(std::ofstream &, Environment &, Bug_Info &);

    Test_Result test_finding_poc(std::ofstream &, Environment &, Bug_Info &);
    Test_Result test_kernel_poc(std::ofstream &, Environment &, Bug_Info &);

    Test_Result test_current_ff(std::ofstream &, Environment &, Bug_Info &);
    Test_Result test_current_poc(std::ofstream &, Environment &, Bug_Info &);

    int record_syzkaller(const Test_Result &);
    int record_kernel(const Test_Result &, Git &);
    int record_release(const Test_Result &);
    int _archive_session();

public:
    std::vector<Version> releases;
    std::vector<Version> kernel_versions;
    std::vector<Version> syzkaller_versions;

    int init(const Environment &, const Bug_Info &, Git &);

    int set_algorithm(const std::string &);
    Bisect_Algorithm algorithm() const
    { return alg; }

    int inc_session()
    { return ++session_count; }

    Session this_session() const
    { return current_session; }
    Bisect_Phase this_phase() const
    { return phase; }

    int remaining() const;
    int stable_remaining() const;

    int gather_compiler_versions(const Environment &);

    int next_phase(const Bisect_Phase, const Environment &, Git &, Git &);
    int skip_syzkaller(const std::string &, Git &);
    int lock_syzkaller();

    // Goto the next session. Build everything as needed
    // Decide next session based on internal state
    int next_session(std::ofstream &, const Environment &, const Bug_Info &, Git &, Git &);

    // Fuzz and return a result
    Test_Result test_current(std::ofstream &, Environment &, Bug_Info &, Git &);

    int record(const Test_Result &, Git &);
    int archive_session(const Test_Result &);

    std::string print_result(const Environment &, const Bug_Info &, Git &, const std::string &) const;
};

bool check_safe_mode(const Test_Result &, bool &, unsigned int &, unsigned int &);
// switch to fuzzing in safe mode. More fuzzing attempts and for longer.
void set_safe_mode(bool &, unsigned int &, unsigned int &);
void log_safe_mode(std::ofstream &, int, int);

void log_datetime(std::ofstream &);

void log_session_info(std::ofstream &, const Session &, const int);
void log_session_compiler(std::ofstream &, const std::string &);

void log_kernel_build_error(std::ofstream &);
void log_syzkaller_build_error(std::ofstream &);

void log_attempt_result(std::ofstream &, const Syzkaller_Result &, int, const std::vector<std::string> &, int);
void log_session_result(std::ofstream &, const Test_Result &, const std::vector<std::string> &);

#endif
