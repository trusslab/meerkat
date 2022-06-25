#ifndef CONSTS_H
#define CONSTS_H

#include <date.h>
#include <string>

// the date syzbot started on (roughly)
const Date SYZBOT_BEGIN_DATE(2017,7,22);

// Some remote repositories/websites
const std::string SYZKALLER_REPO_REMOTE = "https://github.com/google/syzkaller";
const std::string SYZBOT_FIXED_LINK = "https://syzkaller.appspot.com/upstream/fixed";

const std::string SPACER = "====================================================================================================================================================\n";

const int FUZZTIMES = 3;
const int TIME_INCREMENT = 1;

#endif