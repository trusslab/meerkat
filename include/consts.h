#ifndef CONSTS_H
#define CONSTS_H

#include <date.h>
#include <string>

// the date syzbot started on (roughly)
const Date SYZBOT_BEGIN_DATE(2017,7,22);

// Some remote repositories/websites
const std::string SYZKALLER_REPO_REMOTE = "https://github.com/google/syzkaller";
const std::string SYZBOT_FIXED_LINK = "https://syzkaller.appspot.com/upstream/fixed";

const std::string OLDEST_SYZKALLER_HASH = "87f9bdb8688ceafa804eb49d566bdc38dfb9fd5e";
const std::string LATEST_SYZKALLER_HASH = "8f633d840e3eb6454f036e9da3285bcf27345616";

const std::string SPACER = "====================================================================================================================================================\n";

const int FUZZTIMES = 3;
const int TIME_INCREMENT = 1;
const int START_PORT = 12000;

#endif
