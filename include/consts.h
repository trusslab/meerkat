#ifndef CONSTS_H
#define CONSTS_H

#include <date.h>
#include <string>
#include <vector>

// the date syzbot started on (roughly)
const Date SYZBOT_BEGIN_DATE(2017,7,22);

const Date OLD_INOUT_DATE(2020,8,13);

// Some remote repositories/websites
const std::string SYZKALLER_REPO_REMOTE = "https://github.com/google/syzkaller";
const std::string LINUX_REPO_REMOTE = "https://git.kernel.org/pub/scm/linux/kernel/git/";
const std::string SYZBOT_FIXED_LINK = "https://syzkaller.appspot.com/upstream/fixed";

const std::string OLDEST_SYZKALLER_HASH = "87f9bdb8688ceafa804eb49d566bdc38dfb9fd5e";
const std::string LATEST_SYZKALLER_HASH = "a4ae4f428721da42ac15f07d6f3b54584dedee27";

const std::vector<std::string> LINUX_BROKEN_VERSONS = {};

const std::vector<std::string> SYZKALLER_BROKEN_VERSONS = {"ec42220e7773fba548e379606fe445cb30f4c424", "455eff3ca1b884ceceaeae46be97a48ead31f916", "ad54dc7a6dd1fd2f2f106e59ff234f0a5d4686a2",
                                                            "01622de2d0ec3b6cc18aef5bcbd5e76dd634116e", "7aa6bd6859a419bfb445a3621a14124fd7cecced"};

const std::string SPACER = "====================================================================================================================================================\n";

const int TIME_INCREMENT = 1;
const int START_PORT = 12000;

const int BUF_SIZE = 4096;

#endif
