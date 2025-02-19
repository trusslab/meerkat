#ifndef EXEC_API_H
#define EXEC_API_H

#include <string>

// checks if the given process is still running
// takes a single pid.
bool check_alive(int, bool = false);

// Takes a program name, argv, stdout dup, and stderr dup.
// Execs the process and waits for it to finish. Returns
// the return value of the child. Dups the filenames to 
// stdout and stderr if given.
int exec_and_wait(const std::string &, char **, const std::string & = "", const std::string & = "", bool = false);

// Takes a program name, argv, stdout dup, and stderr dup.
// Execs the process and returns the pid of the child.
// Dups the filenames to stdout and stderr if given.
int exec_and_continue(const std::string &, char **, const std::string & = "", const std::string & = "");

// Takes a program name and argv
// Execs the process and waits for it to finish. Returns
// the output of the child up to BUF_SIZE characters.
std::string exec_and_read(const std::string &, char **);

// Kills the given process by pid, then waits on the child
// to reap it. Returns the return value of the child, or err.
int kill_child(int, bool = false);

#endif
