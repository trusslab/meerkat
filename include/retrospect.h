#ifndef RETROSPECT_H
#define RETROSPECT_H

#include <session.h>
#include <fuzz.h>

#include <fstream>

// switch to fuzzing in safe mode. More fuzzing attempts and for longer.
void set_safe_mode(bool &, int &, int &);

void log_session_info(std::ofstream &, const Session &, const int);

void log_session_compiler(std::ofstream &, const std::string &);

void log_kernel_build_error(std::ofstream &);

void log_syzkaller_build_error(std::ofstream &);

void log_session_result(std::ofstream &, const Test_Result &, const std::vector<std::string> &);

#endif
