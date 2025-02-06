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

retrospector=bin/bisector

# =================================================================================================
# Functions

writebugconfig () {
    echo "{" > $inspectorconfig
    echo "    \"bugID\": \"${curBug}\","  >> $inspectorconfig
    echo "    \"bug_name\": \"${bugName}\"," >> $inspectorconfig
    echo "    \"bug_link\": \"${buglink}\"," >> $inspectorconfig
    # extra config are just ignored, so anchor hash is fine
    echo "    \"anchor_hash\": \"${findhash}\"," >> $inspectorconfig
    echo "    \"arch\": \"${arch}\"," >> $inspectorconfig
    echo "    \"repository\": \"${repo}\"," >> $inspectorconfig
    echo "    \"branch\": \"${branch}\"," >> $inspectorconfig
    echo "    \"kernel_config\": \"${wd}config-${curBug}.txt\"," >> $inspectorconfig
    echo "    \"reproducers\": \"${wd}reproducers/\"," >> $inspectorconfig
    echo "    \"wd\": \"${wd}\"," >> $inspectorconfig
    echo "    \"home\": \"${inspectdir}\"," >> $inspectorconfig
    echo "    \"syzkaller\": \"${inspectdir}syzkaller/\"," >> $inspectorconfig
    echo "    \"compilers\": \"${gccdir}\"," >> $inspectorconfig
    # TODO: Maybe change this to image and key
    echo "    \"images\": \"${imagedir}\"" >> $inspectorconfig
    echo "}" >> $inspectorconfig    
}

printhelp () {
    echo "Help:"
    echo "    s - the line in parse/bugs.csv to start on"
    echo "    e - the last line in parse/bugs.csv to use"
    echo "    i - the id given to this manager"
    echo "    b - determine the name of the bug file in parse"
    echo "    m - the maximum time to fuzz at the finding commit"
    echo "    a - the arch to build on (amd64/i386)"
    echo "    F - The feature string"
    echo "    S - run in safe mode"
}

# =================================================================================================

mtime=""
feature=""
safemode=""
targetarch="amd64" # i386

# get the start and end lines from the arguments
while getopts "s:e:i:b:m:a:F:S" flag
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
        F)
            feature="-F ${OPTARG}" ;;
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

wd="${inspectdir}wd-inspector-${id}/"
inspectorconfig=${wd}bisector.cfg
logfile=${wd}log/manager.log

if [ ! -d $wd ]; then
    echo "Making the working directory: $wd"
    mkdir $wd
fi

if [ ! -d ${wd}reproducers ]; then
    echo "Making the reproducer directory: ${wd}reproducers"
    mkdir ${wd}reproducers
fi

if [ ! -d ${wd}log ]; then
    echo "Making the log directory"
    mkdir ${wd}log
fi

totallines=$(cat $bugfile | wc -l)
if (( $endLine > $totallines || $startLine < 1 || $startLine > $endLine )); then
    echo "Bad start and/or end lines!"
    exit
fi

line=$startLine

echo "file: $bugfile startline: $startLine endline: $endLine arch: $targetarch" >> $logfile
echo "" >> $logfile

# TODO: Move/Remove old files from previous bugs

# TODO: map out new csv layout

# batch 2, 3
# "$l,$name,$truefinddate,$fixlink,$fixdate,$config,$findlink,$finddate,$guiltylink,$guiltydate,$bit32,     ${allrepro[@]}"
# batch 1
# "$l,$name,$truefinddate,$fixlink,$fixdate,$repro, $config,  $findlink,$finddate,  $guiltylink,$guiltydate,$bit32"
#  $1,$2,   $3,           $4,      $5,      $6,     $7,       $8,       $9,         $10,        $11,        $12
while (( $line <= $endLine )); do
    linetext=$(sed -n ${line}p $bugfile)
    fixDate=$(echo "$linetext" | awk -F',' '{ print $5; }')
    findDate=$(echo "$linetext" | awk -F',' '{ print $8; }')
    guiltyDate=$(echo "$linetext" | awk -F',' '{ print $10; }')
    echo -n "${line},${fixDate},${findDate},${guiltyDate}" >> $logfile

    arch=$(echo "$linetext" | awk -F',' '{ print $11; }')

    # TODO: filter bugs so these checks are not necessary
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

    # ignore most recent bugs as they are broken to my elfutils
    elfutilAge=$(( $($inspectdir/helpers/diffdate "2024-01-01" $findDate) ))

    # check that the time to find is good and interesting
    findAge=$(( $($inspectdir/helpers/diffdate $findDate $startDate) ))

    if (( $findAge > 1 && $fixAge >= 0 )) && [[ $arch == $targetarch ]] && (( $elfutilAge > 0 )); then
        # bug number and name
        bugNum=${line} #"$(printf "%04d" $(echo "$linetext" | awk -F',' '{ print $1; }'))"
        curBug="bug${bugNum}"
        bugName="$(echo "$linetext" | awk -F',' '{ print $2; }')"
        if [[ $(echo "${bugName}" | grep "^KMSAN" | cat) != "" ]]; then
            echo ",KMSAN bug. Skip until clang" >> $logfile
            echo "KMSAN bug on line ${line}"
            line=$(( $line + 1 ))
            continue
        fi

        # kernel config - download it
        configlink=$(echo "$linetext" | awk -F',' '{ print $6; }')
        rm -f ${wd}config-*
        wget $configlink -O ${wd}config-${curBug}.txt 2> /dev/null

        # reproducers - download them
        rm -rf ${wd}reproducers/*
        allrepro=($(echo "$linetext" | awk -F',' '{ print $12; }'))
        reprocount=0
        for reprolink in ${allrepro[@]}; do
            reprocount=$(( $reprocount + 1 ))
            wget $reprolink -O ${wd}reproducers/repro-${curBug}-${reprocount}.prog 2> /dev/null
        done

        # linux kernel repository
        # the link to the finding commit has what we need
        findlink=$(echo "$linetext" | awk -F',' '{ print $7; }')
        repo=$(echo "$findlink" | grep -o "https://git\.kernel.*\.git" | cat)
        shortrepo=$(echo "$repo" | awk -F'/' '{ print $9"/"$10; }' | cat)
        if [[ $shortrepo == "torvalds/linux.git" ]]; then
            branch="master"
        else
            echo "Bad repository on line $line: $repo"
            branch=""
        fi

        # clean up the bug name if it is a duplicate name (i.e. "bugname (2)")
        if [[ $(echo "$bugName" | grep "([0-9]*)$" | cat) != "" ]]; then
            bugName=${bugName::-4}
        fi

        findhash=$(echo $findlink | grep -o "[0-9a-f]*$" | cat)
        buglink=$(echo "$linetext" | awk -F',' '{ print $1; }')

        if [[ $configlink != "" && $bugName != "" && $findlink != "" && $repo != "" && $branch != "" ]]; then
            writebugconfig

            echo ",good fuzz,$findDate" >> $logfile
            echo "./$retrospector -i $id -c $inspectorconfig -a ${findhash} $mtime $safemode" >> $logfile
            set +e
            ./$retrospector -i $id -c $inspectorconfig -a ${findhash} ${feature} $mtime $safemode
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
