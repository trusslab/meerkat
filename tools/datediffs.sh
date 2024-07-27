#!/bin/bash

set -e

bugfile=parse/bugs-2023-unique.csv

finding=()
guilty=()

which helpers/diffdate
if (( $? != 0 )); then
    echo "Could not find helpers/diffdate"
fi

# get all of the finding dates
readarray -t finding <<< "$(awk -F',' '{ print $8 }' < ${bugfile})"

# get all of the guilty dates
readarray -t guilty <<< "$(awk -F',' '{ print $3 }' < ${bugfile})"

for (( i=1; i<${#finding[@]}; i++ )); do
    d=$(( $(helpers/diffdate ${finding[$i]} ${guilty[$i]}) ))
#    l=$(( $(../helpers/diffdate ${guilty[$i]} "2017-09-26") ))
    if (( $d >= 0 )); then
        echo "${finding[$i]},${guilty[$i]},$d"
    fi
done
