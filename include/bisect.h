#ifndef BISECT_H
#define BISECT_H

#include <date.h>
#include <environment.h>
#include <inspector_config.h>
#include <result.h>
#include <session.h>
#include <version.h>

#include <fstream>
#include <vector>
#include <set>

enum Bisect_Phase {Bisect_Init, Bisect_Finding, Bisect_Syzkaller, Bisect_Kernel, Bisect_Done};

class Bisect
{
private:
    Bisect_Phase phase;

    unsigned int session_count;
    Session current_session;
    Session last_session;
    std::set<Session> past_sessions;

    Version bisect_version;

    Version finding_version;
    Version guilty_version;
    Version merge_commit;

    int kernel_index;
    int syzkaller_index;

    // right is the older date (lower date). higher index
    // left is the recent date (higher date). lower index
    int left;
    int right;
    int middle;
    Date high_date;
    Date low_date;
    Date find_date;

    std::vector<Version> gcc_versions;
    std::vector<Version> clang_versions;

    int next_stable_binary_syzkaller();
    int next_stable_binary_kernel();
    int next_stable_binary();

    int init_syzkaller_phase();
    int init_kernel_phase();

    bool already_fuzzed(const Session &) const;
    bool session_was_found(const Session &) const;
    bool session_was_stable(const Session &) const;

    int build_current_kernel(std::ofstream &, const Environment &, const InspectorConfig &, const Bug_Info &);
    int build_current_syzkaller(const Environment &, const InspectorConfig &, const Bug_Info &);

    int goto_finding_session(std::ofstream &, const Environment &, const InspectorConfig &, const Bug_Info &);
    int goto_syzkaller_session(std::ofstream &, const Environment &, const InspectorConfig &, const Bug_Info &);
    int goto_kernel_session(std::ofstream &, const Environment &, const InspectorConfig &, const Bug_Info &);

    Test_Result test_finding(std::ofstream &, Environment &, InspectorConfig &, Bug_Info &);
    Test_Result test_syzkaller(std::ofstream &, Environment &, InspectorConfig &, Bug_Info &);
    Test_Result test_kernel(std::ofstream &, Environment &, InspectorConfig &, Bug_Info &);

    int record_syzkaller(const Test_Result &);
    int record_kernel(const Test_Result &);
    int archive_current_session();

public:
    std::vector<Version> kernel_versions;
    std::vector<Version> syzkaller_versions;

    int init(const Environment &, const InspectorConfig &, const Bug_Info &, bool);
    int init(const Environment &, const InspectorConfig &, const Bug_Info &, bool, const std::string &);

    int inc_session()
    { return ++session_count; }

    Session this_session() const
    { return current_session; }
    Bisect_Phase this_phase() const
    { return phase; }

    std::string high_date_str() const
    { return high_date.get_date(); }
    std::string low_date_str() const
    { return low_date.get_date(); }
    int remaining() const
    { return right - left; }
    int stable_remaining() const;

    int gather_compiler_versions(const InspectorConfig &inspector);
    Version find_merge_commit(const Environment &env, const Bug_Info &bug);

    int next_phase(Bisect_Phase);
    int skip_syzkaller();

    // Goto the next session. Build everything as needed
    // Decide next session based on internal state
    int next_session(std::ofstream &, const Environment &, const InspectorConfig &, const Bug_Info &);

    // Fuzz and return a result
    Test_Result test_current(std::ofstream &, Environment &, InspectorConfig &, Bug_Info &);

    int record(const Test_Result &);
    int archive_session(const Test_Result &);

    std::string print_result(const Environment &, const Bug_Info &, const std::string &) const;
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
