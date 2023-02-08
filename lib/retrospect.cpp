#include <consts.h>

#include <iostream>

using namespace std;

void set_safe_mode(bool &safe_mode, int &max_time)
{
    safe_mode = true;
    FUZZTIMES = 5;
    max_time = 60;
    cout << "Switching to safe-mode: Fuzzing " << FUZZTIMES << " times at " << max_time << " minutes\n";
}
