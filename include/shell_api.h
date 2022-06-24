#ifndef SHELL_API_H
#define SHELL_API_H

#include <inspector_config.h>

#include <string>

// returns the path environment variable
std::string get_path();

// exports an environment variable
int export_env(const std::string &);

// calls lynx and dumps the result to a dump file
int lynx_dump(const std::string &, const std::string &);

// calls sed -i with a given pattern and file
int sed_i(const std::string &, const std::string &);

// runs make for the current directory
int make(int, const std::string & = "");

// runs cp src dest
int copy(const std::string &, const std::string &);

// Go Interface functions

// exports go to the path
int export_go(const InspectorConfig &);

// go mod calls
int go_mod_init();
int go_mod_tidy();
int go_mod_vendor();

#endif