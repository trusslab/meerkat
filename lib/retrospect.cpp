#include <consts.h>
#include <session.h>
#include <result.h>
#include <version.h>

#include <iostream>
#include <fstream>
#include <iomanip>

using namespace std;

void log_safe_mode(ofstream &logfile, int max_time, int fuzztimes)
{
    cout << "Switching to Safe Mode: Fuzzing " << fuzztimes << " times at " << max_time << " minutes\n";
    logfile << "Switching to Safe Mode: Fuzzing " << fuzztimes << " times at " << max_time << " minutes\n";
}

void set_safe_mode(bool &safe_mode, int &max_time, int &fuzztimes)
{
    safe_mode = true;
    fuzztimes = 5;
    max_time = max_time > 30 ? max_time : 30;
}

// Checks the given result to see if SyzRetrospector should switch to safe mode.
// If yes, sets safe mode
bool check_safe_mode(const Test_Result &result, bool &safe_mode, int &max_time, int &fuzztimes)
{
    if (!safe_mode && result.found && result.attempts.back().ttf > max_time * 0.8)
    {
        set_safe_mode(safe_mode, max_time, fuzztimes);
        return true;
    }
    return false;
}

// Identifies groups of unstable commits and makes a guess
// whether a given commit is stable or unstable.
// Returns true if stable, false if unstable.
bool infer_stability(vector<Version> &versions, const int m)
{
    int ir = m, il = m;
    if (versions.at(m).skipped)
        return false;
    
    for (; ir < versions.size() && !versions.at(m).skipped; ir++);
    for (; il > 0 && !versions.at(m).skipped; il--);

    if (ir < versions.size() && il > 0)
        return ir - il >= 100;
    else
        return true;
}

// Skip back to a recent stable commit.
// It is assumed that nearby commits will also be unstable,
// so skip back based on a ratio of the remaining commit versions.
// May skip forward only if there are no stable commits back in time.
int skip_commit(const int r, const int l, const int mid, vector<Version> &versions)
{
    int s = (r - l) / 8;
    int m = mid;
    while (m < r && !infer_stability(versions, m))
    {
        versions.at(m).skipped = true;
        m += (s < 100 ? s : 100);
    }
    
    if (m < r)
        return m;
    
    m = mid;
    while (m > l && !infer_stability(versions, m))
    {
        versions.at(m).skipped = true;
        m -= (s < 100 ? s : 100);
    }

    return m > l ? m : -1;
}

// Binary search abstracted here to include skipping.
// If there are no stable commits left, end the search.
// r is older date (lower). higher index
// l is recent date (higher). lower index
int get_next_commit_binary(const int r, const int l, vector<Version> &versions)
{
    int m = (r + l) / 2;
    if (!infer_stability(versions, m))
        m = skip_commit(r, l, m, versions);
    return m;
}

void log_session_info(ofstream &logfile, const Session &session, const int count)
{
    logfile << "\nSession:   " << count << "\n"
            << "Template:  " << session.syz_template.date.get_date() << " - " << session.syz_template.name << "\n"
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
        logfile << (fuzz_is_in(cr.name, dups) ? "*** " : "    ") << right << setw(4) << cr.time << "  " << cr.name << endl << flush;
}

void log_session_result(ofstream &logfile, const Test_Result &result, const vector<string> &dups)
{
    logfile << "The bug was " << (result.found ? "" : "not ") << "found.\n" << flush;

    if (!result.stable)
        logfile << "Warning: This session is unstable.\n" << flush;
}
