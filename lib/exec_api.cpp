#include <exec_api.h>
#include <string>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

int exec_and_wait(const string & prog, const string & outfile = "")
{
    pid_t pid = fork();
    if (pid < 0)
    {
        cerr << "Error: Failed to fork.\n";
        return;
    }
    else if (pid == 0)
    {
        // child
        if (!outfile.empty())
        {
            // dup file to stdout
        }
    }
    else if (pid > 0)
    {
        // parent. wait for child to finish.
    }

    return 0;
}