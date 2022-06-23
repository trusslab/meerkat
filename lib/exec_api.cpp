#include <exec_api.h>

#include <string>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

int exec_and_wait(const string & prog, char ** args, const string & outfile)
{
    int ret, ret_status = 0;
    pid_t pid = fork();
    if (pid < 0)
    {
        cerr << "Error: Failed to fork.\n";
        return -1;
    }
    else if (pid == 0)
    {
        // child
        if (!outfile.empty())
        {
            // duping file in C because I couldn't find an easy way to do it in C++
            int fd = open(outfile.c_str(), O_CREAT | O_WRONLY, 0666);
            if (fd < 0)
            {
                cerr << "Error: Could not open file " << outfile << " for child process.\n";
                exit(-1);
            }
            dup2(fd, 1);
            close(fd);
        }

        execvp(prog.c_str(), args);
        
        cerr << "Error: exec for " << prog << " failed.\n";
        return -1;
    }
    else if (pid > 0)
    {
        // parent. wait for child to finish.
        waitpid(pid, &ret, 0);
        if (WIFEXITED(ret))
            ret_status = WEXITSTATUS(ret);

        if (ret_status != 0)
            cerr << "Warning: Child process " << prog << " exited with error status " << ret_status << ".\n";
    }

    return ret_status;
}