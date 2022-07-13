#include <exec_api.h>
#include <consts.h>

#include <string>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

using namespace std;

bool check_alive(int pid)
{
    int ret;
    int ret_status = waitpid(pid, &ret, WNOHANG);

    if (ret_status < 0)
    {
        cerr << "Error: Waitpid on pid " << pid << " failed.\n";
        return false;
    }
    else if (ret_status > 0)
        return false;
    else if (ret_status == 0)
        return true;

    return false;
}

int exec_and_wait(const string & prog, char ** args, const string & outfile, const string &errfile)
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
            int fd = open(outfile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if (fd < 0)
            {
                cerr << "Error: Could not open file " << outfile << " for child process.\n";
                exit(-1);
            }
            dup2(fd, 1);
        }
        if (!errfile.empty())
        {
            // duping file in C because I couldn't find an easy way to do it in C++
            int fd = open(errfile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if (fd < 0)
            {
                cerr << "Error: Could not open file " << errfile << " for child process.\n";
                exit(-1);
            }
            dup2(fd, 2);
        }

        execvp(prog.c_str(), args);
        
        cerr << "Error: exec for " << prog << " failed.\n";
        exit(-1);
    }
    else if (pid > 0)
    {
        // parent. wait for child to finish.
        waitpid(pid, &ret, 0);
        if (WIFEXITED(ret))
            ret_status = WEXITSTATUS(ret);

        if (ret_status != 0 && prog != "grep")
            cerr << "Warning: Child process " << prog << " exited with error status " << ret_status << ".\n";
    }

    return ret_status;
}

int exec_and_continue(const string & prog, char ** args, const string &outfile, const string &errfile)
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
            int fd = open(outfile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if (fd < 0)
            {
                cerr << "Error: Could not open file " << outfile << " for child process.\n";
                exit(-1);
            }
            dup2(fd, 1);
        }
        if (!errfile.empty())
        {
            // duping file in C because I couldn't find an easy way to do it in C++
            int fd = open(errfile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if (fd < 0)
            {
                cerr << "Error: Could not open file " << errfile << " for child process.\n";
                exit(-1);
            }
            dup2(fd, 2);
        }

        execvp(prog.c_str(), args);
        
        cerr << "Error: exec for " << prog << " failed.\n";
        exit(-1);
    }

    return pid;
}

string exec_and_read(const string & prog, char ** args)
{
    int ret, ret_status = 0;
    int pipefd[2];
    char buf[BUF_SIZE];

    ret = pipe(pipefd);
    if (ret < 0)
        return "";

    pid_t pid = fork();
    if (pid < 0)
    {
        cerr << "Error: Failed to fork.\n";
        return "";
    }
    else if (pid == 0)
    {
        // child
        close(pipefd[0]);
        dup2(pipefd[1], 1);

        execvp(prog.c_str(), args);
        
        cerr << "Error: exec for " << prog << " failed.\n";
        exit(-1);
    }
    else if (pid > 0)
    {
        // parent. wait for child to finish.
        close(pipefd[1]);
        waitpid(pid, &ret, 0);
        if (WIFEXITED(ret))
            ret_status = WEXITSTATUS(ret);

        if (ret_status != 0 && prog != "grep")
            cerr << "Warning: Child process " << prog << " exited with error status " << ret_status << ".\n";

        read(pipefd[0], buf, BUF_SIZE);
    }

    string output(buf);

    return output;
}

int kill_child(int pid)
{
    int ret, ret_status;
    kill(pid, SIGINT);
    waitpid(pid, &ret, 0);
    if (WIFEXITED(ret))
        ret_status = WEXITSTATUS(ret);

    if (ret_status != 0)
        cerr << "Warning: Child process exited with error status " << ret_status << ".\n";
    
    return ret_status;
}
