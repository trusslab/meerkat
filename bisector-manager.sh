#!/bin/bash

set -e

source parameters/config.cfg

# input file. Made by parse/bparse.sh
bugfile=$bisectdir/parse/bugs.csv

# number to identify the bug by
number=1
wd=""
startLine=1
endLine=0

bisector=bin/bisector

# =================================================================================================
# Functions

writebugconfig () {
    echo "{" > $bisectorconfig
    echo "    \"bugID\": \"${curBug}\","  >> $bisectorconfig
    echo "    \"bug_name\": \"${bugName}\"," >> $bisectorconfig
    echo "    \"bug_link\": \"${buglink}\"," >> $bisectorconfig
    # extra config are just ignored, so anchor hash is fine
    echo "    \"anchor_hash\": \"${findhash}\"," >> $bisectorconfig
    echo "    \"arch\": \"amd64\"," >> $bisectorconfig
    echo "    \"repository\": \"${repo}\"," >> $bisectorconfig
    echo "    \"branch\": \"${branch}\"," >> $bisectorconfig
    echo "    \"kernel_config\": \"${wd}config-${curBug}.txt\"," >> $bisectorconfig
    echo "    \"reproducers\": \"${wd}reproducers/\"," >> $bisectorconfig
    echo "    \"reproducer\": \"${championrepro}\"," >> $bisectorconfig
    echo "    \"wd\": \"${wd}\"," >> $bisectorconfig
    echo "    \"home\": \"${bisectdir}\"," >> $bisectorconfig
    echo "    \"syzkaller\": \"${bisectdir}syzkaller/\"," >> $bisectorconfig
    echo "    \"compilers\": \"${gccdir}\"," >> $bisectorconfig
    echo "    \"ccache\": \"ccache\"," >> $bisectorconfig
    echo "    \"image\": \"${image}\"," >> $bisectorconfig
    echo "    \"image_key\": \"${imagekey}\"" >> $bisectorconfig
    echo "}" >> $bisectorconfig
}

printhelp () {
    echo "Help:"
    echo "    s - the line in parse/bugs.csv to start on"
    echo "    e - the last line in parse/bugs.csv to use"
    echo "    i - the id given to this manager"
    echo "    b - determine the name of the bug file in parse"
    echo "    m - the maximum time to fuzz at the finding commit"
    echo "    F - The feature string"
    echo "        [ all, default, poc-test, ff-test, setup-only, find-only, ff-no-find-backup, stateful-corpus, patch-kernel ]"
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
            bugfile="$bisectdir/parse/${OPTARG}" ;;
        m)
            mtime="-m ${OPTARG}" ;;
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

wd="${bisectdir}wd-bisector-${id}/"
bisectorconfig=${wd}bisector.cfg
logfile=${wd}log/manager.log

if [ ! -d $wd ]; then
    mkdir $wd
fi

if [ ! -d ${wd}reproducers ]; then
    mkdir ${wd}reproducers
fi

if [ ! -d ${wd}log ]; then
    mkdir ${wd}log
fi

totallines=$(cat $bugfile | wc -l)
if (( $endLine > $totallines || $startLine < 1 || $startLine > $endLine )); then
    echo "Bad start and/or end lines!"
    exit
fi

line=$startLine

echo "file: $bugfile startline: $startLine endline: $endLine" >> $logfile
echo "" >> $logfile

# batch 2, 3
# "$l,$name,$truefinddate,$fixlink,$fixdate,$config,$findlink,$finddate,$guiltylink,$guiltydate,$bit32,     ${allrepro[@]}"
# batch 1
# "$l,$name,$truefinddate,$fixlink,$fixdate,$repro, $config,  $findlink,$finddate,  $guiltylink,$guiltydate,$bit32"

# $bugNum,$l,$name,$truefinddate,$config,$findlink,$finddate,$fixlink,$fixdate,$guiltylink,$guiltydate,$bisectConverge,$bisectHash,$bisectDate,$bisectReproHash,$bisectReproDate,$bisectRepro,$bisectErr,${allrepro[@]}
# $1,     $2,$3,   $4,           $5,     $6,       $7,       $8,      $9,       $10,       $11,        $12,            $13,        $14,        $15,             $16,             $17,         $18,       $19
while (( $line <= $endLine )); do
    linetext=$(sed -n ${line}p $bugfile)
    fixDate=$(echo "$linetext" | awk -F',' '{ print $9; }')
    findDate=$(echo "$linetext" | awk -F',' '{ print $7; }')
    guiltyDate=$(echo "$linetext" | awk -F',' '{ print $11; }')
    echo -n "${line},${fixDate},${findDate},${guiltyDate}" >> $logfile

    # ignore most recent bugs as they are broken to my elfutils
    #elfutilAge=$(( $($bisectdir/helpers/diffdate "2024-01-01" $findDate) ))

    #if (( $elfutilAge > 0 )); then
    # bug number and name
    bugNum="$(printf "%04d" $(echo "$linetext" | awk -F',' '{ print $1; }'))"
    curBug="bug${bugNum}"
    bugName="$(echo "$linetext" | awk -F',' '{ print $3; }')"

    # kernel config - download it
    rm -f ${wd}config-*
    configlink=$(echo "$linetext" | awk -F',' '{ print $5; }')
    wget $configlink -O ${wd}config-${curBug}.txt 2> /dev/null

    # reproducers - download them
    rm -rf ${wd}reproducers/*
    allrepro=$(echo "$linetext" | awk -F',' '{ print $19; }')
    bisectrepro=$(echo "$linetext" | awk -F',' '{ print $17; }')
    championrepro=""
    reprocount=0
    for reprolink in ${allrepro[@]}; do
        reprocount=$(( $reprocount + 1 ))
        wget $reprolink -O ${wd}reproducers/repro-${curBug}-${reprocount}.prog 2> /dev/null
        if [[ $championrepro == "" && $reprolink == $bisectrepro ]]; then
            championrepro=repro-${curBug}-${reprocount}.prog
        fi
    done

    # linux kernel repository
    # the link to the finding commit has what we need
    findlink=$(echo "$linetext" | awk -F',' '{ print $6; }')
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
    buglink=$(echo "$linetext" | awk -F',' '{ print $2; }')

    if [[ $configlink != "" && $bugName != "" && $findlink != "" && $repo != "" && $branch != "" ]]; then
        writebugconfig

        echo ",good fuzz,$findDate" >> $logfile
        echo "./$bisector -i $id -c $bisectorconfig -a ${findhash} ${feature} $mtime $safemode" >> $logfile
        set +e
        ./$bisector -i $id -c $bisectorconfig -a ${findhash} ${feature} $mtime $safemode 2>&1 | tee ${wd}log/${curBug}.log
        set -e
        number=$(( $number + 1 ))
    else
        echo "Possible bad parse on line $line"
        echo ",bad parse" >> $logfile
    fi

    line=$(( $line + 1 ))
done

echo "Ending at: $(date)"
echo "All Done!"
