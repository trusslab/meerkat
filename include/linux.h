#ifndef FUZZ_PREP_H
#define FUZZ_PREP_H

#include <environment.h>
#include <git.h>
#include <version.h>

#include <string>
#include <vector>
#include <iostream>

// Initialize and pull the given repository
Git prep_kernel_local_repo(Environment &);

// Outer function to determine the threadedness of the bug.
// Returns the allocation to use.
VMConfig determine_threadedness(Environment &);

// Reads the gcc version file and returns them in vector form
std::vector<Version> grab_compiler_versions(const std::string &);

int set_kernel_config(const std::string &, const std::vector<std::string> &);
int unset_kernel_config(const std::string &, const std::vector<std::string> &);

// Grabs the correct kernel version, applies any patches needed,
// copies the config in, and build the kernel
int build_kernel(const Environment &, Git &, const Version &, const std::string &, bool = false);

// runs make clean
int clean_kernel(const Environment &);

#endif
