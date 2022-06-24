#ifndef EXEC_API_H
#define EXEC_API_H

#include <string>

bool check_alive(int);
int exec_and_wait(const std::string &, char ** args, const std::string & = "", const std::string & = "");
int exec_and_continue(const std::string &, char ** args, const std::string & = "", const std::string & = "");
int kill_child(int);

#endif