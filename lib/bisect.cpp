#include <bisect.h>
#include <consts.h>
#include <environment.h>
#include <file_api.h>
#include <fuzz.h>
#include <fuzz_prep.h>
#include <git.h>
#include <my_string.h>
#include <result.h>
#include <shell_api.h>
#include <syzkaller.h>
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
    remove_file(tagfile);
    return releases;
}

int Bisect::init(const Environment &env, Git &linux_git)
{
    session_count = 0;
    repro_count = 0;
    phase = Bisect_Init;
    git_stop = false;
    defer_repro = false;

    if (gather_compiler_versions(env) < 0)
        return -1;

    anchor_version.name = env.anchor_hash;
    anchor_version.date = linux_git.get_commit_date(env.anchor_hash);
    
    bisect_version.name.clear();
    good_version.name.clear();
    last_session.kernel.name.clear();

    return 0;
}

int Bisect::set_mode(const Bisect_Mode &m)
{
    bisect_mode = m;
    return 0;
}

int Bisect::bisect_remaining(Git &linux_git) const
{
    if (bisect_version.name.empty() || good_version.name.empty())
        return -1;
    return linux_git.bisect_remaining(bisect_version.name, good_version.name);
}

int Bisect::remaining(Git &linux_git) const
{
    switch(phase)
    {
    case Bisect_Anchor:
        return 1;
    case Bisect_Releases:
        return releases.size() - index; // TODO: Might change
    case Bisect_Kernel:
        return bisect_remaining(linux_git);
    default:
        return -1;
    }
    return -1;
}

int Bisect::gather_compiler_versions(const Environment &env)
{
    gcc_versions = grab_compiler_versions(env.gcc_dir + "/gccVersions.csv");
    return gcc_versions.size();
}

int Bisect::init_anchor_phase(Git &linux_git)
{
    // Turns out there's nothing to do here.
    return 0;
}

int find_first_ancestor(Git &linux_git, const std::string &child, const std::vector<Version> &list)
{
    int r = 0, l = list.size() - 1;
    int m, best = -1;

    while (r < l)
    {
        m = (r + l)/2;
        if (linux_git.is_ancestor(child, list.at(m).name))
        {
            l = m - 1;
            best = m;
        }
        else
        {
            r = m + 1;
        }
    }
    return best;
}

int skip_tags(std::vector<Version> &releases)
{
    int inc = 1;
    std::vector<Version> tmp = releases;
    releases.clear();
    for (int i = 0; i < tmp.size(); i += inc)
    {
        releases.push_back(tmp.at(i));
        // This mimics how Syz-Bisect skips some releases as it goes back
        if (i == 2 || i == 14 || i == 32)
            inc++;
    }
    return 0;
}

int Bisect::init_releases_phase(const Environment &env, Git &linux_git)
{
    // We can get here after first or second anchor test.
    Version anchor = good_version.name.empty() ? bisect_version : good_version;

    // Gather linux release tags, dates, and hashes
    releases = gather_release_tags(env, linux_git);
    if (releases.size() <= 0)
    {
        std::cerr << "Error: Failed to get Linux release tags.\n" << std::flush;
        return -1;
    }

    // Using commit date may be flakey here, so use git.
    int i = find_first_ancestor(linux_git, anchor.name, releases);
    if (i < 0)
    {
        std::cerr << "Error: Failed to find first ancestor tag.\n" << std::flush;
        return -1;
    }

    releases = std::vector<Version>(releases.begin() + i, releases.end());
    skip_tags(releases);
    if (releases.size() <= 0)
        return -1;
    
    // Set index to -1 here so it is 0 when we increment the first time.
    index = -1;
    return 0;
}

int Bisect::init_bisect_phase(Git &linux_git)
{
    git_stop = false;
    // Switch over to git bisect for this phase
    std::string bad = bisect_version.name;
    std::string good = good_version.name;
    linux_git.cleanup();
    linux_git.bisect_reset();
    if (linux_git.error() < 0)
        return -1;
    linux_git.bisect_start(bad, good);
    if (linux_git.error() < 0)
    {
        std::cerr << "Error: git bisect start " << bad << " " << good << " failed\n" << std::flush;
        return -1;
    }

    return 0;
}

int Bisect::next_phase(const Bisect_Phase next, const Environment &env, Git &linux_git)
{
    int err = 0;
    phase = next;

    switch (phase)
    {
    case Bisect_Anchor:
        err = init_anchor_phase(linux_git);
        break;
    case Bisect_Releases:
        err = init_releases_phase(env, linux_git);
        break;
    case Bisect_Kernel:
        err = init_bisect_phase(linux_git);
        break;
    default:
        err = -1;
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

int Bisect::build_current_kernel(const Environment &env, Git &linux_git, bool bisecting)
{
    std::string compiler;
    compiler = get_compiler(gcc_versions, current_session.kernel.date, env);
    log_session_compiler(compiler);
    return prep_kernel(env, linux_git, current_session.kernel, compiler, bisecting);
}

int Bisect::goto_anchor_session(const Environment &env, Git &linux_git)
{
    int err = 0;
    if (linux_git.cleanup() < 0)
        return -1;

    // Choose the anchor commit if this is the first round, or good versino if second round
    Version linux_version = good_version.name.empty() ? anchor_version : good_version;

    current_session = Session(linux_version, mode(), false);
    log_session_info(this_session(), inc_session());

    err = build_current_kernel(env, linux_git);
    if (err < 0)
    {
        log_kernel_build_error();
        return -1;
    }

    return 0;
}

int Bisect::goto_release_session(const Environment &env, Git &linux_git)
{
    int err = 0;
    Version linux_version;

retry:
    index++;
    if (index >= releases.size() || (index > 0 && !last_session.found && last_session.stable))
    {
        return 1;
    }
    else if (index < 0)
    {
        std::cerr << "Error: index < 0 in goto_release_session_ff\n" << std::flush;
        return -1;
    }

    linux_version = releases.at(index);
    current_session = Session(linux_version, mode(), false);
    log_session_info(current_session, inc_session());

    // May be no need to rebuild kernel version
    if (last_session.kernel.name == current_session.kernel.name)
        return err;

    err = build_current_kernel(env, linux_git);
    if (err < 0)
    {
        log_kernel_build_error();
        std::cout << "Attempting to recover.\n" << std::flush;
        current_session.stable = false;
        _archive_session();
        goto retry;
    }

    return err;
}

int Bisect::goto_bisect_session(const Environment &env, Git &linux_git)
{
    int err = 0;
    if (git_stop)
        return 1;

retry:
    // For git bisect, we should already be at the next commit to test
    current_session = Session(linux_git.get_current_version(), mode(), false);
    if (linux_git.error() < 0)
        return -1;
    log_session_info(current_session, inc_session());

    err = build_current_kernel(env, linux_git, true);
    if (err < 0)
    {
        log_kernel_build_error();
        std::cout << "Attempting to recover.\n" << std::flush;
        current_session.stable = false;
        _archive_session();
        linux_git.cleanup();
        err = linux_git.bisect_skip();
        // TODO: Fix this return value nonsense to better handle multiple guilty commits
        if (err == -3)
            std::cout << "Git bisect reported multiple guilty commits\n" << std::flush;
        if (err <= -2)
            return 1;
        goto retry;
    }

    return err;
}

// Goto the next session based on the internal state. Called functions should:
// update internal indices as needed
// build kernel and syzkaller
// return 0 to continue same phase, return 1 to indicate phase is done.
int Bisect::next_session(const Environment &env, Git &linux_git)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Anchor:
        err = goto_anchor_session(env, linux_git);
        break;
    case Bisect_Releases:
        err = goto_release_session(env, linux_git);
        break;
    case Bisect_Kernel:
        err = goto_bisect_session(env, linux_git);
        break;
    default:
        err = -1;
    }
    return err;
}

std::string viable_repro_name(const Environment &env)
{
    std::string base_filename = "repro-" + env.working_name + "-";
    std::string filename;
    int n;
    for (n = 0; n < 1000; n++)
    {
        filename = env.reprodir + base_filename + std::to_string(n) + ".prog";
        if (!check_file(filename))
            break;
    }

    if (n >= 1000)
        std::cerr << "Warning: Ran out of PoC filenames!\n" << std::flush;

    return filename;
}

std::string find_crash_log(const Environment &env)
{
    std::string crash_name;
    std::vector<std::string> crash_hashes = list_dir(env.syzwd + "/crashes");
    for (std::string hash : crash_hashes)
    {
        crash_name = get_crash_name(hash);
        if (fuzz_is_crash_in(crash_name, env.duplicates))
            return hash + "/log0";
    }

    std::cerr << "Error: Failed to find expected syzkaller crash for syz-repro.\n" << std::flush;
    return "";
}

int Bisect::do_syz_repro(Environment &env)
{
    std::string prog = viable_repro_name(env);
    std::string crash_log = find_crash_log(env);
    if (crash_log.empty())
        return -1;
    std::cout << "Running syz-repro.\n" << std::flush;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    if (run_syz_repro(env, prog, crash_log))
    {
        std::cout << "New PoC saved at " << prog << std::endl << std::flush;
        env.primary_repro = prog;
        uniqify_reproducers(env);
        std::cout << "Primary PoC:     " << env.primary_repro << std::endl;
        defer_repro = false;
    }
    else
        defer_repro = true;

    std::cout << "Run Time:        " << runtime(start) << std::endl << std::flush;
    return 0;
}

Test_Result Bisect::test_anchor_ff(Environment &env)
{
    Test_Result result = fuzz_loop_finding(env);
    return result;
}

Test_Result Bisect::test_anchor_poc(Environment &env)
{
    Test_Result result = poc_loop(env);
    return result;
}

Test_Result Bisect::test_anchor(Environment &env)
{
    Test_Result result;
    if (mode() == Mode_FF)
    {
        result = test_anchor_ff(env);
        env.max_time = (env.safe_mode ? env.max_time : result.suggest_ttf);
    }
    else if (mode() == Mode_PoC)
    {
        result = test_anchor_poc(env);
    }

    log_session_result(result);
    return result;
}

Test_Result Bisect::test_bisect_ff(Environment &env)
{
    Test_Result result = fuzz_loop(env);
    if (result.found && (repro_count % REPRO_FREQ == 0 || defer_repro))
        do_syz_repro(env);

    repro_count += result.found && !defer_repro ? 1 : 0;
    return result;
}

Test_Result Bisect::test_bisect_poc(Environment &env)
{
    Test_Result result = poc_loop(env);
    return result;
}

Test_Result Bisect::test_bisect(Environment &env)
{
    Test_Result result;
    if (mode() == Mode_FF)
    {
        result = test_bisect_ff(env);
    }
    else if (mode() == Mode_PoC)
    {
        result = test_bisect_poc(env);
    }

    log_session_result(result);
    if (check_safe_mode(result, env.safe_mode, env.max_time, env.fuzztimes))
        log_safe_mode(env.max_time, env.fuzztimes);

    return result;
}

Test_Result Bisect::test_current(Environment &env, Git &linux_git)
{
    Test_Result res;

retry:
    switch (phase)
    {
    case Bisect_Anchor:
        res = test_anchor(env);
        break;
    case Bisect_Releases:
    case Bisect_Kernel:
        res = test_bisect(env);
        break;
    default:
        break;
    }

    if (res.retry && env.try_patch)
    {
        // In the case a blocking bug is found and removed, rebuild and go again
        log_session_info(current_session, inc_session());
        build_current_kernel(env, linux_git);
        goto retry;
    }

    return res;
}

// Set good_version to be the parent of the bisect_commit
int Bisect::set_good_version(Git &linux_git)
{
    std::string parent = linux_git.get_first_parent(bisect_version.name);
    Date parent_date = linux_git.get_commit_date(parent);
    good_version = Version(parent, parent_date);
    return 0;
}

int Bisect::record_kernel(const Test_Result &result, Git &linux_git)
{
    if (!already_fuzzed(current_session))
        archive_session(result);
    
    int res = 0;
    if (result.found)
    {
        linux_git.cleanup();
        res = linux_git.bisect_bad();
        bisect_version = current_session.kernel;
    }
    else if (result.stable)
    {
        linux_git.cleanup();
        res = linux_git.bisect_good();
        good_version = current_session.kernel;
    }
    else if (!result.stable)
    {
        linux_git.cleanup();
        res = linux_git.bisect_skip();
    }
    if (res <= -2)
        git_stop = true;

    return res;
}

int Bisect::record_release(const Test_Result &result)
{
    if (!already_fuzzed(current_session))
        archive_session(result);
    
    if (result.found)
    {
        bisect_version = current_session.kernel;
    }
    else if (result.stable)
        good_version = current_session.kernel;
    
    return 0;
}

int Bisect::record_anchor(const Test_Result &result)
{
    bisect_version = current_session.kernel;
    archive_session(result);
    return 0;
}

int Bisect::record(const Test_Result &result, Git &linux_git)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Anchor:
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

std::string Bisect::print_anchor_fail(const Environment &env, const std::chrono::steady_clock::time_point &start,
                                        const std::chrono::steady_clock::time_point &stage_start, const std::string &stage_title, const std::string &repro) const
{
    // TODO: iomanip this
    std::stringstream ss;
    std::string title = "==== Partial Result" + (!stage_title.empty() ? ": " + stage_title : "") + " ====";
    ss << "\n" << title << "\n";
    if (!repro.empty())
        ss << "Reproducer:           " << repro << "\n";
    ss << "Bisection Result:     Failed to reproduce at the anchor commit.\n";
    ss << "Stage Time:           " << runtime(stage_start) << "\n";
    ss << "Run Time:             " << runtime(start) << "\n";
    ss << std::setw(title.size()) << std::setfill('=') << "=" << "\n";

    return ss.str();
}

std::string Bisect::print_partial_result(const Environment &env, Git &linux_git, const std::chrono::steady_clock::time_point &start,
                                        const std::chrono::steady_clock::time_point &stage_start, const std::string &stage_title, const std::string &repro) const
{
    // TODO: iomanip this
    std::stringstream ss;
    std::string title = "==== Partial Result" + (!stage_title.empty() ? ": " + stage_title : "") + " ====";
    ss << "\n" << title << "\n";
    if (!repro.empty())
        ss << "Reproducer:           " << repro << "\n";
    if (!bisect_version.name.empty())
    {
        ss << "Bisection Result:     " << bisect_version.date.get_date() << " - " << bisect_version.name << "\n";
        ss << "Bisected Commit Name: " << linux_git.get_commit_name(bisect_version.name) << "\n";
    }
    ss << "Stage Time:           " << runtime(stage_start) << "\n";
    ss << "Run Time:             " << runtime(start) << "\n";
    ss << std::setw(title.size()) << std::setfill('=') << "=" << "\n";

    return ss.str();
}

std::string Bisect::print_result(const Environment &env, Git &linux_git, const std::chrono::steady_clock::time_point &start) const
{
    // TODO: iomanip this
    std::stringstream ss;
    ss << "Bug Name:             " << env.name << "\n";
    ss << "Bug Link:             " << env.buglink << "\n";
    if (!bisect_version.name.empty())
    {
        ss << "Bisection Result:     " << bisect_version.date.get_date() << " - " << bisect_version.name << "\n";
        ss << "Bisected Commit Name: " << linux_git.get_commit_name(bisect_version.name) << "\n";
    }
    else
    {
        ss << "Bisection Result:     Failed to bisect the bug.\n";
    }
    ss << "Run Time:             " << runtime(start) << "\n\n";

    ss << "Anchor Commit:        " << anchor_version.date.get_date() << " - " << anchor_version.name << "\n";
    ss << "Bisection Result:     " << bisect_version.date.get_date() << " - " << bisect_version.name << "\n";

    return ss.str();
}

std::string runtime(const std::chrono::steady_clock::time_point &start, const std::chrono::steady_clock::time_point &end)
{
    std::stringstream pretty;
    auto diff = end - start;

    auto hours = std::chrono::duration_cast<std::chrono::hours>(diff);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(diff - hours);
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(diff - hours - minutes);

    pretty << hours.count() << "h" << minutes.count() << "m" << seconds.count() << "s";

    return pretty.str();
}

std::string runtime(const std::chrono::steady_clock::time_point &start)
{
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    return runtime(start, end);
}

int uniqify_reproducers(Environment &env)
{
    std::vector<std::string> repros = list_dir(env.reprodir);
    if (repros.size() <= 0)
    {
        std::cerr << "Error: No reproducer files found.\n" << std::flush;
        return -1;
    }

    std::vector<std::string> keep, remove;
    bool removed = false;
    bool saved_primary = false;
    keep.push_back(repros.front());
    for (int i = 1; i < repros.size(); i++)
    {
        removed = false;
        for (std::string kept : keep)
        {
            if (compare_files(kept, repros.at(i)))
            {
                if (!saved_primary && !env.primary_repro.empty()
                    && repros.at(i).find(env.primary_repro) != std::string::npos)
                {
                    saved_primary = true;
                    env.primary_repro = kept;
                }
                remove.push_back(repros.at(i));
                removed = true;
                break;
            }
        }
        if (!removed)
            keep.push_back(repros.at(i));
    }

    for (std::string r : remove)
        remove_file(r);

    return 0;
}

void log_safe_mode(int max_time, int fuzztimes)
{
    std::cout << "Switching to Safe Mode: Fuzzing " << fuzztimes << " times at " << max_time << " minutes\n";
}

void set_safe_mode(bool &safe_mode, unsigned int &max_time, unsigned int &fuzztimes)
{
    safe_mode = true;
    fuzztimes = MAX_FUZZ_TIMES;
    max_time = max_time > DEFAULT_MAX_TIME ? max_time : DEFAULT_MAX_TIME;
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
              << std::left << std::setw(SESHW) << "Session:" << count << "\n"
              << std::left << std::setw(SESHW) << "Kernel:" << session.kernel.string() << "\n" << std::flush;
}

void log_session_compiler(const std::string &compiler)
{
    std::cout << std::left << std::setw(SESHW) << "Compiler:" << compiler << "\n" << std::flush;
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

void log_attempt_result_poc(const Syzkaller_Result &attempt, int i, const std::vector<std::string> &dups)
{
    std::cout << "Attempt " << i << ":\n";

    if (attempt.reports.size() == 0)
        std::cout << "    No crashes found.\n" << std::flush;
    
    for (Crash_Report cr : attempt.reports)
        std::cout << (fuzz_is_crash_in(cr.name, dups) ? "*** " : "    ") << cr.name << std::endl << std::flush;
}

void log_session_result(const Test_Result &result)
{
    std::cout << "The bug was " << (result.found ? "" : "not ") << "found.\n" << std::flush;

    if (!result.stable)
        std::cout << "This commit is flagged as unstable.\n" << std::flush;
}
