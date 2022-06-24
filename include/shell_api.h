#ifndef SHELL_API_H
#define SHELL_API_H

#include <inspector_config.h>

#include <string>

std::string get_path();
int export_env(const std::string &);

int lynx_dump(const std::string &, const std::string &);
int sed_i(const std::string &, const std::string &);
int make(int, const std::string & = "");
int copy(const std::string &, const std::string &);

// Go Interface functions
int export_go(const InspectorConfig &);
int go_mod_init();
int go_mod_tidy();
int go_mod_vendor();

#endif