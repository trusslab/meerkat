#!/bin/bash

set -e

source inspector-config/parameters.cfg

# input file. Made by parse/bparse.sh
bugfile=$inspectdir/parse/bugs.csv

# the date that syzbot started fuzzing
syzbotDate="2017-07-22"

# number to identify the bug by
number=1
wd=""
startLine=1
endLine=$(( $(cat $bugfile | wc -l) ))

# =================================================================================================
# Functions

writebugconfig () {
    echo -n "" > $inspectorconfig
    echo "curBug=\"$curBug\"" >> $inspectorconfig
    echo "bugname=\"$bugName\"" >> $inspectorconfig
    echo "kpref=\"$kpref\"" >> $inspectorconfig
    echo "repo=$repo" >> $inspectorconfig
    echo "kconfig=$inspectdir/$wd/config-$curBug.txt" >> $inspectorconfig
    echo "repro=$inspectdir/$wd/repro-$curBug.prog" >> $inspectorconfig
    echo "kallerwd=$inspectdir/$wd/wd-kaller" >> $inspectorconfig
    echo "syzconfig=$inspectdir/$wd/syzkaller.cfg" >> $inspectorconfig
    echo "managerwd=$inspectdir/$wd" >> $inspectorconfig
    echo "syzdir=$inspectdir/$wd/syzkaller" >> $inspectorconfig
    echo "buglink=\"$buglink\"" >> $inspectorconfig
}

printhelp () {
    echo "Help:"
    echo "    s - the line in parse/bugs.csv to start on"
    echo "    e - the last line in parse/bugs.csv to use"
    echo "    i - the id given to this manager"
}

# =================================================================================================

# get the start and end lines from the arguments
while getopts "s:e:i:" flag
do
    case $flag in
        s)
            startLine="${OPTARG}" ;;
        e)
            endLine="${OPTARG}" ;;
        i)
            id="${OPTARG}" ;;
        *)
            printhelp
            exit
    esac
done

if [[ $id == "" ]]; then
    echo "No id given. Use -i <id>"
    exit
fi

wd="wd-inspector-$id"
inspectorconfig=$wd/bug.cfg
logfile=$wd/log/manager-log.csv

if [ ! -d $wd ]; then
    echo "Making the working directory: $wd"
    mkdir $wd
fi

if [ ! -d $wd/log ]; then
    echo "Making the log directory"
    mkdir $wd/log
fi

totallines=$(cat $bugfile | wc -l)
if (( $endLine > $totallines || $startLine < 1 || $startLine > $endLine )); then
    echo "Bad start and/or end lines!"
    exit
fi

line=$startLine

echo "startline:,$startLine,endline:,$endLine" >> $logfile
echo "" >> $logfile

while (( $line <= $endLine )); do
    linetext=$(sed -n ${line}p $bugfile)
    fixDate=$(echo "$linetext" | awk -F',' '{ print $4; }')
    findDate=$(echo "$linetext" | awk -F',' '{ print $8; }')
    guiltyDate=$(echo "$linetext" | awk -F',' '{ print $10; }')
    echo -n "$line,$fixDate,$findDate,$guiltyDate" >> $logfile

    # if the bug is older than syzbot, use syzbot as the starting date
    syzbotAge=$(( $($inspectdir/helpers/diffdate $guiltyDate $syzbotDate) ))
    if (( $syzbotAge < 0 )); then
        startDate=$syzbotDate
    else
        startDate=$guiltyDate
    fi

    # check that the fixing date is not bogus
    fixAge=$(( $($inspectdir/helpers/diffdate $fixDate $findDate) ))
    if (( $fixAge < 0 )); then
        # sed at the end because shell script is weird about what is escaped in urls...
        findlink=$(echo "$linetext" | awk -F',' '{ print $7; }' | sed 's/\\//' | sed 's/log/commit/')
        snapshot=$(lynx -dump -dont_wrap_pre -width=300 $findlink)
        findDate=$(echo "$snapshot" | grep -m 1 "^[ ]*committer " | grep -o "20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]" | cat)
        fixAge=$(( $($inspectdir/helpers/diffdate $fixDate $findDate) ))
    fi

    # check that the time to find is good and interesting
    findAge=$(( $($inspectdir/helpers/diffdate $findDate $startDate) ))

    if (( $findAge > 1 && $fixAge >= 0)); then
        # bug number and name
        curBug="bug$line"
        bugName="$(echo "$linetext" | awk -F',' '{ print $2; }')"

        # kernel config - download it
        configlink=$(echo "$linetext" | awk -F',' '{ print $6; }')
        wget $configlink -O $wd/config-$curBug.txt

        # reproducer - download it
        reprolink=$(echo "$linetext" | awk -F',' '{ print $5; }')
        wget $reprolink -O $wd/repro-$curBug.prog

        # linux kernel repository
        # the link to the finding commit has what we need
        findlink=$(echo "$linetext" | awk -F',' '{ print $7; }')
        repo=$(echo "$findlink" | grep "https://git.kernel" | awk -F'/' '{ print $9"/"$10; }' | cat)
        if [[ $repo == "bpf/bpf.git" ]]; then
            kpref="bpf"
        elif [[ $repo == "bpf/bpf-next.git" ]]; then
            kpref="bpf-next"
        elif [[ $repo == "davem/net.git" ]]; then
            kpref="net"
        elif [[ $repo == "davem/net-next.git" ]]; then
            kpref="net-next"
        elif [[ $repo == "gregkh/usb.git" ]]; then
            kpref="usb"
        elif [[ $repo == "next/linux-next.git" ]]; then
            kpref="linux-next"
        elif [[ $repo == "torvalds/linux.git" ]]; then
            kpref="linux"
        else
            echo "Bad repository on line $line: $repo"
            kpref=""
        fi

        # clean up the bug name if it is a duplicate name
        if [[ $(echo "$bugName" | grep "([0-9]*)$" | cat) != "" ]]; then
            bugName=${bugName::-4}
        fi

        findhash=$(echo $findlink | grep -o "[0-9a-f]*$" | cat)
        buglink=$(echo "$linetext" | awk -F',' '{ print $1; }')

        if [[ $reprolink != "" && $configlink != "" && $bugName != "" && $findlink != "" && $repo != "" && $kpref != "" && fixhash != "" ]]; then
            writebugconfig

            echo ",good fuzz,$findDate,$findDate,$startDate" >> $logfile
            # run inspector on the bug
            # using finding date as the ending date
            echo "Fuzzing. start: $startDate; end: $findDate; finding: $findDate"
            ./syzInspector.sh -f $findDate -F $findhash -s $startDate -e $findDate -i $id
            number=$(( $number + 1 ))
        else
            echo "Possible bad parse on line $line"
            echo ",bad parse" >> $logfile
        fi
    else
        if (( $findAge <= 1 && $findAge >= 0 )); then
            echo "Bug was found within 1 day on line $line: $fixDate, finding: $findDate, guilty: $guiltyDate"
            echo ",too young" >> $logfile
        else
            echo "Bad dates on line $line: fixing: $fixDate, finding: $findDate, guilty: $guiltyDate"
            echo ",bad dates" >> $logfile
        fi
    fi

    line=$(( $line + 1 ))
done

echo "All Done!"
