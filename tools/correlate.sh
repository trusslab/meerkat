#!/bin/bash
set -e

# A short script to find bugs in one file based on their nuumber in another.
# Since I grabbed bugs multiple times, their numbers are not consistent.

old=`pwd`
cd ../parse

# old and new files
originfile=bugs-upstream.csv
newfile=bugs-all_repro.csv
donefile=bugs-already_done.csv

# get the bug name from the bug number
bugname=$(sed -n $1p $originfile | awk -F',' '{ print $2}')

# get the new bug
newbug=$(grep "$bugname" $newfile)
newbugname=$(echo "$newbug" | awk -F',' '{ print $2}')
newnumber=$(grep -n "$bugname" $newfile | grep -o "^\([0-9]\+\)")

# check names
if [[ $bugname == $newbugname ]]; then
    echo "$newnumber $newbugname"
    #echo "$newbug" >> $donefile
    #sed -i "${newnumber}d" $newfile
else
    echo "$bugname"
    echo "$newbugname"
    echo "$newnumber"
fi

cd $old
