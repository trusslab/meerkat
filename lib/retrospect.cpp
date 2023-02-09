#include <consts.h>

#include <iostream>

using namespace std;

void set_safe_mode(bool &safe_mode, int &max_time, int &fuzztimes)
{
    safe_mode = true;
    fuzztimes = 5;
    max_time = 60;
    cout << "Switching to safe-mode: Fuzzing " << fuzztimes << " times at " << max_time << " minutes\n";
}
