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
    lock_syz = false;

    gather_compiler_versions(env);

    finding_version.name = bug.find_hash;
    finding_version.date = linux_git.get_commit_date(bug.find_hash);
    
    // Gather linux release tags, dates, and hashes
    releases = gather_release_tags(env, linux_git);

    if (bug.have_fdate)
        find_date = bug.find_date;
    else
        find_date = finding_version.date;

    last_session.kernel.name.clear();
    last_session.syzkaller.name.clear();

    return 0;
}

int Bisect::set_algorithm(const std::string &algstr)
{
    if (algstr == "focused-fuzz-stateful")
        alg = ALG_FF_STATEFUL;
    else if (algstr == "focused-fuzz-clean")
        alg = ALG_FF_CLEAN;
    else if (algstr == "poc-ff-backup")
        alg = ALG_BISECT_FF;
    else if (algstr == "syz-bisect")
        alg = ALG_SYZ_BISECT;
    else if (algstr == "setup-only")
        alg = ALG_SETUP;
    else if (algstr == "finding-only")
        alg = ALG_FINDING;
    else
        return -1;
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
    case Bisect_Syzkaller:
        return right - left;
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

int Bisect::init_releases_phase_ff(Git &syzkaller_git)
{
    // We can get here by the first release search, or after syzkaller version is known.
    Version anchor = lock_syz ? bisect_session.kernel : finding_version;

    // Cut out any releases from after the finding date. This helps line up the index.
    int i = 0;
    for (i = 0; i < releases.size() && releases.at(i).date > anchor.date; i++);
    releases = std::vector<Version>(releases.begin() + i, releases.end());
    if (releases.size() <= 0)
        return -1;
    // Set index to -1 here so it is 0 when we increment the first time.
    index = -1;

    Version syzkaller_version = lock_syz ? locked_syzkaller : syzkaller_git.get_version_by_date_raw(anchor.date);
    bisect_session = Session(anchor, syzkaller_version, true);
    bisect_index = -1;
    return 0;
}

int Bisect::init_syzkaller_phase(const Environment &env, Git &linux_git, Git &syzkaller_git)
{
    // We are bisecting syzkaller. locking the version is impossible.
    lock_syz = false;
    // Coming off of a release search, we need to grab the kernel and syzkaller versions in between
    std::string new_hash = bisect_index >= 0 ? releases.at(bisect_index).name : finding_version.name;
    std::string old_hash = releases.at(bisect_index + 1).name;
    kernel_versions = get_kernel_versions(env, linux_git, old_hash, new_hash);

    Date new_date = linux_git.get_commit_date(new_hash);
    Date old_date = linux_git.get_commit_date(old_hash);
    if (old_date < SYZBOT_BEGIN_DATE)
        old_date = SYZBOT_BEGIN_DATE;
    syzkaller_git.checkout("master");
    new_hash = syzkaller_git.get_commit_by_date_raw(new_date);
    old_hash = syzkaller_git.get_commit_by_date_raw(old_date);
    syzkaller_versions = get_syzkaller_versions(env, syzkaller_git, old_hash, new_hash);

    if (kernel_versions.size() <= 1 || syzkaller_versions.size() <= 1)
    {
        std::cerr << "Error: Failed to grab kernel and/or syzkaller versions\n" << std::flush;
        return -1;
    }

    // bisect session is the one where the bug reproduces, which carries over from the last phase

    // right is the older date (lower date). higher index
    // left is the recent date (higher date). lower index
    right = syzkaller_versions.size() - 1;
    left = 1; // we know the bug reproduces at sykaller_versions[0]
    index = 0;

    return (right >= 0 ? 0 : -1);
}

int Bisect::init_kernel_phase(Git &linux_git)
{
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
    Version anchor = lock_syz ? bisect_session.kernel : finding_version;

    // Cut out any releases from after the finding date. This helps line up the index.
    int i = 0;
    for (i = 0; i < releases.size() && releases.at(i).date > anchor.date; i++);
    releases = std::vector<Version>(releases.begin() + i, releases.end());
    if (releases.size() <= 0)
        return -1;
    // Set index to -1 here so it is 0 when we increment the first time.
    index = -1;

    Version syzkaller_version = lock_syz ? locked_syzkaller : syzkaller_git.get_version_by_date_raw(finding_version.date);
    bisect_session = Session(anchor, syzkaller_version, true);
    bisect_index = -1;
    lock_syz = true;
    return 0;
}

int Bisect::next_phase_ff(const Environment &env, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Init:
    case Bisect_Finding:
        break;
    case Bisect_Releases:
        err = init_releases_phase_ff(syzkaller_git);
        break;
    case Bisect_Syzkaller:
        err = init_syzkaller_phase(env, linux_git, syzkaller_git);
        break;
    case Bisect_Kernel:
        err = init_kernel_phase(linux_git);
        break;
    case Bisect_Done:
        break;
    default:
        err = -1;
        break;
    }
    return err;
}

int Bisect::next_phase_poc(const Environment &env, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Init:
    case Bisect_Finding:
        break;
    case Bisect_Releases:
        err = init_releases_phase_poc(syzkaller_git);
        break;
    case Bisect_Kernel:
        err = init_kernel_phase(linux_git);
        break;
    case Bisect_Syzkaller:
        break;
    case Bisect_Done:
        break;
    default:
        err = -1;
        break;
    }
    return err;
}

int Bisect::next_phase(const Bisect_Phase next, const Environment &env, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    phase = next;
    switch(alg)
    {
    case ALG_FF_CLEAN:
    case ALG_FF_STATEFUL:
        err = next_phase_ff(env, linux_git, syzkaller_git);
        break;
    case ALG_BISECT_FF:
    case ALG_SYZ_BISECT:
        err = next_phase_poc(env, linux_git, syzkaller_git);
        break;
    default:
        err = -1;
        break;
    }

    return err;
}

int Bisect::skip_syzkaller(const std::string &hash, Git &syzkaller_git)
{
    if (phase != Bisect_Finding)
        return -1;
    phase = Bisect_Releases;
    lock_syz = true;
    locked_syzkaller = Version(hash, syzkaller_git.get_commit_date(hash));
    return 0;
}

int Bisect::lock_syzkaller()
{
    lock_syz = true;
    locked_syzkaller = bisect_session.syzkaller;
    return 0;
}

int Bisect::next_stable_binary_syzkaller()
{
    middle = (left + right) / 2;
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

int Bisect::build_current_kernel(std::ofstream &logfile, const Environment &env, const Bug_Info &bug, Git &linux_git)
{
    std::string compiler;
    std::cout << SPACER
         << "Prepping the kernel\n";
    compiler = get_compiler(gcc_versions, clang_versions, current_session.kernel.date, env);
    log_session_compiler(logfile, compiler);
    return prep_kernel(env, bug, linux_git, current_session.kernel, compiler);
}

int Bisect::build_current_syzkaller(const Environment &env, const Bug_Info &bug, Git &syzkaller_git, bool do_slim)
{
    std::cout << SPACER
        << "Prepping Syzkaller\n";
    return prep_syzkaller(env, bug, syzkaller_git, current_session.syzkaller, do_slim);
}

int Bisect::goto_finding_session(std::ofstream &logfile, const Environment &env, const Bug_Info &bug, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    Version linux_version, syzkaller_version;

    if (linux_git.cleanup() < 0 || syzkaller_git.cleanup() < 0)
        return -1;

    // Fetch the finding commit
    linux_version = finding_version;

    // Get latest syzkaller version for that date
    syzkaller_version = syzkaller_git.get_version_by_date_raw(linux_version.date);
    if (syzkaller_git.error() < 0)
    {
        std::cerr << "Error: Failed to read Syzkaller commit version by date\n" << std::flush;
        return -1;
    }

    current_session = Session(linux_version, syzkaller_version, false);
    log_session_info(logfile, this_session(), inc_session());

    err = build_current_kernel(logfile, env, bug, linux_git);
    if (err < 0)
    {
        log_kernel_build_error(logfile);
        return -1;
    }

    bool do_slim = (alg == ALG_FF_STATEFUL || alg == ALG_FF_CLEAN);
    err = build_current_syzkaller(env, bug, syzkaller_git, do_slim);
    if (err < 0)
    {
        log_syzkaller_build_error(logfile);
        return -1;
    }

    return 0;
}

int Bisect::goto_release_session_ff(std::ofstream &logfile, const Environment &env, const Bug_Info &bug, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    Version linux_version, syzkaller_version;
    Date old_date;

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
    old_date = linux_version.date > SYZBOT_BEGIN_DATE ? linux_version.date : SYZBOT_BEGIN_DATE;
    syzkaller_version = lock_syz ? locked_syzkaller : syzkaller_git.get_version_by_date_raw(old_date);
    current_session = Session(linux_version, syzkaller_version, false);
    log_session_info(logfile, current_session, inc_session());

    if (!already_fuzzed(this_session()))
    {
        // May be no need to rebuild kernel version
        if (last_session.kernel.name != current_session.kernel.name)
        {
            err = build_current_kernel(logfile, env, bug, linux_git);
            if (err < 0)
            {
                log_kernel_build_error(logfile);
                logfile << "Attempting to recover.\n" << std::flush;
                current_session.stable = false;
                _archive_session();
                goto retry;
            }
        }

        // May be no need to rebuild syzkaller
        if (last_session.syzkaller.name != current_session.syzkaller.name)
        {
            err = build_current_syzkaller(env, bug, syzkaller_git);
            if (err < 0)
            {
                log_syzkaller_build_error(logfile);
                return -1;
            }
        }
    }

    return err;
}

int Bisect::goto_syzkaller_session_ff(std::ofstream &logfile, const Environment &env, const Bug_Info &bug, Git &linux_git, Git &syzkaller_git)
{
    if (left > right)
        return 1;
    
    int err = 0;
    Version linux_version, syzkaller_version;

    next_stable_binary();
    syzkaller_version = syzkaller_versions.at(middle);
    index = middle;
retry_kernel:
    linux_version = get_stable_version_by_date(kernel_versions, syzkaller_version.date);
    current_session = Session(linux_version, syzkaller_version, false);
    log_session_info(logfile, current_session, inc_session());

    if (!already_fuzzed(this_session()))
    {
        // There's a chance the kernel is already built. Take advantage of this.
        if (last_session.kernel != current_session.kernel)
        {
            err = build_current_kernel(logfile, env, bug, linux_git);
            if (err < 0)
            {
                log_kernel_build_error(logfile);
                logfile << "Attempting to recover.\n" << std::flush;
                current_session.stable = false;
                _archive_session();
                goto retry_kernel;
            }
        }

        err = build_current_syzkaller(env, bug, syzkaller_git);
        if (err < 0)
        {
            log_syzkaller_build_error(logfile);
            return -1;
        }
    }
    return err;
}

int Bisect::goto_kernel_session_ff(std::ofstream &logfile, const Environment &env, const Bug_Info &bug, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    if (git_stop)
        return 1;

retry:
    // For git bisect, we should already be at the next commit to test
    current_session = Session(linux_git.get_current_version(), locked_syzkaller, false);
    log_session_info(logfile, current_session, inc_session());

    if (!already_fuzzed(this_session()))
    {
        err = build_current_kernel(logfile, env, bug, linux_git);
        if (err < 0)
        {
            log_kernel_build_error(logfile);
            logfile << "Attempting to recover.\n" << std::flush;
            current_session.stable = false;
            _archive_session();
            linux_git.cleanup();
            if (linux_git.bisect_skip() == -2)
                return 1;
            goto retry;
        }

        // if syzkaller version is locked, no need to build it all the time
        if (last_session.syzkaller.name != current_session.syzkaller.name && last_session.stable)
        {
            err = build_current_syzkaller(env, bug, syzkaller_git);
            if (err < 0)
            {
                log_syzkaller_build_error(logfile);
                return -1;
            }
        }
    }

    return err;
}

int Bisect::goto_release_session_poc(std::ofstream &logfile, const Environment &env, const Bug_Info &bug, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    Version linux_version;

retry:
    index++;
    if (index >= releases.size() || (!last_session.found && last_session.stable))
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
    current_session = Session(linux_version, locked_syzkaller, false);
    log_session_info(logfile, current_session, inc_session());

    if (!already_fuzzed(this_session()) && last_session.kernel.name != current_session.kernel.name)
    {
        err = build_current_kernel(logfile, env, bug, linux_git);
        if (err < 0)
        {
            log_kernel_build_error(logfile);
            logfile << "Attempting to recover.\n" << std::flush;
            current_session.stable = false;
            _archive_session();
            goto retry;
        }
    }

    return err;
}

int Bisect::goto_kernel_session_poc(std::ofstream &logfile, const Environment &env, const Bug_Info &bug, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    if (git_stop)
        return 1;

retry:
    // For git bisect, we should already be at the next commit to test
    current_session = Session(linux_git.get_current_version(), locked_syzkaller, false);
    log_session_info(logfile, current_session, inc_session());

    if (!already_fuzzed(this_session()))
    {
        err = build_current_kernel(logfile, env, bug, linux_git);
        if (err < 0)
        {
            log_kernel_build_error(logfile);
            logfile << "Attempting to recover.\n" << std::flush;
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

int Bisect::goto_syzkaller_session_poc(std::ofstream &logfile, const Environment &env, const Bug_Info &bug, Git &linux_git, Git &syzkaller_git)
{   
    int err = 0;
    Version linux_version, syzkaller_version;

    linux_version = good_session.kernel;
    syzkaller_version = syzkaller_git.get_version_by_date_raw(linux_version.date);
    current_session = Session(linux_version, syzkaller_version, false);
    log_session_info(logfile, current_session, inc_session());

    if (!already_fuzzed(this_session()))
    {
        // There's a chance the kernel is already built. Take advantage of this.
        if (last_session.kernel != current_session.kernel)
        {
            err = build_current_kernel(logfile, env, bug, linux_git);
            if (err < 0)
            {
                log_kernel_build_error(logfile);
                return -1;
            }
        }

        err = build_current_syzkaller(env, bug, syzkaller_git);
        if (err < 0)
        {
            log_syzkaller_build_error(logfile);
            return -1;
        }
    }
    return err;
}

int Bisect::next_session_ff(std::ofstream &outf, const Environment &env, const Bug_Info &bug, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Releases:
        err = goto_release_session_ff(outf, env, bug, linux_git, syzkaller_git);
        break;
    case Bisect_Syzkaller:
        err = goto_syzkaller_session_ff(outf, env, bug, linux_git, syzkaller_git);
        break;
    case Bisect_Kernel:
        err = goto_kernel_session_ff(outf, env, bug, linux_git, syzkaller_git);
        break;
    default:
        err = -1;
        break;
    }
    return err;
}

int Bisect::next_session_poc(std::ofstream &outf, const Environment &env, const Bug_Info &bug, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    switch(phase)
    {
    case Bisect_Releases:
        err = goto_release_session_poc(outf, env, bug, linux_git, syzkaller_git);
        break;
    case Bisect_Kernel:
        err = goto_kernel_session_poc(outf, env, bug, linux_git, syzkaller_git);
        break;
    case Bisect_Syzkaller:
        err = goto_syzkaller_session_poc(outf, env, bug, linux_git, syzkaller_git);
        break;
    default:
        err = -1;
        break;
    }
    return err;
}

// Goto the next session based on the internal state. Called functions should:
// update internal indices as needed
// build kernel and syzkaller
// return 0 to continue same phase, return 1 to indicate phase is done.
int Bisect::next_session(std::ofstream &outf, const Environment &env, const Bug_Info &bug, Git &linux_git, Git &syzkaller_git)
{
    int err = 0;
    if (phase == Bisect_Finding)
        return goto_finding_session(outf, env, bug, linux_git, syzkaller_git);

    // switch on algorithm, then each algorithm handles its own phases
    switch(alg)
    {
    case ALG_FF_CLEAN:
    case ALG_FF_STATEFUL:
        err = next_session_ff(outf, env, bug, linux_git, syzkaller_git);
        break;
    case ALG_BISECT_FF:
    case ALG_SYZ_BISECT:
        err = next_session_poc(outf, env, bug, linux_git, syzkaller_git);
        break;
    default:
        err = -1;
    }
    return err;
}

Test_Result Bisect::test_finding_ff(std::ofstream &logfile, Environment &env, Bug_Info &bug)
{
    Test_Result result = fuzz_loop_finding(logfile, env, bug, current_session.syzkaller.date, alg == ALG_FF_STATEFUL);
    env.max_time = (env.safe_mode ? env.max_time : result.suggest_ttf);
    log_session_result(logfile, result, bug.duplicates);
    bug.blocking_bugs.count_blocking_bugs(result);
    return result;
}

Test_Result Bisect::test_syzkaller(std::ofstream &logfile, Environment &env, Bug_Info &bug)
{
    Test_Result result;
    std::cout << SPACER;
    result = fuzz_loop(logfile, env, bug, current_session.syzkaller.date, false);
    log_session_result(logfile, result, bug.duplicates);
    bug.blocking_bugs.count_blocking_bugs(result);
    if (check_safe_mode(result, env.safe_mode, env.max_time, env.fuzztimes))
        log_safe_mode(logfile, env.max_time, env.fuzztimes);

    return result;
}

Test_Result Bisect::test_kernel_ff(std::ofstream &logfile, Environment &env, Bug_Info &bug)
{
    Test_Result result;
    std::cout << SPACER;
    result = fuzz_loop(logfile, env, bug, current_session.syzkaller.date, alg == ALG_FF_STATEFUL);
    log_session_result(logfile, result, bug.duplicates);
    bug.blocking_bugs.count_blocking_bugs(result);
    if (env.try_patch && patch_blocking_bugs(result, bug) > 0)
    {
        result.retry = true;
        logfile << "Attempted to patch one or more blocking bugs. Retrying\n" << std::flush;
    }
    if (check_safe_mode(result, env.safe_mode, env.max_time, env.fuzztimes))
        log_safe_mode(logfile, env.max_time, env.fuzztimes);
    return result;
}

Test_Result Bisect::test_current_ff(std::ofstream &outf, Environment &env, Bug_Info &bug)
{
    Test_Result res;
    switch(phase)
    {
    case Bisect_Finding:
        res = test_finding_ff(outf, env, bug);
        break;
    case Bisect_Releases:
        res = test_kernel_ff(outf, env, bug);
        break;
    case Bisect_Syzkaller:
        res = test_syzkaller(outf, env, bug);
        break;
    case Bisect_Kernel:
        res = test_kernel_ff(outf, env, bug);
        break;
    default:
        break;
    }
    return res;
}

Test_Result Bisect::test_finding_poc(std::ofstream &logfile, Environment &env, Bug_Info &bug)
{
    Test_Result result = repro_loop_finding(logfile, env, bug, current_session.syzkaller.date);
    env.max_time = (env.safe_mode ? env.max_time : result.suggest_ttf);
    log_session_result(logfile, result, bug.duplicates);
    bug.blocking_bugs.count_blocking_bugs(result);
    return result;
}

Test_Result Bisect::test_kernel_poc(std::ofstream &logfile, Environment &env, Bug_Info &bug)
{
    Test_Result result;
    std::cout << SPACER;
    result = repro_loop(logfile, env, bug, current_session.syzkaller.date);
    log_session_result(logfile, result, bug.duplicates);
    bug.blocking_bugs.count_blocking_bugs(result);
    if (env.try_patch && patch_blocking_bugs(result, bug) > 0)
    {
        result.retry = true;
        logfile << "Attempted to patch one or more blocking bugs. Retrying\n" << std::flush;
    }
    if (check_safe_mode(result, env.safe_mode, env.max_time, env.fuzztimes))
        log_safe_mode(logfile, env.max_time, env.fuzztimes);
    return result;
}

Test_Result Bisect::test_current_poc(std::ofstream &outf, Environment &env, Bug_Info &bug)
{
    Test_Result res;
    switch(phase)
    {
    case Bisect_Finding:
        res = test_finding_poc(outf, env, bug);
        break;
    case Bisect_Releases:
    case Bisect_Kernel:
        res = test_kernel_poc(outf, env, bug);
        break;
    case Bisect_Syzkaller:
        res = test_syzkaller(outf, env, bug);
        break;
    default:
        break;
    }
    return res;
}

Test_Result Bisect::test_current(std::ofstream &logfile, Environment &env, Bug_Info &bug, Git &linux_git)
{
    Test_Result res;

    if (!already_fuzzed(this_session()))
    {
retry:
        switch (alg)
        {
        case ALG_FF_CLEAN:
        case ALG_FF_STATEFUL:
            res = test_current_ff(logfile, env, bug);
            break;
        case ALG_BISECT_FF:
        case ALG_SYZ_BISECT:
            res = test_current_poc(logfile, env, bug);
            break;
        default:
            break;
        }

        if (res.retry && env.try_patch)
        {
            // In the case a blocking bug is found and removed, rebuild and go again
            log_session_info(logfile, current_session, inc_session());
            build_current_kernel(logfile, env, bug, linux_git);
            goto retry;
        }
    }
    else
    {
        std::cout << "This session has already been fuzzed. Skipping.\n";
        res.found = session_was_found(this_session()) == 1 ? true : false;
        res.stable = session_was_stable(this_session()) == 1 ? true : false;
        logfile << "The bug was " << (res.found ? "found " : "not found ") << "in a previous identical fuzzing session.\n" << std::flush;
    }

    return res;
}

int Bisect::record_syzkaller(const Test_Result &result)
{
    if (!already_fuzzed(current_session))
        archive_session(result);

    if (result.found)
    {
        // This will definitely be wonky corrupted for other algorithms, but it doesn't matter
        left = middle + 1;
        bisect_session = current_session;
        bisect_index = index;
    }
    else
    {
        right = middle - 1;
        good_session = current_session;
    }
    return 0;
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

int Bisect::record_finding(const Test_Result &result)
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
        err = record_finding(result);
        break;
    case Bisect_Releases:
        err = record_release(result);
        break;
    case Bisect_Syzkaller:
        err = record_syzkaller(result);
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
    switch(phase)
    {
    case Bisect_Releases:
        releases.at(index).skipped = !current_session.stable && !current_session.found;
        break;
    case Bisect_Kernel:
    case Bisect_Syzkaller:
        kernel_versions.at(index).skipped = !current_session.stable && !current_session.found;
        break;
    default:
        break;
    }
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

std::string Bisect::print_result(const Environment &env, const Bug_Info &bug, Git &linux_git, const std::string &start) const
{
    // TODO: iomanip this
    std::stringstream ss;
    ss << "Bug Name:             " << bug.name << "\n";
    ss << "Bug Link:             " << bug.buglink << "\n";
    ss << "Bisection Result:     " << bisect_session.kernel.date.get_date() << " - " << bisect_session.kernel.name << "\n";
    ss << "Bisected Commit Name: " << linux_git.get_commit_name(bisect_session.kernel.name) << "\n";
    ss << "Run Time:             " << chomp(start) << " - " << chomp(date("%Y-%m-%d %T")) << "\n";
    ss << "Arch:                 " << bug.arch << "\n\n";

    ss << "Finding Date:         " << find_date.get_date() << "\n";
    ss << "Finding Commit:       " << finding_version.date.get_date() << " - " << finding_version.name << "\n";
    ss << "Bisection Result:     " << bisect_session.kernel.date.get_date() << " - " << bisect_session.kernel.name << "\n";

    return ss.str();
}

void log_safe_mode(std::ofstream &logfile, int max_time, int fuzztimes)
{
    std::cout << "Switching to Safe Mode: Fuzzing " << fuzztimes << " times at " << max_time << " minutes\n";
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

std::string get_datetime()
{
    return date("%Y-%m-%d %T");
}

void log_datetime(std::ofstream &logfile)
{
    logfile << get_datetime();
}

void log_session_info(std::ofstream &logfile, const Session &session, const int count)
{
    // date puts an endline there on its own
    logfile << "\n" << get_datetime() << ""
            << "Session:   " << count << "\n"
            << "Syzkaller: " << session.syzkaller.date.get_date() << " - " << session.syzkaller.name << "\n"
            << "Kernel:    " << session.kernel.date.get_date() << " - " << session.kernel.name
            << (session.kernel.tag.empty() ? "" : " ("+session.kernel.tag+")") << "\n" << std::flush;
}

void log_session_compiler(std::ofstream &logfile, const std::string &compiler)
{
    logfile << "Compiler:  " << compiler << "\n" << std::flush;
}

void log_kernel_build_error(std::ofstream &logfile)
{
    logfile << "Error: The kernel failed to make.\n" << std::flush;
}

void log_syzkaller_build_error(std::ofstream &logfile)
{
    logfile << "Error: Syzkaller failed to make.\n" << std::flush;
}

// the bug was found
// Attempt 1:
//     Time  Bug Name
//        5  KASAN...
// ***    9  UBSAN...
// Attempt 2:
// ...

void log_attempt_result(std::ofstream &logfile, const Syzkaller_Result &attempt, int i, const std::vector<std::string> &dups, int fuzztimes)
{
    logfile << "Attempt " << i << ":" << (i > fuzztimes ? " (RETRY)" : "") << "\n";

    if (attempt.reports.size() > 0)
        logfile << "    Time  Bug Name\n" << std::flush;
    else
        logfile << "    No crashes found.\n" << std::flush;
    
    for (Crash_Report cr : attempt.reports)
        logfile << (fuzz_is_crash_in(cr.name, dups) ? "*** " : "    ") << std::right << std::setw(4) << cr.time << "  " << cr.name << std::endl << std::flush;
}

void log_session_result(std::ofstream &logfile, const Test_Result &result, const std::vector<std::string> &dups)
{
    logfile << "The bug was " << (result.found ? "" : "not ") << "found.\n" << std::flush;

    if (!result.stable)
        logfile << "Warning: This session is unstable.\n" << std::flush;
}
