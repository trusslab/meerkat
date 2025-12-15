#ifndef REPORT_H
#define REPORT_H

#include <vector>
#include <string>

// A library for parsing Syzkaller-style report files and extracting stack traces

#define cut_here "------------[ cut here ]------------"

enum Crash_Type { CT_UNKNOWN, CT_KASAN, CT_SLEEPING, CT_WARNING, CT_WARNING_RCU };

int parse_report(const std::string &, std::vector<std::string> &);

#endif
