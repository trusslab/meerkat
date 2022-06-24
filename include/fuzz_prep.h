#ifndef FUZZ_PREP_H
#define FUZZ_PREP_H

#include <inspector_config.h>
#include <bug_info.h>
#include <date.h>
#include <version.h>

#include <string>
#include <iostream>

// reads the reproducer file to determine how many
// procs syzbot used to trigger the bug. The number
// of procs can be used to determine the threadedness of the bug
int get_procs_from_repro(const std::string &);

// Outer function to determine the threadedness of the bug.
// Returns the allocation to use.
VMConfig determine_threadedness(const InspectorConfig &, const Bug_Info &, std::ostream &);

// Reads the gcc version file and returns them
// in vector form
std::vector<Version> grab_gcc_versions();

// resets inspector state
void reset_inspector();

// exports the correct gcc version to the path for use
int export_gcc(const std::vector<Version> &, const Date &);

// removes gcc from the path (actually just resets the path)
void clean_gcc();

// Grabs the correct kernel version, applies any patches needed,
// copies the config in, and build the kernel
int prep_kernel(const Bug_Info &, const InspectorConfig &);

// grabs the correct syzkaller version, applies any patches
// slims the template, and builds syzkaller
int prep_syzkaller(const Bug_Info &, const InspectorConfig &);

// cleans files when we switch syzkaller versions
void clean_syzkaller();

// calculates the bloat in the template.
// syscalls in template - syscalls in reproducer
void calc_bloat();

#endif