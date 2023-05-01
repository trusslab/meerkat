#!/bin/bash

set -e

bugfile=/mnt/sda/jtbursey/SyzInspector/parse/bugs-all_repro.csv
totalLines=$(wc -l $bugfile | grep -o "[0-9]*")

startLine=1
endLine=$totalLines
outfile=""

# get the start and end lines from the arguments
while getopts "s:e:o:" flag
do
    case $flag in
        s)
            startLine="${OPTARG}" ;;
        e)
            endLine="${OPTARG}" ;;
        o)
            outfile="${OPTARG}" ;;
        *)
            echo "Bad Option"
            exit
    esac
done

# file and line sanitation
if [[ $outfile != "" ]]; then
    echo -n "" > $outfile
fi

if (( $startLine > $totalLines || $endLine > $totalLines || $startLine > $endLine )); then
    echo "Bad start or end lines. Max: $totalLines"
    exit
fi

currentLine=$startLine
while (( $currentLine <= $endLine )); do
    # get the bug link and name from the bug file
    line=$(sed -n ${currentLine}p $bugfile)
    bugLink=$(echo $line | awk -F',' '{ print $1; }')
    bugName=$(echo $line | awk -F',' '{ print $2; }')
    if [[ $(echo "$bugName" | grep "([0-9]*)$" | cat) != "" ]]; then
        bugName=${bugName::-4}
    fi

    echo "Bug Name: $bugName"
    echo "Link:     $bugLink"

    # extract the crashing function from the name
    crashingFunc=$(echo $bugName | awk -F' in ' '{ print $2; }')
    if [[ $crashingFunc == "" ]]; then
        crashingFunc=$(echo $bugName | awk -F' at ' '{ print $2; }')
    fi

    # if still no dice, skip
    if [[ $crashingFunc == "" ]]; then
        echo "Could not get a crashing function from the bug name: $bugName"
        echo ""
        if [[ $outfile != "" ]]; then
            echo "$currentLine,$bugName,$bugLink,HELP" >> $outfile
        fi
        currentLine=$(( $currentLine + 1 ))
        continue
    fi

    echo "Function: $crashingFunc"

    # find a report from the bug page
    bugSnapshot=$(lynx -dump -dont_wrap_pre -width=300 $bugLink)
    crash=$(echo "$bugSnapshot" | awk '/Crashes \(/,0' | grep " upstream " | grep -m 1 "\[\([0-9]\+\)\]\.config[ ]*\[\([0-9]\+\)\]console\|strace log[ ]*\[\([0-9]\+\)\]report" | cat)
    reportLinkNum=$(echo $crash | grep -o "\[\([0-9]\+\)\]report" | grep -o "\([0-9]\+\)" | cat)
    reportLink=$(echo "$bugSnapshot" | grep "^[ ]*${reportLinkNum}. http" | grep -o "http.*$" | cat)

    # download the report
    reportRaw=$(lynx -dump -dont_wrap_pre -width=300 $reportLink)
    # find a potential crash site based on the crashing function
    crashSite=$(echo "$reportRaw" | grep "$crashingFunc" | cat)
    # extract the path
    crashPath=$(echo "$crashSite" | grep -o "\(\([A-Za-z0-9_-]*\/\)\+\)[A-Za-z0-9_-]*\.\(c\|h\)" | head -n 1 | cat)
    # check for a corrupted path in the bug name
    if [[ $crashPath == "" ]]; then
        crashPath=$(echo "$crashingFunc" | grep -o "\(\([A-Za-z0-9_-]*\/\)\+\)[A-Za-z0-9_-]*\.\(c\|h\)" | cat)
    fi

    if [[ $crashPath == "" ]]; then
        echo "Could not find a suitable path for $crashingFunc"
        echo ""
        if [[ $outfile != "" ]]; then
            echo "$currentLine,$bugName,$bugLink,$crashingFunc,HELP" >> $outfile
        fi
        currentLine=$(( $currentLine + 1 ))
        continue
    fi

    # extract the directory
    module=${crashPath%/*}
    rootDir=${module%%/*}
    echo "Module:   $module"
    echo "Root Dir: $rootDir"
    echo ""

    if [[ $outfile != "" ]]; then
        echo "$currentLine,$bugName,$bugLink,$crashingFunc,$module,$rootDir" >> $outfile
    fi

    currentLine=$(( $currentLine + 1 ))
done
