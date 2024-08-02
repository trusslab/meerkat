#!/bin/bash

set -e

line=1
bugfile=../parse/bugs.csv
outfile=manager-assiciated.txt
maxline=$(cat $bugfile | wc -l)

echo -n "" > $outfile

while (( $line <= $maxline )); do
    buglink=$(sed -n ${line}p $bugfile | awk -F',' '{ print $1; }')

    bsnapshot=$(lynx -dump -dont_wrap_pre -width=300 $buglink)

    # decide on a good crash (exact same as in bparse)
    # get a list of the crashes
    precrash=$(echo "$bsnapshot" | grep "\[[0-9]*\]\.config[ ]*\[[0-9]*\]log[ ]*\[[0-9]*\]report" | cat)

    # get only crashes with syz reproducers
    precrash1=$(echo "$precrash" | grep "\[[0-9]*\]syz" | cat)

    # get only crashes in upstream
    precrash1=$(echo "$precrash1" | grep " upstream " | cat)

    # sort crashes by date and take the oldest
    crash=$(echo "$precrash1" | sort -k3 | sort -k2 | head -n 1 | cat)

    # grep out the manager
    manager=$(echo "$crash" | grep -o "^[ ]*[A-Za-z0-9\-]* " | grep -o "[A-Za-z0-9\-]*" | cat)

    firstcrash=$(echo "$precrash" | sort -k3 | sort -k2 | head -n 1 | cat)
    firstmanager=$(echo "$firstcrash" | grep -o "^[ ]*[A-Za-z0-9\-]* " | grep -o "[A-Za-z0-9\-]*" | cat)

    echo "$line,$manager,$firstmanager" >> $outfile
    line=$(( $line + 1 ))
done