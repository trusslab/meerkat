#!/bin/bash

set -e

source ../parameters/config.cfg

# input file. Made by parse/bparse.sh
bugfile=$bisectdir/parse/bugs.csv

# the date that syzbot started fuzzing
syzbotDate="2017-07-22"

# number to identify the bug by
number=1
wd=""
startLine=1
endLine=0

# =================================================================================================
# Functions

writebugconfig () {
    echo -n "" > $bisectorconfig
    echo "curBug=\"$curBug\"" >> $bisectorconfig
    echo "bugname=\"$bugName\"" >> $bisectorconfig
    echo "kpref=\"$kpref\"" >> $bisectorconfig
    echo "arch=$arch" >> $bisectorconfig
    echo "repo=$repo" >> $bisectorconfig
    echo "kconfig=$bisectdir/$wd/config-$curBug.txt" >> $bisectorconfig
    echo "repro=$bisectdir/$wd/repro-$curBug.prog" >> $bisectorconfig
    echo "kallerwd=$bisectdir/$wd/wd-kaller" >> $bisectorconfig
    echo "syzconfig=$bisectdir/$wd/syzkaller.cfg" >> $bisectorconfig
    echo "managerwd=$bisectdir/$wd" >> $bisectorconfig
    echo "syzdir=$bisectdir/$wd/syzkaller" >> $bisectorconfig
    echo "buglink=\"$buglink\"" >> $bisectorconfig
}

printhelp () {
    echo "Help:"
    echo "    s - the line in parse/bugs.csv to start on"
    echo "    e - the last line in parse/bugs.csv to use"
    echo "    i - the id given to this manager"
    echo "    b - determine the name of the bug file in parse/"
}

# =================================================================================================

setuponly=""
nopoc=""
mtime=""
findonly=""

# get the start and end lines from the arguments
while getopts "s:e:i:b:" flag
do
    case $flag in
        s)
            startLine="${OPTARG}" ;;
        e)
            endLine="${OPTARG}" ;;
        i)
            id="${OPTARG}" ;;
        b)
            bugfile="$bisectdir/parse/${OPTARG}" ;;
        *)
            printhelp
            exit
    esac
done

if (( endLine == 0 )); then
    endLine=$(( $(cat $bugfile | wc -l) ))
fi

if [[ $id == "" ]]; then
    echo "No id given. Use -i <id>"
    exit
fi

wd="wd-bisector-$id"
bisectorconfig=../$wd/bug.cfg
logfile=../$wd/log/merges.log

if [ ! -d ../$wd ]; then
    echo "Making the working directory: ../$wd"
    mkdir ../$wd
fi

if [ ! -d ../$wd/log ]; then
    echo "Making the log directory"
    mkdir ../$wd/log
fi

totallines=$(cat $bugfile | wc -l)
if (( $endLine > $totallines || $startLine < 1 || $startLine > $endLine )); then
    echo "Bad start and/or end lines!"
    exit
fi

line=$startLine

echo "file: $bugfile startline: $startLine endline: $endLine" >> $logfile
echo "" >> $logfile

while (( $line <= $endLine )); do
    linetext=$(sed -n ${line}p $bugfile)
    fixDate=$(echo "$linetext" | awk -F',' '{ print $5; }')
    findDate=$(echo "$linetext" | awk -F',' '{ print $9; }')
    guiltyDate=$(echo "$linetext" | awk -F',' '{ print $11; }')

    arch=$(echo "$linetext" | awk -F',' '{ print $12; }')

    # if the bug is older than syzbot, use syzbot as the starting date
    syzbotAge=$(( $($bisectdir/helpers/diffdate $guiltyDate $syzbotDate) ))
    if (( $syzbotAge < 0 )); then
        startDate=$syzbotDate
    else
        startDate=$guiltyDate
    fi

    # check that the fixing date is not bogus
    fixAge=$(( $($bisectdir/helpers/diffdate $fixDate $findDate) ))
    if (( $fixAge < 0 )); then
        # sed at the end because shell script is weird about what is escaped in urls...
        findlink=$(echo "$linetext" | awk -F',' '{ print $8; }' | sed 's/\\//' | sed 's/log/commit/')
        snapshot=$(lynx -dump -dont_wrap_pre -width=300 $findlink)
        findDate=$(echo "$snapshot" | grep -m 1 "^[ ]*committer " | grep -o "20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]" | cat)
        fixAge=$(( $($bisectdir/helpers/diffdate $fixDate $findDate) ))
    fi

    # check that the time to find is good and interesting
    findAge=$(( $($bisectdir/helpers/diffdate $findDate $startDate) ))

    if (( $findAge > 1 && $fixAge >= 0 )) && [[ $arch == "amd64" ]]; then
        # bug number and name
        curBug="bug$line"
        bugName="$(echo "$linetext" | awk -F',' '{ print $2; }')"
        configlink="$(echo "$linetext" | awk -F',' '{ print $7; }')"
        reprolink=$(echo "$linetext" | awk -F',' '{ print $6; }')

        # linux kernel repository
        # the link to the finding commit has what we need
        findlink=$(echo "$linetext" | awk -F',' '{ print $8; }')
        repo=$(echo "$findlink" | grep "https://git.kernel" | awk -F'/' '{ print $9"/"$10; }' | cat)
        if [[ $repo == "torvalds/linux.git" ]]; then
            kpref="linux"
        else
            echo "Bad repository on line $line: $repo"
            kpref=""
        fi

        guiltylink=$(echo "$linetext" | awk -F',' '{ print $10; }')

        # clean up the bug name if it is a duplicate name
        if [[ $(echo "$bugName" | grep "([0-9]*)$" | cat) != "" ]]; then
            bugName=${bugName::-4}
        fi

        findhash=$(echo $findlink | grep -o "[0-9a-f]*$" | cat)
        buglink=$(echo "$linetext" | awk -F',' '{ print $1; }')
        guiltyhash=$(echo $guiltylink | grep -o "[0-9a-f]*$" | cat)

        if [[ $reprolink != "" && $configlink != "" && $bugName != "" && $findlink != "" && $repo != "" && $kpref != "" ]]; then
            writebugconfig
            set +e
            ./findmerge -F $findhash -G $guiltyhash -i $id
            set -e
            number=$(( $number + 1 ))
        fi
    fi

    line=$(( $line + 1 ))
done

echo "All Done!"