#!/bin/bash

set -e

finding=()
guilty=()

# get all of the finding dates
readarray -t finding <<< "$(awk -F',' '{ print $7 }' < bugs.csv)"


# get all of the guilty dates
readarray -t guilty <<< "$(awk -F',' '{ print $9 }' < bugs.csv)"

for (( i=1; i<${#finding[@]}; i++ )); do
    d=$(( $(../helpers/diffdate ${finding[$i]} ${guilty[$i]}) ))
#    l=$(( $(../helpers/diffdate ${guilty[$i]} "2017-09-26") ))
    if (( $d > 0 )); then
        echo "${finding[$i]},${guilty[$i]},$d"
    fi
done
