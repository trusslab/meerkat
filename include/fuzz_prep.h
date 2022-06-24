#ifndef FUZZ_PREP_H
#define FUZZ_PREP_H

#include <inspector_config.h>
#include <bug_info.h>
#include <date.h>
#include <version.h>

#include <string>
#include <iostream>

int get_procs_from_repro(const std::string &);   // determine the threadedness of the reproducer
VMConfig determine_threadedness(const InspectorConfig &, const Bug_Info &, std::ostream &);

std::vector<Version> grab_gcc_versions();   // fetch gcc versions from file
void reset_inspector();                     // reset the found crashes, wd, etc...
int export_gcc(const std::vector<Version> &, const Date &);                             // set environment variable for correct gcc
void clean_gcc();                           // remove gcc from path
int prep_kernel(const Bug_Info &, const InspectorConfig &);     // go get the correct kernel version and build it
int prep_syzkaller(const Bug_Info &, const InspectorConfig &);  // go get the correct syzkaller version and build it
void clean_syzkaller();
void calc_bloat();                          // calculate how much bloat is in the slimmed template

#endif