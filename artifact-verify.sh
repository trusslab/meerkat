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
    echo "    mk1p: Run Meerkat with Mk_1p settings."
    echo "    mknm: Run Meerkat with Mk_nm settings."
    echo "    mkm1p: Run Meerkat with Mk_m1p settings."
    echo "    mk: Run Meerkat with Mk settings."
    # TODO: add basic, short, full
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

profile=${1}
profileArg=${2}
if [[ ${profile} == "" ]]; then
    log "No profile was given! Please provide at least one argument:"
    usage
    exit
fi

if [[ ${profile} == "help" ]]; then
    usage
    exit
fi

./setup.sh
if [[ ${profile} == "setup-only" ]]; then
    log "Setup complete"
    exit
fi

s=0
e=0
F=""
b=parse/bugs.csv
i=1
m=10

if [[ ${profileArg} == "basic" ]]; then
    s=2
    e=2
elif [[ ${profileArg} == "short" ]]; then
    log "Not set up yet!"
    exit
elif [[ ${profileArg} == "full" ]]; then
    s=1
    e=200
fi

if [[ ${profile} == "mk" ]]; then
    F="ff-test,poc-test,stateful-corpus,poc-all-pocs"
elif [[ ${profile} == "mkm1p" ]]; then
    F="ff-test,poc-test,stateful-corpus"
elif [[ ${profile} == "mknm" ]]; then
    F="poc-test,poc-all-pocs"
elif [[ ${profile} == "mk1p" ]]; then
    F="poc-test"
fi

./mk-manager.sh -s ${s} -e ${e} -i ${i} -b ${b} -m ${m} -F ${F}

