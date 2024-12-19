#ifndef FUZZ_PREP_H
#define FUZZ_PREP_H

#include <bug_info.h>
#include <date.h>
#include <environment.h>
#include <git.h>
#include <version.h>

#include <string>
#include <vector>
#include <iostream>

// reads the reproducer file to determine how many
// procs syzbot used to trigger the bug. The number
// of procs can be used to determine the threadedness of the bug
int get_procs_from_repro(const std::string &);

// Outer function to determine the threadedness of the bug.
// Returns the allocation to use.
VMConfig determine_threadedness(Environment &, const Bug_Info &, std::ostream &);

// Reads the gcc version file and returns them
// in vector form
std::vector<Version> grab_compiler_versions(const std::string &);

// returns the name of the compiler to be used
std::string get_compiler(const std::vector<Version> &, const std::vector<Version> &, const Date &, const Environment &);

// removes gcc from the path (actually just resets the path)
int clean_path(const std::string &);

int set_kernel_config(const std::string &, const std::vector<std::string> &);
int unset_kernel_config(const std::string &, const std::vector<std::string> &);

// Grabs the correct kernel version, applies any patches needed,
// copies the config in, and build the kernel
int prep_kernel(const Environment &, const Bug_Info &, Git &, const Version &, const std::string &);

// runs make clean
int clean_kernel(const Environment &);

// grabs the correct syzkaller version, applies any patches
// slims the template, and builds syzkaller
int prep_syzkaller(const Environment &, const Bug_Info &, Git &, const Version &, bool = true);

// writes the syzkaller config to the config file.
// also shifts the host port by one
int write_syzkaller_config(const Environment &, const Bug_Info &, const Date &);

// deletes the syzkaller working directory and recreates it.
void reset_kaller_wd(const Environment &);
int prepare_kaller_wd(const Environment &, const Bug_Info &, bool);

// cleans files when we switch syzkaller versions.
// Simply removes any non-hidden file.
// Same behavior as rm -r *
int clean_syzkaller(const Environment &);

#endif
