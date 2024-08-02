#!/bin/bash

set -e

source parameters/config.cfg

# input file. Made by parse/bparse.sh
bugfile=$inspectdir/parse/bugs.csv

# the date that syzbot started fuzzing
syzbotDate="2017-07-22"

# number to identify the bug by
number=1
wd=""
startLine=1
endLine=0

retrospector=syzInspector

# =================================================================================================
# Functions

writebugconfig () {
    echo -n "" > $inspectorconfig
    echo "curBug=\"$curBug\"" >> $inspectorconfig
    echo "bugname=\"$bugName\"" >> $inspectorconfig
    echo "kpref=\"$kpref\"" >> $inspectorconfig
    echo "arch=$arch" >> $inspectorconfig
    echo "repo=$repo" >> $inspectorconfig
    echo "kconfig=$inspectdir/$wd/config-$curBug.txt" >> $inspectorconfig
    echo "repro=$inspectdir/$wd/reproducers" >> $inspectorconfig
    echo "reproall=$inspectdir/$wd/repro-$curBug-all.prog" >> $inspectorconfig
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
    echo "    b - determine the name of the bug file in parse"
    echo "    m - the maximum time to fuzz at the finding commit"
    echo "    a - the arch to build on (amd64/i386)"
    echo "    p - fuzz without the poc as a seed"
    echo "    f - fuzz only at the finding commit"
    echo "    x - only set up the kernel and syzkaller. Do not fuzz"
    echo "    d - build and use a debug version of SyzInspector"
    echo "    S - run in safe mode"
}

# =================================================================================================

setuponly=""
nopoc=""
mtime=""
findonly=""
safemode=""
targetarch="amd64" # i386

# get the start and end lines from the arguments
while getopts "s:e:i:b:m:a:xpfdS" flag
do
    case $flag in
        s)
            startLine="${OPTARG}" ;;
        e)
            endLine="${OPTARG}" ;;
        i)
            id="${OPTARG}" ;;
        b)
            bugfile="$inspectdir/parse/${OPTARG}" ;;
        m)
            mtime="-m ${OPTARG}" ;;
        a)
            targetarch="${OPTARG}" ;;
        p)
            nopoc="--no-poc" ;;
        f)
            findonly="--find-only"
            setuponly="" ;;
        x)
            setuponly="--setup-only"
            findonly="" ;;
        d)
            retrospector=syzInspector-debug
            make -f Makefile debug ;;
        S)
            safemode="--safe-mode" ;;
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

echo "Starting at: $(date)"

wd="wd-inspector-$id"
inspectorconfig=$wd/bug.cfg
logfile=$wd/log/manager.log

if [ ! -d $wd ]; then
    echo "Making the working directory: $wd"
    mkdir $wd
fi

if [ ! -d $wd/reproducers ]; then
    echo "Making the reproducer directory: $wd/reproducers"
    mkdir $wd/reproducers
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

echo "file: $bugfile startline: $startLine endline: $endLine arch: $targetarch" >> $logfile
echo "" >> $logfile

# "$l,$name,$truefinddate,$fixlink,$fixdate,$config,$findlink,$finddate,$guiltylink,$guiltydate,$bit32,${allrepro[@]}"
while (( $line <= $endLine )); do
    linetext=$(sed -n ${line}p $bugfile)
    fixDate=$(echo "$linetext" | awk -F',' '{ print $5; }')
    findDate=$(echo "$linetext" | awk -F',' '{ print $8; }')        # was $9
    guiltyDate=$(echo "$linetext" | awk -F',' '{ print $10; }')     # was $11
    echo -n "$line,$fixDate,$findDate,$guiltyDate" >> $logfile

    arch=$(echo "$linetext" | awk -F',' '{ print $11; }')           # was $12

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
        findlink=$(echo "$linetext" | awk -F',' '{ print $7; }' | sed 's/\\//' | sed 's/log/commit/')   # was $8
        snapshot=$(lynx -dump -dont_wrap_pre -width=300 $findlink)
        findDate=$(echo "$snapshot" | grep -m 1 "^[ ]*committer " | grep -o "20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]" | cat)
        fixAge=$(( $($inspectdir/helpers/diffdate $fixDate $findDate) ))
    fi

    # ignore most recent bugs as they are broken to my elfutils
    elfutilAge=$(( $($inspectdir/helpers/diffdate "2024-01-01" $findDate) ))

    # check that the time to find is good and interesting
    findAge=$(( $($inspectdir/helpers/diffdate $findDate $startDate) ))

    if (( $findAge > 1 && $fixAge >= 0 )) && [[ $arch == $targetarch ]] && (( $elfutilAge > 0 )); then
        # bug number and name
        curBug="bug${line}"
        bugName="$(echo "$linetext" | awk -F',' '{ print $2; }')"
        if [[ $(echo "${bugName}" | grep "^KMSAN" | cat) != "" ]]; then
            echo ",KMSAN bug. Skip until clang" >> $logfile
            echo "KMSAN bug on line ${line}"
            line=$(( $line + 1 ))
            continue
        fi

        # kernel config - download it
        configlink=$(echo "$linetext" | awk -F',' '{ print $6; }')      # was $7
        wget $configlink -O $wd/config-$curBug.txt

        # reproducers - download them
        rm -rf $wd/reproducers/*
        allrepro=($(echo "$linetext" | awk -F',' '{ print $12; }'))
        reprocount=0
        for reprolink in ${allrepro[@]}; do
            reprocount=$(( $reprocount + 1 ))
            wget $reprolink -O $wd/reproducers/repro-$curBug-$reprocount.prog
        done
        cat $wd/reproducers/repro-$curBug-*.prog > $wd/repro-$curBug-all.prog

        # linux kernel repository
        # the link to the finding commit has what we need
        findlink=$(echo "$linetext" | awk -F',' '{ print $7; }')        # was $8
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

        guiltylink=$(echo "$linetext" | awk -F',' '{ print $9; }')     # was $10

        # clean up the bug name if it is a duplicate name
        if [[ $(echo "$bugName" | grep "([0-9]*)$" | cat) != "" ]]; then
            bugName=${bugName::-4}
        fi

        findhash=$(echo $findlink | grep -o "[0-9a-f]*$" | cat)
        buglink=$(echo "$linetext" | awk -F',' '{ print $1; }')
        guiltyhash=$(echo $guiltylink | grep -o "[0-9a-f]*$" | cat)

        if [[ $reprolink != "" && $configlink != "" && $bugName != "" && $findlink != "" && $repo != "" && $kpref != "" ]]; then
            writebugconfig

            echo ",good fuzz,$findDate,$findDate,$startDate" >> $logfile
            echo "./$retrospector -F $findhash -f $findDate -G $guiltyhash -i $id $mtime $nopoc $findonly $setuponly $safemode" >> $logfile
            # run inspector on the bug
            # using finding date as the ending date
            echo "Fuzzing. finding: $findhash; guilty: $guiltyhash"
            set +e
            ./$retrospector -F $findhash -f $findDate -G $guiltyhash -i $id $mtime $nopoc $findonly $setuponly $safemode
            set -e
            number=$(( $number + 1 ))
        else
            echo "Possible bad parse on line $line"
            echo ",bad parse" >> $logfile
        fi
    else
        if (( $elfutilAge <= 0 )); then
            echo "Bug found too recently (2024 check) on line $line: fixing: $fixDate, finding: $findDate, guilty: $guiltyDate"
            echo ",2024 check" >> $logfile
        elif (( $findAge <= 1 && $findAge >= 0 )); then
            echo "Bug was found within 1 day on line $line: $fixDate, finding: $findDate, guilty: $guiltyDate"
            echo ",too young" >> $logfile
        elif [[ $arch != $targetarch ]]; then
            echo ",wrong arch" >> $logfile
        else
            echo "Bad dates on line $line: fixing: $fixDate, finding: $findDate, guilty: $guiltyDate"
            echo ",bad dates" >> $logfile
        fi
    fi

    line=$(( $line + 1 ))
done

echo "Ending at: $(date)"
echo "All Done!"
