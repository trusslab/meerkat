#ifndef EXEC_API_H
#define EXEC_API_H

#include <string>

// checks if the given process is still running
// takes a single pid.
bool check_alive(int);

// Takes a program name, argv, stdout dup, and stderr dup.
// Execs the process and waits for it to finish. Returns
// the return value of the child. Dups the filenames to 
// stdout and stderr if given.
int exec_and_wait(const std::string &, char ** args, const std::string & = "", const std::string & = "");

// Takes a program name, argv, stdout dup, and stderr dup.
// Execs the process and returns the pid of the child.
// Dups the filenames to stdout and stderr if given.
int exec_and_continue(const std::string &, char ** args, const std::string & = "", const std::string & = "");

// Kills the given process by pid, then waits on the child
// to reap it. Returns the return value of the child, or err.
int kill_child(int);

#endif