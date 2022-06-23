#ifndef EXEC_API_H
#define EXEC_API_H

#include <string>

int exec_and_wait(const std::string &, char ** args,  const std::string & = "");
int exec_and_continue(const std::string &, char ** args,  const std::string & = "");

#endif