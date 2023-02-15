#ifndef RETROSPECT_H
#define RETROSPECT_H

#include <session.h>
#include <result.h>
#include <version.h>

#include <fstream>

void log_safe_mode(std::ofstream &, int, int);

// switch to fuzzing in safe mode. More fuzzing attempts and for longer.
void set_safe_mode(bool &, int &, int &);

bool check_safe_mode(const Test_Result &, bool &, int &, int &);

int get_next_commit_binary(const int, const int, std::vector<Version> &);

void log_session_info(std::ofstream &, const Session &, const int);

void log_session_compiler(std::ofstream &, const std::string &);

void log_kernel_build_error(std::ofstream &);

void log_syzkaller_build_error(std::ofstream &);

void log_attempt_result(std::ofstream &, const Syzkaller_Result &, int, const std::vector<std::string> &, int);

void log_session_result(std::ofstream &, const Test_Result &, const std::vector<std::string> &);

#endif
