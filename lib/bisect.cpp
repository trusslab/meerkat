#include <bisect.h>
#include <bug_info.h>
#include <consts.h>
#include <environment.h>
#include <fuzz.h>
#include <fuzz_prep.h>
#include <git.h>
#include <git_traverse.h>
#include <my_string.h>
#include <result.h>
#include <session.h>
#include <shell_api.h>
#include <version.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <chrono>

std::vector<Version> gather_release_tags(const Environment &env, Git &linux_git)
{
    std::string tagfile = env.wd + "linux_release_tags.txt";
    std::vector<Version> releases;
    linux_git.dump_tags(tagfile);
    
    std::ifstream inf;
    inf.open(tagfile);
    std::string tag;
    while (getline(inf, tag))
    {
        releases.push_back(Version(tag, linux_git.get_tag_hash(tag), linux_git.get_tag_date(tag)));
    }
    inf.close();
    return releases;
}

int Bisect::init(const Environment &env, const Bug_Info &bug, Git &linux_git)
{
    session_count = 0;
    phase = Bisect_Init;
    git_stop = false;

    gather_compiler_versions(env);

    finding_version.name = bug.find_hash;
    finding_version.date = linux_git.get_commit_date(bug.find_hash);
    
    // Gather linux release tags, dates, and hashes
    releases = gather_release_tags(env, linux_git);

    last_session.kernel.name.clear();

    return 0;
}

int Bisect::set_mode(const Bisect_Mode &m)
{
    bisect_mode = m;
    return 0;
}

int Bisect::remaining() const
{
    switch(phase)
    {
    case Bisect_Finding:
        return 1;
    case Bisect_Releases:
        return releases.size() - index;
    case Bisect_Kernel:
        return git_remaining;
    default:
        return -1;
    }
    return -1;
}

int Bisect::stable_remaining() const
{
    std::vector<Version> versions(kernel_versions.begin() + left, kernel_versions.begin() + right);
    infer_stability(versions);
    return get_only_stable(versions).size();
}

int Bisect::gather_compiler_versions(const Environment &env)
{
    gcc_versions = grab_compiler_versions(env.gcc_dir + "/gccVersions.csv");
    clang_versions = grab_compiler_versions(env.gcc_dir + "/clangVersions.csv");
    return (gcc_versions.size() + clang_versions.size() != 0);
}

int Bisect::init_releases_phase_ff()
{
    // We can get here by the first release search, or after syzkaller version is known.
    Version anchor = false ? bisect_session.kernel : finding_version;

    // Cut out any releases from after the finding date. This helps line up the index.
    int i = 0;
    for (i = 0; i < releases.size() && releases.at(i).date > anchor.date; i++);
    releases = std::vector<Version>(releases.begin() + i, releases.end());
    if (releases.size() <= 0)
        return -1;
    // Set index to -1 here so it is 0 when we increment the first time.
    index = -1;

    bisect_session = Session(anchor, true);
    bisect_index = -1;
    return 0;
}

int Bisect::init_gb_phase(Git &linux_git)
{
    git_stop = false;
    // Switch over to git bisect for this phase
    std::string bad = bisect_session.kernel.name;
    std::string good = releases.at(bisect_index + 1).name;
    linux_git.cleanup();
    linux_git.bisect_reset();
    if (linux_git.error() < 0)
        return -1;
    git_remaining = linux_git.bisect_start(bad, good);
    if (linux_git.error() < 0)
    {
        std::cerr << "Error: git bisect start " << bad << " " << good << " failed\n" << std::flush;
        return -1;
    }

    return 0;
}

int Bisect::init_releases_phase_poc(Git &syzkaller_git)
{
    // We can get here by the first release search, or after syzkaller version is known.
    Version anchor = false ? bisect_session.kernel : finding_version;

    // Cut out any releases from after the finding date. This helps line up the index.
    int i = 0;
    for (i = 0; i < releases.size() && releases.at(i).date > anchor.date; i++);
    releases = std::vector<Version>(releases.begin() + i, releases.end());
    if (releases.size() <= 0)
        return -1;
    // Set index to -1 here so it is 0 when we increment the first time.
    index = -1;

    bisect_session = Session(anchor, true);
    bisect_index = -1;
    return 0;
}

int Bisect::next_phase(const Bisect_Phase next, const Environment &env, Git &linux_git)
{
    int err = 0;
    phase = next;

    // TODO: phase stuff

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

int Bisect::build_current_kernel(const Environment &env, const Bug_Info &bug, Git &linux_git, bool bisecting)
{
    std::string compiler;
    std::cout << SPACER
         << "Prepping the kernel\n";
    compiler = get_compiler(gcc_versions, clang_versions, current_session.kernel.date, env);
    log_session_compiler(compiler);
    return prep_kernel(env, bug, linux_git, current_session.kernel, compiler, bisecting);
}

int Bisect::goto_anchor_session(const Environment &env, const Bug_Info &bug, Git &linux_git)
{
    int err = 0;
    Version linux_version;

    if (linux_git.cleanup() < 0)
        return -1;

    // Fetch the finding commit
    linux_version = finding_version;

    current_session = Session(linux_version, false);
    log_session_info(this_session(), inc_session());

    err = build_current_kernel(env, bug, linux_git);
    if (err < 0)
    {
        log_kernel_build_error();
        return -1;
    }

    return 0;
}

int Bisect::goto_release_session(const Environment &env, const Bug_Info &bug, Git &linux_git)
{
    int err = 0;
    Version linux_version;

retry:
    index++;
    if (index >= releases.size() || (index > 0 && !last_session.found && last_session.stable))
    {
        std::cout << "There are no more stable releases to test\n" << std::flush;
        return 1;
    }
    else if (index < 0)
    {
        std::cerr << "Error: index < 0 in goto_release_session_ff\n" << std::flush;
        return -1;
    }

    linux_version = releases.at(index);
    current_session = Session(linux_version, false);
    log_session_info(current_session, inc_session());

    if (!already_fuzzed(this_session()))
    {
        // May be no need to rebuild kernel version
        if (last_session.kernel.name != current_session.kernel.name)
        {
            err = build_current_kernel(env, bug, linux_git);
            if (err < 0)
            {
                log_kernel_build_error();
                std::cout << "Attempting to recover.\n" << std::flush;
                current_session.stable = false;
                _archive_session();
                goto retry;
            }
        }
    }

    return err;
}

int Bisect::goto_bisect_session(const Environment &env, const Bug_Info &bug, Git &linux_git)
{
    int err = 0;
    if (git_stop)
        return 1;

retry:
    // For git bisect, we should already be at the next commit to test
    current_session = Session(linux_git.get_current_version(), false);
    if (linux_git.error() < 0)
        return -1;
    log_session_info(current_session, inc_session());

    if (!already_fuzzed(this_session()))
    {
        err = build_current_kernel(env, bug, linux_git, true);
        if (err < 0)
        {
            log_kernel_build_error();
            std::cout << "Attempting to recover.\n" << std::flush;
            current_session.stable = false;
            _archive_session();
            linux_git.cleanup();
            if (linux_git.bisect_skip() == -2)
                return 1;
            goto retry;
        }
    }

    return err;
}

// Goto the next session based on the internal state. Called functions should:
// update internal indices as needed
// build kernel and syzkaller
// return 0 to continue same phase, return 1 to indicate phase is done.
int Bisect::next_session(const Environment &env, const Bug_Info &bug, Git &linux_git)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Finding:
        err = goto_anchor_session(env, bug, linux_git);
        break;
    case Bisect_Releases:
        err = goto_release_session(env, bug, linux_git);
        break;
    case Bisect_Kernel:
        err = goto_bisect_session(env, bug, linux_git);
        break;
    default:
        err = -1;
    }
    return err;
}

Test_Result Bisect::test_anchor_ff(Environment &env, Bug_Info &bug)
{
    Test_Result result = fuzz_loop_finding(env, bug, false /* env feature sc */);
    return result;
}

Test_Result Bisect::test_anchor_poc(Environment &env, Bug_Info &bug)
{
    Test_Result result = poc_loop_finding(env, bug);
    return result;
}

Test_Result Bisect::test_anchor(Environment &env, Bug_Info &bug)
{
    Test_Result result;
    if (mode() == Mode_FF)
    {
        result = test_anchor_ff(env, bug);
    }
    else if (mode() == Mode_PoC)
    {
        result = test_anchor_poc(env, bug);
    }

    env.max_time = (env.safe_mode ? env.max_time : result.suggest_ttf);
    log_session_result(result, bug.duplicates);
    bug.blocking_bugs.count_blocking_bugs(result);
    return result;
}

Test_Result Bisect::test_bisect_ff(Environment &env, Bug_Info &bug)
{
    Test_Result result;
    result = fuzz_loop(env, bug, false /* env feature sc */);
    return result;
}

Test_Result Bisect::test_bisect_poc(Environment &env, Bug_Info &bug)
{
    Test_Result result;
    result = poc_loop(env, bug);
    return result;
}

Test_Result Bisect::test_bisect(Environment &env, Bug_Info &bug)
{
    Test_Result result;
    if (mode() == Mode_FF)
    {
        result = test_bisect_ff(env, bug);
    }
    else if (mode() == Mode_PoC)
    {
        result = test_bisect_poc(env, bug);
    }

    log_session_result(result, bug.duplicates);
    bug.blocking_bugs.count_blocking_bugs(result);
    if (env.try_patch && patch_blocking_bugs(result, bug) > 0)
    {
        result.retry = true;
        std::cout << "Attempted to patch one or more blocking bugs. Retrying\n" << std::flush;
    }
    if (check_safe_mode(result, env.safe_mode, env.max_time, env.fuzztimes))
        log_safe_mode(env.max_time, env.fuzztimes);

    return result;
}

Test_Result Bisect::test_current(Environment &env, Bug_Info &bug, Git &linux_git)
{
    Test_Result res;

    if (!already_fuzzed(this_session()))
    {
retry:
        switch (phase)
        {
        case Bisect_Finding:
            res = test_anchor(env, bug);
            break;
        case Bisect_Releases:
        case Bisect_Kernel:
            res = test_bisect(env, bug);
            break;
        default:
            break;
        }

        if (res.retry && env.try_patch)
        {
            // In the case a blocking bug is found and removed, rebuild and go again
            log_session_info(current_session, inc_session());
            build_current_kernel(env, bug, linux_git);
            goto retry;
        }
    }
    else
    {
        res.found = session_was_found(this_session()) == 1 ? true : false;
        res.stable = session_was_stable(this_session()) == 1 ? true : false;
        std::cout << "The bug was " << (res.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << std::flush;
    }

    return res;
}

int Bisect::record_kernel(const Test_Result &result, Git &linux_git)
{
    if (!already_fuzzed(current_session))
        archive_session(result);
    
    if (result.found)
    {
        linux_git.cleanup();
        git_remaining = linux_git.bisect_bad();
        bisect_session = current_session;
    }
    else if (result.stable)
    {
        linux_git.cleanup();
        git_remaining = linux_git.bisect_good();
        good_session = current_session;
    }
    else if (!result.stable)
    {
        linux_git.cleanup();
        git_remaining = linux_git.bisect_skip();
    }
    if (git_remaining == -2)
        git_stop = true;

    return 0;
}

int Bisect::record_release(const Test_Result &result)
{
    if (!already_fuzzed(current_session))
        archive_session(result);
    
    if (result.found)
    {
        bisect_session = current_session;
        bisect_index = index;
    }
    else if (result.stable)
        good_session = current_session;
    
    return 0;
}

int Bisect::record_anchor(const Test_Result &result)
{
    bisect_session = current_session;
    archive_session(result);
    return 0;
}

int Bisect::record(const Test_Result &result, Git &linux_git)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Finding:
        err = record_anchor(result);
        break;
    case Bisect_Releases:
        err = record_release(result);
        break;
    case Bisect_Kernel:
        err = record_kernel(result, linux_git);
        break;
    default:
        err = -1;
        break;
    }
    return err;
}

int Bisect::_archive_session()
{
    if (phase == Bisect_Releases)
        releases.at(index).skipped = !current_session.stable && !current_session.found;

    last_session = this_session();
    past_sessions.insert(this_session());
    return 0;
}

int Bisect::archive_session(const Test_Result &result)
{
    current_session.found = result.found;
    current_session.stable = result.stable;
    return _archive_session();
}

std::string Bisect::print_result(const Environment &env, const Bug_Info &bug, Git &linux_git, const std::chrono::steady_clock::time_point &start) const
{
    // TODO: iomanip this
    std::stringstream ss;
    ss << "Bug Name:             " << bug.name << "\n";
    ss << "Bug Link:             " << bug.buglink << "\n";
    ss << "Bisection Result:     " << bisect_session.kernel.date.get_date() << " - " << bisect_session.kernel.name << "\n";
    ss << "Bisected Commit Name: " << linux_git.get_commit_name(bisect_session.kernel.name) << "\n";
    ss << "Run Time:             " << runtime(start) << "\n";
    ss << "Arch:                 " << bug.arch << "\n\n";

    ss << "Finding Commit:       " << finding_version.date.get_date() << " - " << finding_version.name << "\n";
    ss << "Bisection Result:     " << bisect_session.kernel.date.get_date() << " - " << bisect_session.kernel.name << "\n";

    return ss.str();
}

std::string runtime(const std::chrono::steady_clock::time_point &start)
{
    std::stringstream pretty;
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto diff = start - end;

    auto hours = std::chrono::duration_cast<std::chrono::hours>(diff);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(diff - hours);
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(diff - hours - minutes);

    pretty << hours.count() << "h" << minutes.count() << "m" << seconds.count() << "s";

    return pretty.str();
}

void log_safe_mode(int max_time, int fuzztimes)
{
    std::cout << "Switching to Safe Mode: Fuzzing " << fuzztimes << " times at " << max_time << " minutes\n";
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

std::string get_datetime()
{
    return date("%Y-%m-%d %T");
}

void log_datetime()
{
    std::cout << get_datetime();
}

void log_session_info(const Session &session, const int count)
{
    // date puts an endline there on its own
    std::cout << "\n" << get_datetime() << ""
              << "Session:   " << count << "\n"
              << "Kernel:    " << session.kernel.date.get_date() << " - " << session.kernel.name
              << (session.kernel.tag.empty() ? "" : " ("+session.kernel.tag+")") << "\n" << std::flush;
}

void log_session_compiler(const std::string &compiler)
{
    std::cout << "Compiler:  " << compiler << "\n" << std::flush;
}

void log_kernel_build_error()
{
    std::cout << "Error: The kernel failed to make.\n" << std::flush;
}

void log_syzkaller_build_error()
{
    std::cout << "Error: Syzkaller failed to make.\n" << std::flush;
}

// the bug was found
// Attempt 1:
//     Time  Bug Name
//        5  KASAN...
// ***    9  UBSAN...
// Attempt 2:
// ...

void log_attempt_result(const Syzkaller_Result &attempt, int i, const std::vector<std::string> &dups, int fuzztimes)
{
    std::cout << "Attempt " << i << ":" << (i > fuzztimes ? " (RETRY)" : "") << "\n";

    if (attempt.reports.size() > 0)
        std::cout << "    Time  Bug Name\n" << std::flush;
    else
        std::cout << "    No crashes found.\n" << std::flush;
    
    for (Crash_Report cr : attempt.reports)
        std::cout << (fuzz_is_crash_in(cr.name, dups) ? "*** " : "    ") << std::right << std::setw(4) << cr.time << "  " << cr.name << std::endl << std::flush;
}

void log_session_result(const Test_Result &result, const std::vector<std::string> &dups)
{
    std::cout << "The bug was " << (result.found ? "" : "not ") << "found.\n" << std::flush;

    if (!result.stable)
        std::cout << "Warning: This session is unstable.\n" << std::flush;
}
