#include <consts.h>
#include <environment.h>
#include <fuzz.h>
#include <fuzz_prep.h>
#include <git_api.h>
#include <git_traverse.h>
#include <inspector_config.h>
#include <my_string.h>
#include <result.h>
#include <retrospect.h>
#include <session.h>
#include <shell_api.h>
#include <version.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>

using namespace std;

int Bisect::init(const Environment &env, const InspectorConfig &inspector, const Bug_Info &bug, bool have_fdate)
{
    session_count = 0;
    phase = Bisect_Init;

    gather_compiler_versions(inspector);

    // commits are arranged newest (low index) to oldest (high index)
    // Back is guilty, front is finding
    kernel_versions = get_kernel_versions(env, bug);
    if (kernel_versions.size() == 0)
        return -1;

    finding_version.name = bug.find_hash;
    guilty_version.name = bug.guilty_hash;
    finding_version.date = kernel_versions.front().date;
    if (have_fdate)
        high_date = find_date = bug.find_date;
    else
        high_date = find_date = finding_version.date;

    low_date = guilty_version.date = kernel_versions.back().date;
    if (low_date < SYZBOT_BEGIN_DATE)
        low_date = SYZBOT_BEGIN_DATE;

    // commits are arranged newest (low index) to oldest (high index)
    syzkaller_versions = get_syzkaller_versions(env);
    if (syzkaller_versions.size() == 0)
        return -1;

    last_session.kernel.name.clear();
    last_session.syzkaller.name.clear();

    return 0;
}

int Bisect::init(const Environment &env, const InspectorConfig &inspector, const Bug_Info &bug, bool have_fdate, const string &known_syzkaller)
{
    int err = init(env, inspector, bug, have_fdate);
    if (err < 0)
        return err;
    bisect_version = Version(known_syzkaller, git_get_commit_date(env.wd, env.syzdir, known_syzkaller));
    return err;

}

int Bisect::stable_remaining() const
{
    vector<Version> versions(kernel_versions.begin() + left, kernel_versions.begin() + right);
    infer_stability(versions);
    return get_only_stable(versions).size();
}

int Bisect::gather_compiler_versions(const InspectorConfig &inspector)
{
    gcc_versions = grab_compiler_versions(inspector.get_gcc_dir() + "/gccVersions.csv");
    clang_versions = grab_compiler_versions(inspector.get_gcc_dir() + "/clangVersions.csv");
    return (gcc_versions.size() + clang_versions.size() != 0);
}

Version Bisect::find_merge_commit(const Environment &env, const Bug_Info &bug)
{
    merge_commit = git_find_merge_commit(env.kerneldir, kernel_versions, bug.guilty_hash);

    if (!merge_commit.name.empty())
    {
        low_date = merge_commit.date > SYZBOT_BEGIN_DATE ? merge_commit.date : SYZBOT_BEGIN_DATE;
        // cut the kernel_versions here. Find the merge commit, then erase everything after it.
        kernel_versions.erase(kernel_versions.begin() + get_index_by_name(kernel_versions, merge_commit.name) + 1, kernel_versions.end());
    }
    
    return merge_commit;
}

int Bisect::init_syzkaller_phase()
{
    bisect_version = syzkaller_versions.front();

    // right is the older date (lower date). higher index
    // left is the recent date (higher date). lower index
    right = syzkaller_versions.size() - 2;
    left = 0;

    return (right >= 0 ? 0 : -1);
}

int Bisect::init_kernel_phase()
{
    // coming off of syzkaller bisection, lock the syzkaller version.
    current_session.syzkaller = Version(bisect_version.name, bisect_version.date);

    // right is the starting date. older date (lower date). higher index
    // left is the ending date. recent date (higher date). lower index
    if (!merge_commit.name.empty())
    {
        right = get_starting_index(kernel_versions, merge_commit.date);
        low_date = merge_commit.date;
    }
    else
    {
        right = get_starting_index(kernel_versions, guilty_version.date);
        low_date = guilty_version.date;
    }
    if (low_date < SYZBOT_BEGIN_DATE)
    {
        low_date = SYZBOT_BEGIN_DATE;
        right = get_starting_index(kernel_versions, low_date);
    }
    high_date = bisect_version.date;
    left = get_ending_index(kernel_versions, high_date);
    bisect_version = kernel_versions.at(left);
    return 0;
}

int Bisect::next_phase(Bisect_Phase expect)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Init:
        phase = Bisect_Finding;
        break;
    case Bisect_Finding:
        phase = Bisect_Syzkaller;
        err = init_syzkaller_phase();
        break;
    case Bisect_Syzkaller:
        phase = Bisect_Kernel;
        err = init_kernel_phase();
        break;
    case Bisect_Kernel:
        phase = Bisect_Done;
        break;
    default:
        err = -1;
        break;
    }

    if (expect != phase)
        err = -1;

    return err;
}

int Bisect::skip_syzkaller()
{
    if (phase != Bisect_Finding)
        return -1;
    phase = Bisect_Syzkaller;
    return 0;
}

int Bisect::next_stable_binary_syzkaller()
{
    middle = (left + right) / 2;
    return 0;
}

// Binary search abstracted here to include skipping.
// If there are no stable commits left, end the search.
// right is older date (lower). higher index
// left is recent date (higher). lower index
int Bisect::next_stable_binary_kernel()
{
    // for left to right, copy into versions
    vector<Version> versions(kernel_versions.begin() + left, kernel_versions.begin() + right);
    cout << "Versions Remaining: " << versions.size() << "/" << remaining() << endl << flush;
    infer_stability(versions, true);
    versions = get_only_stable(versions);
    cout << "Stable Versions: ~" << versions.size() << endl << flush;

    if (versions.size() == 0)
        return 1;

    int m = versions.size() / 2, old_mid = middle;

    // find hash again in kernel versions
    for (int i = 0; i < kernel_versions.size(); i++)
    {
        if (versions.at(m).name == kernel_versions.at(i).name)
        {
            middle = i;
            break;
        }
    }
    cout << "Middle: " << middle << "/" << m << endl << flush;
    // infinite loop prevention
    if (middle == old_mid)
        return 1;

    return 0;
}

int Bisect::next_stable_binary()
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Syzkaller:
        err = next_stable_binary_syzkaller();
        break;
    case Bisect_Kernel:
        err = next_stable_binary_kernel();
        break;
    default:
        err = -1;
        break;
    }
    return err;
}

bool Bisect::already_fuzzed(const Session &session) const
{
    return past_sessions.find(session) != past_sessions.end();
}

bool Bisect::session_was_found(const Session &session) const
{
    if (!already_fuzzed(session))
        return false;
    
    return past_sessions.find(session)->found;
}

bool Bisect::session_was_stable(const Session &session) const
{
    if (!already_fuzzed(session))
        return true;

    return past_sessions.find(session)->stable;
}

int Bisect::build_current_kernel(ofstream &logfile, const Environment &env, const InspectorConfig &inspector, const Bug_Info &bug)
{
    string compiler;
    cout << SPACER
         << "Prepping the kernel\n";
    compiler = get_compiler(gcc_versions, clang_versions, current_session.kernel.date, inspector);
    log_session_compiler(logfile, compiler);
    return prep_kernel(env, bug, inspector, current_session.kernel, env.linux_repo_remote, compiler);
}

int Bisect::build_current_syzkaller(const Environment &env, const InspectorConfig &inspector, const Bug_Info &bug)
{
    cout << SPACER
        << "Prepping Syzkaller\n";
    return prep_syzkaller(env, bug, inspector, current_session.syzkaller);
}

int Bisect::goto_finding_session(ofstream &logfile, const Environment &env, const InspectorConfig &inspector, const Bug_Info &bug)
{
    int err = 0;
    string compiler;
    Version linux_version, syzkaller_version;

    kernel_index = get_index_by_name(kernel_versions, kernel_versions.front().name);
    if (kernel_index < 0)
    {
        cerr << "This kernel version does not exist.\n";
        logfile << "Error: Could not find kernel version " << kernel_versions.front().name << ".\n" << flush;
        return -1;
    }
    linux_version = kernel_versions.at(kernel_index);
    syzkaller_version = get_version_by_date(syzkaller_versions, high_date);

    current_session = Session(linux_version, syzkaller_version, false);
    log_session_info(logfile, this_session(), inc_session());

    err = build_current_kernel(logfile, env, inspector, bug);
    if (err < 0)
    {
        log_kernel_build_error(logfile);
        return -1;
    }

    err = build_current_syzkaller(env, inspector, bug);
    if (err < 0)
    {
        log_syzkaller_build_error(logfile);
        return -1;
    }

    return 0;
}

int Bisect::goto_syzkaller_session(ofstream &logfile, const Environment &env, const InspectorConfig &inspector, const Bug_Info &bug)
{
    if (left > right)
        return 1;
    
    int err = 0;
    string compiler;
    Version linux_version, syzkaller_version;

    next_stable_binary();
    syzkaller_version = syzkaller_versions.at(middle);
retry_kernel:
    linux_version = get_stable_version_by_date(kernel_versions, syzkaller_version.date);
    kernel_index = get_index_by_name(kernel_versions, linux_version.name);
    current_session = Session(linux_version, syzkaller_version, false);
    log_session_info(logfile, current_session, inc_session());

    if (!already_fuzzed(this_session()))
    {
        // There's a chance the kernel is already built. Take advantage of this.
        if (last_session.kernel != current_session.kernel)
        {
            err = build_current_kernel(logfile, env, inspector, bug);
            if (err < 0)
            {
                log_kernel_build_error(logfile);
                logfile << "Attempting to recover.\n" << flush;
                current_session.stable = false;
                archive_current_session();
                goto retry_kernel;
            }
        }

        err = build_current_syzkaller(env, inspector, bug);
        if (err < 0)
        {
            log_syzkaller_build_error(logfile);
            return -1;
        }
    }
    return err;
}

int Bisect::goto_kernel_session(ofstream &logfile, const Environment &env, const InspectorConfig &inspector, const Bug_Info &bug)
{
    if (left > right)
        return 1;

    int err = 0;
    string compiler;

retry:
    err = next_stable_binary();
    kernel_index = middle;
    if (err == 1)
    {
        cout << "There are no more stable commits to test.\n";
        return 1;
    }
    // keeping in mind locked syzkaller version
    current_session.kernel = kernel_versions.at(middle);
    current_session.found = false;
    current_session.stable = true;
    log_session_info(logfile, current_session, inc_session());

    if (!already_fuzzed(this_session()))
    {
        err = build_current_kernel(logfile, env, inspector, bug);
        if (err < 0)
        {
            log_kernel_build_error(logfile);
            logfile << "Attempting to recover.\n" << flush;
            current_session.stable = false;
            archive_current_session();
            goto retry;
        }

        // if syzkaller version is locked, no need to build it all the time
        if (last_session.syzkaller.name != current_session.syzkaller.name)
        {
            err = build_current_syzkaller(env, inspector, bug);
            if (err < 0)
            {
                log_syzkaller_build_error(logfile);
                return -1;
            }
        }
    }

    return err;
}

// Goto the next session based on the internal state. Called functions should:
// update internal indices as needed
// build kernel and syzkaller
// return 0 to continue same phase, return 1 to indicate phase is done.
int Bisect::next_session(ofstream &outf, const Environment &env, const InspectorConfig &inspector, const Bug_Info &bug)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Finding:
        err = goto_finding_session(outf, env, inspector, bug);
        break;
    case Bisect_Syzkaller:
        err = goto_syzkaller_session(outf, env, inspector, bug);
        break;
    case Bisect_Kernel:
        err = goto_kernel_session(outf, env, inspector, bug);
        break;
    default:
        err = -1;
        break;
    }

    return err;
}

Test_Result Bisect::test_finding(std::ofstream &logfile, Environment &env, InspectorConfig &inspector, Bug_Info &bug)
{
    Test_Result result;
    result = fuzz_loop_finding(logfile, env, bug, inspector, current_session.syzkaller.date);
    env.max_time = (env.safe_mode ? env.max_time : result.suggest_ttf);
    log_session_result(logfile, result, bug.duplicates);
    bug.blocking_bugs.count_blocking_bugs(result);
    return result;
}

Test_Result Bisect::test_syzkaller(std::ofstream &logfile, Environment &env, InspectorConfig &inspector, Bug_Info &bug)
{
    Test_Result result;
    if (!already_fuzzed(this_session()))
    {
        cout << SPACER;
        result = fuzz_loop(logfile, env, bug, inspector, current_session.syzkaller.date);
        log_session_result(logfile, result, bug.duplicates);
        bug.blocking_bugs.count_blocking_bugs(result);
        if (check_safe_mode(result, env.safe_mode, env.max_time, env.fuzztimes))
            log_safe_mode(logfile, env.max_time, env.fuzztimes);
    }
    else
    {
        cout << "This session has already been fuzzed. Skipping.\n";
        result.found = session_was_found(this_session()) == 1 ? true : false;
        result.stable = session_was_stable(this_session()) == 1 ? true : false;
        logfile << "The bug was " << (result.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
    }

    return result;
}

Test_Result Bisect::test_kernel(std::ofstream &logfile, Environment &env, InspectorConfig &inspector, Bug_Info &bug)
{
    Test_Result result;
    if (!already_fuzzed(this_session()))
    {
        cout << SPACER;
        result = fuzz_loop(logfile, env, bug, inspector, current_session.syzkaller.date);
        log_session_result(logfile, result, bug.duplicates);
        bug.blocking_bugs.count_blocking_bugs(result);
        if (patch_blocking_bugs(result, bug) > 0)
        {
            result.retry = true;
            logfile << "Attempted to patch one or more blocking bugs. Retrying\n" << flush;
        }
        if (check_safe_mode(result, env.safe_mode, env.max_time, env.fuzztimes))
            log_safe_mode(logfile, env.max_time, env.fuzztimes);
    }
    else
    {
        cout << "This session has already been fuzzed. Skipping.\n";
        result.found = session_was_found(this_session()) == 1 ? true : false;
        result.stable = session_was_stable(this_session()) == 1 ? true : false;
        logfile << "The bug was " << (result.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << flush;
    }
    return result;
}

Test_Result Bisect::test_current(std::ofstream &outf, Environment &env, InspectorConfig &inspector, Bug_Info &bug)
{
    Test_Result res;
    res.retry = false;
retry:
    switch(phase)
    {
    case Bisect_Finding:
        res = test_finding(outf, env, inspector, bug);
        break;
    case Bisect_Syzkaller:
        res = test_syzkaller(outf, env, inspector, bug);
        break;
    case Bisect_Kernel:
        res = test_kernel(outf, env, inspector, bug);
        break;
    default:
        break;
    }
    if (res.retry)
    {
        // In the case a blocking bug is found and removed, rebuild and go again
        log_session_info(outf, current_session, inc_session());
        build_current_kernel(outf, env, inspector, bug);
        goto retry;
    }

    return res;
}

int Bisect::record_syzkaller(const Test_Result &result)
{
    if (!already_fuzzed(current_session))
        archive_session(result);

    if (result.found)
    {
        left = middle + 1;
        bisect_version = current_session.syzkaller;
        high_date = current_session.syzkaller.date;
    }
    else
    {
        right = middle - 1;
        low_date = current_session.syzkaller.date;
    }
    return 0;
}

int Bisect::record_kernel(const Test_Result &result)
{
    if (!already_fuzzed(current_session))
        archive_session(result);
    
    if (result.found)
    {
        left = middle + 1;
        bisect_version = current_session.kernel;
    }
    else if (result.stable)
        right = middle - 1;

    return 0;
}

int Bisect::record(const Test_Result &result)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Finding:
        err = archive_session(result);
        break;
    case Bisect_Syzkaller:
        err = record_syzkaller(result);
        break;
    case Bisect_Kernel:
        err = record_kernel(result);
        break;
    default:
        err = -1;
        break;
    }

    return err;
}

int Bisect::archive_current_session()
{
    kernel_versions.at(kernel_index).skipped = !current_session.stable && !current_session.found;
    last_session = this_session();
    past_sessions.insert(this_session());
    return 0;
}

int Bisect::archive_session(const Test_Result &result)
{
    current_session.found = result.found;
    current_session.stable = result.stable;
    return archive_current_session();
}

std::string Bisect::print_result(const Environment &env, const Bug_Info &bug, const std::string &start) const
{
    // TODO: iomanip this
    std::stringstream ss;
    ss << "Bug Name:             " << bug.name << "\n";
    ss << "Bug Link:             " << bug.buglink << "\n";
    ss << "Bisection Result:     " << bisect_version.date.get_date() << " - " << bisect_version.name << "\n";
    ss << "Bisected Commit Name: " << get_commit_name(env.kerneldir, bisect_version.name) << "\n";
    ss << "Run Time:             " << chomp(start) << " - " << chomp(date("%Y-%m-%d %T")) << "\n";
    ss << "Arch:                 " << bug.arch << "\n\n";

    ss << "Finding Date:         " << find_date.get_date() << "\n";
    ss << "Finding Commit:       " << finding_version.date.get_date() << " - " << finding_version.name << "\n";
    ss << "Bisection Result:     " << bisect_version.date.get_date() << " - " << bisect_version.name << "\n";
    if (!merge_commit.name.empty())
        ss << "Guilty Merge:         " << merge_commit.date.get_date() << " - " << merge_commit.name << "\n";
    else
        ss << "Guilty Merge:         " << "None\n";
    ss << "Guilty Commit:        " << guilty_version.date.get_date() << " - " << guilty_version.name << "\n";

    return ss.str();
}

void log_safe_mode(ofstream &logfile, int max_time, int fuzztimes)
{
    cout << "Switching to Safe Mode: Fuzzing " << fuzztimes << " times at " << max_time << " minutes\n";
    logfile << "Switching to Safe Mode: Fuzzing " << fuzztimes << " times at " << max_time << " minutes\n";
}

void set_safe_mode(bool &safe_mode, unsigned int &max_time, unsigned int &fuzztimes)
{
    safe_mode = true;
    fuzztimes = 5;
    max_time = max_time > 30 ? max_time : 30;
}

// Checks the given result to see if SyzInspector should switch to safe mode.
// If yes, sets safe mode
bool check_safe_mode(const Test_Result &result, bool &safe_mode, unsigned int &max_time, unsigned int &fuzztimes)
{
    if (!safe_mode && result.found && result.attempts.back().ttf > max_time * 0.8)
    {
        set_safe_mode(safe_mode, max_time, fuzztimes);
        return true;
    }
    return false;
}

string get_datetime()
{
    return date("%Y-%m-%d %T");
}

void log_datetime(ofstream &logfile)
{
    logfile << get_datetime();
}

void log_session_info(ofstream &logfile, const Session &session, const int count)
{
    // date puts an endline there on its own
    logfile << "\n" << get_datetime() << ""
            << "Session:   " << count << "\n"
            << "Syzkaller: " << session.syzkaller.date.get_date() << " - " << session.syzkaller.name << "\n"
            << "Kernel:    " << session.kernel.date.get_date() << " - " << session.kernel.name << "\n" << flush;
}

void log_session_compiler(ofstream &logfile, const string &compiler)
{
    logfile << "Compiler:  " << compiler << "\n" << flush;
}

void log_kernel_build_error(ofstream &logfile)
{
    logfile << "Error: The kernel failed to make.\n" << flush;
}

void log_syzkaller_build_error(ofstream &logfile)
{
    logfile << "Error: Syzkaller failed to make.\n" << flush;
}

// the bug was found
// Attempt 1:
//     Time  Bug Name
//        5  KASAN...
// ***    9  UBSAN...
// Attempt 2:
// ...

void log_attempt_result(ofstream &logfile, const Syzkaller_Result &attempt, int i, const vector<string> &dups, int fuzztimes)
{
    logfile << "Attempt " << i << ":" << (i > fuzztimes ? " (RETRY)" : "") << "\n";

    if (attempt.reports.size() > 0)
        logfile << "    Time  Bug Name\n" << flush;
    else
        logfile << "    No crashes found.\n" << flush;
    
    for (Crash_Report cr : attempt.reports)
        logfile << (fuzz_is_crash_in(cr.name, dups) ? "*** " : "    ") << right << setw(4) << cr.time << "  " << cr.name << endl << flush;
}

void log_session_result(ofstream &logfile, const Test_Result &result, const vector<string> &dups)
{
    logfile << "The bug was " << (result.found ? "" : "not ") << "found.\n" << flush;

    if (!result.stable)
        logfile << "Warning: This session is unstable.\n" << flush;
}
