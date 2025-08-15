#ifndef FUZZ_PREP_H
#define FUZZ_PREP_H

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
VMConfig determine_threadedness(Environment &);

// Reads the gcc version file and returns them in vector form
std::vector<Version> grab_compiler_versions(const std::string &);

int set_kernel_config(const std::string &, const std::vector<std::string> &);
int unset_kernel_config(const std::string &, const std::vector<std::string> &);

// Grabs the correct kernel version, applies any patches needed,
// copies the config in, and build the kernel
int prep_kernel(const Environment &, Git &, const Version &, const std::string &, bool = false);

// runs make clean
int clean_kernel(const Environment &);

// writes the syzkaller config to the config file.
// also shifts the host port by one
int write_syzkaller_config(const Environment &);

// deletes the syzkaller working directory and recreates it.
void reset_kaller_wd(const Environment &);
int prepare_kaller_wd(const Environment &);

void reset_runner_wd(const Environment &);

#endif
