#include <consts.h>
#include <session.h>
#include <fuzz.h>

#include <iostream>
#include <fstream>
#include <iomanip>

using namespace std;

void set_safe_mode(bool &safe_mode, int &max_time, int &fuzztimes)
{
    safe_mode = true;
    fuzztimes = 5;
    max_time = 60;
    cout << "Switching to safe-mode: Fuzzing " << fuzztimes << " times at " << max_time << " minutes\n";
}

void log_session_info(ofstream &logfile, const Session &session, const int count)
{
    logfile << "Session:   " << count << "\n"
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

void log_session_result(ofstream &logfile, const Test_Result &result, const vector<string> &dups)
{
    logfile << "The bug was " << (result.found ? "" : "not ") << "found.\n" << flush;
    for (int i = 0; i < result.attempts.size(); i++)
    {
        logfile << "Attempt " << i << ":\n"
                << "    Time  Bug Name\n";
        
        for (Crash_Report cr : result.attempts.at(i).reports)
            logfile << (fuzz_is_in(cr.name, dups) ? "*** " : "    ") << right << setw(4) << cr.time << "  " << cr.name << endl << flush;
    }
    logfile << endl << flush;
}
