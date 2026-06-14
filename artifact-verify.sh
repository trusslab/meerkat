#!/bin/bash

# Joseph Bursey <jbursey@uci.edu>

# This script will run various presets for Meerkat for the purpose of artifact
# functional/reproducible verification.

set -e

self=${0}

log() {
    echo "${self}: $@"
}

usage() {
    echo "Usage: ${self} [profile] [profile-arg]"
    echo "Profiles:"
    echo "    help: print this message."
    echo "    setup-only: download/unpack/verify the dependencies."
    echo "    functional: [Suggested test for functional badge]"
    echo "    reproducible: [Suggested test for reproducible badge]"
    echo "    mk1p: "
    echo "    mknm: "
    echo "    mkm1p: "
    echo "    mk: "
    # Profiles:
    # help
    # setup-only
    # functional
    # mk1p
    # mknm
    # mkm1p
    # mk
        # basic (1)
        # short (34)
        # full (all)
    # For each, we want basic (1), short (like 34), and full(all)

}

setup() {
    # Set up the dependencies
}

profile=${1}
profileArg=${2}
if [[ ${profile} == "" ]]; then
    log "No profile was given! Please provide one arguemnt:"
    usage
    exit
fi

