#!/bin/bash

# Joseph Bursey <jbursey@uci.edu>

# This script runs meerkat based on the arguments provided.
# Bug data is taken from a specified bug file, rather than manually entered data.

set -e

self=$0

bisectdir="$(pwd)/"
compilers=${bisectdir}compilers/
image=${bisectdir}image/stretch/stretch.img
imagekey=${bisectdir}image/stretch/stretch.id_rsa
bugfile=${bisectdir}/parse/bugs.csv

# number to identify the bug by
number=1
wd=""
startLine=1
endLine=0

projectname=meerkat
bisector=bin/${projectname}

# =================================================================================================
# Functions

writebugconfig () {
    echo "{" > $bisectorconfig
    echo "    \"bugID\": \"${curBug}\","  >> $bisectorconfig
    # bug name is now optional thanks to aliases
    echo "    \"bug_name\": \"${bugName}\"," >> $bisectorconfig
    echo "    \"aliases\": \"${wd}bugs/\"," >> $bisectorconfig
    echo "    \"bug_link\": \"${buglink}\"," >> $bisectorconfig
    # extra config are just ignored, so anchor hash is fine
    echo "    \"anchor_hash\": \"${findhash}\"," >> $bisectorconfig
    echo "    \"arch\": \"amd64\"," >> $bisectorconfig
    echo "    \"repository\": \"${repo}\"," >> $bisectorconfig
    echo "    \"branch\": \"${branch}\"," >> $bisectorconfig
    echo "    \"kernel_config\": \"${wd}config-${curBug}.txt\"," >> $bisectorconfig
    echo "    \"reproducers\": \"${wd}reproducers/\"," >> $bisectorconfig
    echo "    \"reproducer\": \"${primaryrepro}\"," >> $bisectorconfig
    echo "    \"wd\": \"${wd}\"," >> $bisectorconfig
    echo "    \"home\": \"${bisectdir}\"," >> $bisectorconfig
    echo "    \"syzkaller\": \"${bisectdir}syzkaller/\"," >> $bisectorconfig
    echo "    \"compilers\": \"${compilers}\"," >> $bisectorconfig
    echo "    \"compiler\": \"gcc\"," >> $bisectorconfig # keep gcc the default until further notice
    echo "    \"ccache\": \"ccache\"," >> $bisectorconfig
    echo "    \"image\": \"${image}\"," >> $bisectorconfig
    echo "    \"image_key\": \"${imagekey}\"" >> $bisectorconfig
    echo "}" >> $bisectorconfig
}

writebugalias () {
    # so far, mk-manager just needs to handle the bug name. no reports, only one bug.
    if [ -d ${wd}/bugs/ ]; then
        rm -rf ${wd}/bugs/*
    fi

    mkdir -p ${wd}/bugs/primary/
    echo "${bugName}" > ${wd}/bugs/primary/description
    wget "${reportlink}" -O ${wd}/bugs/primary/report 2> /dev/null
}

printhelp () {
    echo "Usage: ${self} [-seibmF]"
    echo "    s - [int] the line in parse/bugs.csv to start on"
    echo "    e - [int] the last line in parse/bugs.csv to use"
    echo "    i - [int] the id given to this manager"
    echo "    b - [file] determine the name of the bug file"
    echo "    m - [int] the maximum time to fuzz at the finding commit"
    echo "    F - [string] The feature string"
    echo "        [ all, default, poc-test, ff-test, setup-only, find-only, poc-all-pocs, ff-no-find-backup, stateful-corpus, no-patch-kernel ]"
}

# =================================================================================================

mtime=""
feature=""

# get the start and end lines from the arguments
while getopts "s:e:i:b:m:F:" flag
do
    case $flag in
        s)
            startLine="${OPTARG}" ;;
        e)
            endLine="${OPTARG}" ;;
        i)
            id="${OPTARG}" ;;
        b)
            bugfile="${OPTARG}" ;;
        m)
            mtime="-m ${OPTARG}" ;;
        F)
            feature="-F ${OPTARG}" ;;
        *)
            printhelp
            exit
    esac
done

if (( endLine == 0 )); then
    endLine=$(( $(cat ${bugfile} | wc -l) ))
fi

if [[ ${id} == "" ]]; then
    echo "No id given. Use -i <id>"
    exit
fi

echo "Starting at: $(date)"

wd="${bisectdir}wd-${projectname}-${id}/"
bisectorconfig=${wd}${projectname}.cfg
logfile=${wd}log/manager.log

if [ ! -d $wd ]; then
    mkdir ${wd}
fi

if [ ! -d ${wd}reproducers ]; then
    mkdir ${wd}reproducers
fi

if [ ! -d ${wd}log ]; then
    mkdir ${wd}log
fi

if [ ! -d ${wd}old ]; then
    mkdir ${wd}old
fi

totallines=$(cat ${bugfile} | wc -l)
if (( ${endLine} > ${totallines} || ${startLine} < 1 || ${startLine} > ${endLine} )); then
    echo "Bad start and/or end lines!"
    exit
fi

line=${startLine}

echo "file: ${bugfile} startline: ${startLine} endline: ${endLine}" >> $logfile
echo "" >> $logfile

# $bugNum,$l,$name,$truefinddate,$config,$report,$findlink,$finddate,$fixlink,$fixdate,$guiltylink,$guiltydate,$bisectConverge,$bisectHash,$bisectDate,$bisectReproHash,$bisectReproDate,$bisectRepro,$bisectErr,${allrepro[@]}
# $1,     $2,$3,   $4,           $5,     $6,     $7,       $8,       $9,      $10,      $11,       $12,        $13,            $14,        $15,        $16,             $17,             $18,         $19        $20
while (( $line <= $endLine )); do
    linetext=$(sed -n ${line}p ${bugfile})
    fixDate=$(echo "${linetext}" | awk -F',' '{ print $10; }')
    findDate=$(echo "${linetext}" | awk -F',' '{ print $8; }')
    guiltyDate=$(echo "${linetext}" | awk -F',' '{ print $12; }')
    echo -n "${line},${fixDate},${findDate},${guiltyDate}" >> $logfile

    # bug number and name
    bugNum="$(printf "%04d" $(echo "${linetext}" | awk -F',' '{ print $1; }'))"
    curBug="bug${bugNum}"
    bugName="$(echo "${linetext}" | awk -F',' '{ print $3; }')"

    # kernel config - download it
    rm -f ${wd}config-*
    configlink=$(echo "${linetext}" | awk -F',' '{ print $5; }')
    wget ${configlink} -O ${wd}config-${curBug}.txt 2> /dev/null

    reportlink=$(echo "${linetext}" | awk -F',' '{ print $6; }')

    # reproducers - download them
    rm -rf ${wd}reproducers/*
    allrepro=$(echo "${linetext}" | awk -F',' '{ print $20; }')
    bisectrepro=$(echo "${linetext}" | awk -F',' '{ print $18; }')
    primaryrepro=""
    reprocount=0
    for reprolink in ${allrepro[@]}; do
        reprocount=$(( ${reprocount} + 1 ))
        wget ${reprolink} -O ${wd}reproducers/repro-${curBug}-${reprocount}.prog 2> /dev/null
        if [[ ${primaryrepro} == "" && ${reprolink} == ${bisectrepro} ]]; then
            primaryrepro=repro-${curBug}-${reprocount}.prog
        fi
    done

    # linux kernel repository
    # the link to the finding commit has what we need
    findlink=$(echo "${linetext}" | awk -F',' '{ print $7; }')
    repo=$(echo "${findlink}" | grep -o "https://git\.kernel.*\.git" | cat)
    shortrepo=$(echo "${repo}" | awk -F'/' '{ print $9"/"$10; }' | cat)
    if [[ ${shortrepo} == "torvalds/linux.git" ]]; then
        branch="master"
    else
        echo "Bad repository on line ${line}: ${repo}"
        branch=""
    fi

    # clean up the bug name if it is a duplicate name (i.e. "bugname (2)")
    if [[ $(echo "${bugName}" | grep "([0-9]*)$" | cat) != "" ]]; then
        bugName=${bugName::-4}
    fi

    findhash=$(echo ${findlink} | grep -o "[0-9a-f]*$" | cat)
    buglink=$(echo "${linetext}" | awk -F',' '{ print $2; }')

    if [[ ${configlink} != "" && ${bugName} != "" && ${findlink} != "" && ${repo} != "" && ${branch} != "" ]]; then
        writebugconfig
        writebugalias

        echo ",good fuzz,${findDate}" >> $logfile
        echo "./${bisector} -i ${id} -c ${bisectorconfig} -a ${findhash} ${feature} ${mtime}" >> $logfile
        set +e
        ./${bisector} -i ${id} -c ${bisectorconfig} -a ${findhash} ${feature} ${mtime} 2>&1 | tee ${wd}log/${curBug}.log
        set -e
        cp ${wd}reproducers/* ${wd}old/
        number=$(( ${number} + 1 ))
        echo "================================================="
    else
        echo "Possible bad parse on line ${line}"
        echo ",bad parse" >> $logfile
    fi

    line=$(( ${line} + 1 ))
done

echo "Ending at: $(date)"
echo "All Done!"
