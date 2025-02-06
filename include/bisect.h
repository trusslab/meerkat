#ifndef BISECT_H
#define BISECT_H

#include <date.h>
#include <environment.h>
#include <git.h>
#include <result.h>
#include <session.h>
#include <version.h>

#include <fstream>
#include <vector>
#include <set>
#include <chrono>

enum Bisect_Mode {Mode_PoC, Mode_FF};
enum Bisect_Phase {Bisect_Init, Bisect_Anchor, Bisect_Releases, Bisect_Kernel, Bisect_Done};

class Bisect
{
private:
    Bisect_Mode bisect_mode;
    Bisect_Phase phase;

    unsigned int session_count;
    Session current_session;
    Session last_session;
    Session bisect_session;
    Session good_session;
    std::set<Session> past_sessions;

    Version anchor_version;

    int index;
    int bisect_index;

    // right is the older date (lower date). higher index
    // left is the recent date (higher date). lower index
    int left;
    int right;

    int git_remaining;
    bool git_stop;

    std::vector<Version> gcc_versions;
    std::vector<Version> clang_versions;

    int init_releases_phase_ff();
    int init_gb_phase(Git &);

    int init_releases_phase_poc(Git &);

    int next_phase(const Environment &, Git &);

    bool already_fuzzed(const Session &) const;
    bool session_was_found(const Session &) const;
    bool session_was_stable(const Session &) const;

    int build_current_kernel(const Environment &, Git &, bool = false);

    int goto_anchor_session(const Environment &, Git &);
    int goto_release_session(const Environment &, Git &);
    int goto_bisect_session(const Environment &, Git &);

    Test_Result test_anchor_ff(Environment &);
    Test_Result test_anchor_poc(Environment &);

    Test_Result test_bisect_ff(Environment &);
    Test_Result test_bisect_poc(Environment &);

    Test_Result test_anchor(Environment &);
    Test_Result test_bisect(Environment &);

    int record_anchor(const Test_Result &);
    int record_release(const Test_Result &);
    int record_kernel(const Test_Result &, Git &);
    int _archive_session();

public:
    std::vector<Version> releases;
    std::vector<Version> kernel_versions;
    std::vector<Version> syzkaller_versions;

    int init(const Environment &, Git &);

    int set_mode(const Bisect_Mode &);
    Bisect_Mode mode() const
    { return bisect_mode; }

    int inc_session()
    { return ++session_count; }

    Session this_session() const
    { return current_session; }
    Bisect_Phase this_phase() const
    { return phase; }

    int remaining() const;
    int stable_remaining() const;

    int gather_compiler_versions(const Environment &);

    int next_phase(const Bisect_Phase, const Environment &, Git &);

    // Goto the next session. Build everything as needed
    // Decide next session based on internal state
    int next_session(const Environment &, Git &);

    // Fuzz and return a result
    Test_Result test_current(Environment &, Git &);

    int record(const Test_Result &, Git &);
    int archive_session(const Test_Result &);

    std::string print_result(const Environment &, Git &, const std::chrono::steady_clock::time_point &) const;
};

std::string runtime(const std::chrono::steady_clock::time_point &);

bool check_safe_mode(const Test_Result &, bool &, unsigned int &, unsigned int &);
// switch to fuzzing in safe mode. More fuzzing attempts and for longer.
void set_safe_mode(bool &, unsigned int &, unsigned int &);
void log_safe_mode(int, int);

void log_datetime();

void log_session_info(const Session &, const int);
void log_session_compiler(const std::string &);

void log_kernel_build_error();
void log_syzkaller_build_error();

void log_attempt_result(const Syzkaller_Result &, int, const std::vector<std::string> &, int);
void log_session_result(const Test_Result &, const std::vector<std::string> &);

#endif
