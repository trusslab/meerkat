#ifndef BISECT_H
#define BISECT_H

#include <consts.h>
#include <date.h>
#include <environment.h>
#include <git.h>
#include <result.h>
#include <version.h>

#include <fstream>
#include <vector>
#include <set>
#include <chrono>

enum Bisect_Mode {Mode_PoC, Mode_FF};
enum Bisect_Phase {Bisect_Init, Bisect_Anchor, Bisect_Releases, Bisect_Kernel, Bisect_Done};

enum Compiler_Type {CC_GCC, CC_CLANG};

class Session
{
public:
    Version kernel;
    Bisect_Mode mode;
    bool found;
    bool stable;

    Session()
        : stable(true)
    { return; }

    Session(const Version &k, Bisect_Mode m, bool f)
        : kernel(k), mode(m), found(f), stable(true)
    {
        return;
    }

    bool operator==(const Session &other)
    { return kernel == other.kernel && mode == other.mode; }

    bool operator<(const Session &other) const
    { return kernel.id + std::to_string(mode) < other.kernel.id + std::to_string(other.mode); }
};

class Bisect
{
private:
    Bisect_Mode bisect_mode;
    Bisect_Phase phase;

    unsigned int session_count;
    Session current_session;
    Session last_session;
    std::set<Session> past_sessions;

    Version anchor_version;
    Version bisect_version;
    Version good_version;

    int index;

    bool treat_error_as_good;

    bool git_stop;

    unsigned int repro_count;
    bool defer_repro;

    Compiler_Type compiler_type;
    std::string default_compiler;

    int set_compiler_type(const std::string &);
    int check_compiler_versions(const Environment &);
    std::string clang_mux(const Environment &, Git &, const std::string &);
    std::string gcc_mux(const Environment &, Git &, const std::string &);
    std::string get_compiler_for_commit(const Environment &, Git &, const std::string &);

    int init_anchor_phase(Git &);
    int init_releases_phase(const Environment &, Git &);
    int init_bisect_phase(Git &);

    bool already_fuzzed(const Session &) const;
    bool session_was_found(const Session &) const;
    bool session_was_stable(const Session &) const;

    int build_current_kernel(const Environment &, Git &, bool = false);

    int goto_anchor_session(const Environment &, Git &);
    int goto_release_session(const Environment &, Git &);
    int goto_bisect_session(const Environment &, Git &);

    int do_syz_repro(Environment &);

    Test_Result test_anchor_ff(Environment &);
    Test_Result test_anchor_poc(Environment &);

    Test_Result test_bisect_ff(Environment &);
    Test_Result test_bisect_poc(Environment &);

    Test_Result test_anchor(Environment &);
    Test_Result test_bisect(Environment &);

    int bisect_remaining(Git &) const;

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

    int remaining(Git &) const;

    int next_phase(const Bisect_Phase, const Environment &, Git &);

    // Goto the next session. Build everything as needed
    // Decide next session based on internal state
    int next_session(const Environment &, Git &);

    // Fuzz and return a result
    Test_Result test_current(Environment &, Git &);

    int set_good_version(Git &);
    int record(const Test_Result &, Git &);
    int archive_session(const Test_Result &);

    std::string print_anchor_fail(const Environment &, const std::chrono::steady_clock::time_point &, const std::chrono::steady_clock::time_point &, const std::string & = "", const std::string & = "") const;
    std::string print_partial_result(const Environment &, Git &, const std::chrono::steady_clock::time_point &, const std::chrono::steady_clock::time_point &, const std::string & = "", const std::string & = "") const;
    std::string print_result(const Environment &, Git &, const std::chrono::steady_clock::time_point &) const;
};

std::string runtime(const std::chrono::steady_clock::time_point &);

int uniqify_reproducers(Environment &);

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
void log_attempt_result_poc(const Syzkaller_Result &, int, const std::vector<std::string> &);
void log_session_result(const Test_Result &);

#endif
