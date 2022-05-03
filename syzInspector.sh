#!/bin/bash

# quit if any errors
set -e

# =========================================================================================================================================================
# system parameters
source inspector-config/parameters.cfg

# always the same
spacer="===================================================================================================================================================="

# arguments
kernelVersion=""
syzVersion=""
startDate=""
endDate=""
findDate=""
maxtime=720
fuzztimes=3
startport=12000
port=$startport
pc=0
song=$inspectdir/Let_There_Be_Light.mp3

# flags
playmusic=0
dorepro=0
hasstart=0
hasend=0
dofind=0
findbyhash=0

# =========================================================================================================================================================
# source for go
if [[ $(echo $PATH | grep $GOROOT | cat) == "" ]]; then
    export PATH=$GOROOT/bin:$PATH
fi

# keep an old path to revert to. (after we point it at gcc)
TMPPATH=$PATH

# =========================================================================================================================================================
# functions go here

# finds the most recent kernel for kdate
# output stored in kdate and kernelVersion
findkernel () {
    # get the kernel date
    kernelVersion=$(grep -m 1 "$kdate" $inspectdir/parse/$kpref-kVersions.csv | grep -o "$kpref-[0-9a-f]*$" | cat)
    while [[ $kernelVersion == "" ]]
    do
        kdate=$($inspectdir/helpers/decdate $(date +"%Y %m %d" -d $kdate))
        kernelVersion=$(grep -m 1 "$kdate" $inspectdir/parse/$kpref-kVersions.csv | grep -o "$kpref-[0-9a-f]*$" | cat)
    done
}

# finds the most recent syzbot version for sdate
# result in sdate and syzVersion
findsyz () {
    syzVersion=$(grep -m 1 "$sdate" $inspectdir/parse/sVersions.csv | grep -o "[0-9a-f]*$" | cat)
    while [[ $syzVersion == "" ]]
    do
        sdate=$($inspectdir/helpers/decdate $(date +"%Y %m %d" -d $sdate))
        syzVersion=$(grep -m 1 "$sdate" $inspectdir/parse/sVersions.csv | grep -o "[0-9a-f]*$" | cat)
    done
}

# finds the most recent compiler for a given kernel date
prepgcc () {
    cd $gccdir
    gdate=$kdate

    if (( $($inspectdir/helpers/diffdate $gdate "2021-01-10") >= 0 )); then
        gccVersion="gcc-$(gcc --version | grep -o -m 1 "[0-9\.]*$")"
        PATH=$TMPPATH
    else
        gccVersion=$(grep -m 1 "^$gdate" gccVersions.csv | awk -F',' '{ print $2; }' | cat)
        while [[ $gccVersion == "" ]]
        do
            gdate=$($inspectdir/helpers/decdate $(date +"%Y %m %d" -d $gdate))
            gccVersion=$(grep -m 1 "^$gdate" gccVersions.csv | awk -F',' '{ print $2; }' | cat)
        done
        PATH=$gccdir/$gccVersion/bin:$TMPPATH
    fi

    cd $inspectdir
}

cleangcc () {
    PATH=$TMPPATH
}

cleankernel () {
    # clean up the kernel
    echo "Cleaning up the Kernel"
    rm -r $managerwd/kernels/$kernelVersion
}

# downloads and makes the kernel version in kernelVersion
# kernel stored in $inspectdir/kernels/$kernelVersion
kernelprep () {
    cd $managerwd
    echo "$spacer"

    # kernel checking
    cd kernels
    if [ -d "$kernelVersion" ]; then
        echo "Cleaning up old kernel"
        rm -r $kernelVersion
    fi

    # get and make the kernel
    echo "Downloading $kernelVersion"
    wget https://git.kernel.org/pub/scm/linux/kernel/git/$repo/snapshot/$kernelVersion.tar.gz
    tar -xf $kernelVersion.tar.gz
    rm $kernelVersion.tar.gz
    echo "$spacer"
    cp $kconfig $kernelVersion/.config
    cd $kernelVersion

    # apply patch from 760f8522ce08
    # long check to rule out weird half-changes
    if [[ $(grep "#include <sys/socket.h>" scripts/selinux/mdp/mdp.c) != "" && $(grep "#include <sys/socket.h>" scripts/selinux/genheaders/genheaders.c) != "" && $(grep "#include <sys/socket.h>" security/selinux/include/classmap.h) == "" ]]; then
        sed -i 's/#include <sys\/socket.h>//' scripts/selinux/mdp/mdp.c
        sed -i 's/#include <sys\/socket.h>//' scripts/selinux/genheaders/genheaders.c
        sed -i 's/#include <linux\/capability.h>/#include <linux\/capability.h>\n#include <linux\/socket.h>/' security/selinux/include/classmap.h
    fi

    # apply patch to old kernels so they boot
    if [[ $(grep "ifdef CONFIG_X86_64" arch/x86/Makefile | cat) == "" ]]; then
        echo "Applying patch to old kernel"
        sed -i '/LDFLAGS :=/r ../../../patches/patch.txt' arch/x86/Makefile
    fi

    echo "Making the kernel..."
    make -f Makefile olddefconfig
    echo "$spacer"
    make -f Makefile -j$makeproc
    cd $inspectdir
}

cleansyz () {
    # I've had enough issues with this. wipe the directory.
    cd $syzdir
    rm -r *
}

# downloads and makes the syzkaller version in syzVersion
# also grabs the template defined by curbug
# if $1 is 1, also updates the template
# output is in the syzkaller directory
syzprep () {
    # prep Syzkaller
    echo "$spacer"
    cd $syzdir

    # clean out the old version. We don't want different versions to merge.
    cleansyz

    dangerzone=0
    # there is a danger zone where go mod tidy doesn't work. This is a work-around
    if (( $($inspectdir/helpers/diffdate $sdate "2020-07-04") < 0 && $($inspectdir/helpers/diffdate $sdate "2020-04-30") >= 0 )); then
        echo "Handling \"go mod tidy\" danger zone"
        git fetch https://github.com/google/syzkaller 136082ab38d86932bc3ed0087694e99d0e55491b
        git checkout -f FETCH_HEAD
        go mod init
        go mod tidy
        go mod vendor
        dangerzone=1
        echo "$spacer"
    fi

    echo "Fetching Syzkaller version $syzVersion"
    git fetch https://github.com/google/syzkaller $syzVersion
    git checkout -f FETCH_HEAD

    echo "$spacer"
    echo "Slimming the Template"

    if [ -d $syzdir/sys/linux ]; then
        templatedir=$syzdir/sys/linux
    else
        templatedir=$syzdir/sys
    fi

    if [[ $1 -eq 1 ]]; then
        cd $managerwd
        $inspectdir/template/tparse $repro template.txt `ls $templatedir/*.txt`
        # diff may cause issues if the file does not exist
        if [ ! -f $curBug.txt ] || [[ $(diff template.txt $curBug.txt) != "" ]]; then
            mv template.txt $curBug.txt
            templateupdate=1
        else
            rm template.txt
            templateupdate=0
        fi
    else
        templateupdate=0
    fi

    cd $templatedir
    rm *.txt
    cp $managerwd/$curBug.txt ./
    cd $syzdir

    sed -i 's/\$(ADDCFLAGS) \$(CFLAGS) -DGOOS_\$(TARGETOS)=1 -DGOARCH_\$(TARGETARCH)=1/-m64 -O2 -pthread -Wall -static-pie -DGOOS_\$(TARGETOS)=1 -DGOARCH_\$(TARGETARCH)=1/' Makefile
    echo "$spacer"
    echo "Making Syzkaller..."

    # apply a patch related to kvm that causes a boot error
    # also fixed with linux-cc17b22559d9b9c..., but this was easier
    if (( $($inspectdir/helpers/diffdate $sdate "2021-01-01") < 0 && $($inspectdir/helpers/diffdate $sdate "2020-05-01") >= 0 )); then
        sed -i 's/\-enable\-kvm \-cpu host,migratable=off/\-enable\-kvm \-cpu host/' vm/qemu/qemu.go
    fi

    # apply patch for netfilter_bridge/ebtables
    # manually applying because there are too many changes across versions
    if (( $($inspectdir/helpers/diffdate $sdate "2018-09-27") < 0 && $($inspectdir/helpers/diffdate $sdate "2018-02-17") >= 0 )); then
        sed -i '/#include <linux\/netfilter_bridge\/ebtables.h>/r ../../patches/syz-1.txt' executor/common_linux.h
        sed -i 's/#include <linux\/netfilter_bridge\/ebtables.h>//' executor/common_linux.h
        sed -i 's/#include <linux\/if.h>//' executor/common_linux.h
        sed -i 's/#include <errno.h>/#include <errno.h>\n#include <linux\/if.h>/' executor/common_linux.h

        if [ -f pkg/csource/generated.go ]; then
            sed -i '/#include <linux\/netfilter_bridge\/ebtables.h>/r ../../patches/syz-2.txt' pkg/csource/generated.go
            sed -i 's/#include <linux\/netfilter_bridge\/ebtables.h>//' pkg/csource/generated.go
            sed -i 's/#include <linux\/if.h>//' pkg/csource/generated.go
            sed -i 's/#include <errno.h>/#include <errno.h>\n#include <linux\/if.h>/' pkg/csource/generated.go
        fi
    fi

    # build error with strncpy
    # works with gcc-7, broken in 8
    if (( $($inspectdir/helpers/diffdate $sdate "2018-05-13") < 0 && $($inspectdir/helpers/diffdate $sdate "2018-02-10") >= 0 )); then
        sed -i 's/NONFAILING(strncpy(buf, (char\*)a0, sizeof(buf)));/NONFAILING(strncpy(buf, (char\*)a0, sizeof(buf) - 1));/' executor/common_linux.h
        sed -i 's/NONFAILING(strncpy(buf, (char\*)a0, sizeof(buf)));/NONFAILING(strncpy(buf, (char\*)a0, sizeof(buf) - 1));/' pkg/csource/linux_common.go
    fi

    if [ -f Godeps/Godeps.json ] && (( $dangerzone == 0 )); then
        # some dependencies must be imported manually
        echo "Handling Old Modules (Godeps/Godeps.json)"
        go mod init
        go mod tidy
        go mod vendor
    fi

    # figure out a time frame for this. Always deleting is silly
    if [ -f vendor/cloud.google.com/go/storage/not_go110.go ]; then
        # we are not go 1.10 or fushia.
        # and in some earlier versions of syzkaller, this file causes problems. remove it
        rm vendor/cloud.google.com/go/storage/not_go110.go
    fi

    make -f Makefile
    echo "$spacer"
}

# outputs the current config to syzkaller
# added functionality to choose from a set of ports (been having issues with ports not being freed)
syzconfigprep () {
    # port 56741
    port=$(( $startport + $pc ))
    pc=$(( ($pc + 1) % ($fuzztimes + 1) ))

    echo -n "{ " > $syzconfig

    # "target" added on 2017-09-15
    if (( $($inspectdir/helpers/diffdate $sdate "2017-09-15") >= 0 )); then
        echo -n "\"target\": \"linux/amd64\", " >> $syzconfig
    fi

    echo -n "\"http\": \"127.0.0.1:$port\", \"workdir\": \"$kallerwd\", " >> $syzconfig

    # "vmlinux" until 2018-06-27, then "kernel_obj" starting on 2018-06-28
    if (( $($inspectdir/helpers/diffdate $sdate "2018-06-28") >= 0 )); then
        echo -n "\"kernel_obj\": \"$managerwd/kernels/$kernelVersion\", " >> $syzconfig
    else
        echo -n "\"vmlinux\": \"$managerwd/kernels/$kernelVersion/vmlinux\", " >> $syzconfig
    fi

    # change image when syzkaller did
    if (( $($inspectdir/helpers/diffdate $sdate "2018-09-04") >= 0 )); then
        echo -n "\"image\": \"$inspectdir/image/stretch/stretch.img\", \"sshkey\": \"$inspectdir/image/stretch/stretch.id_rsa\", " >> $syzconfig
    else
        echo -n "\"image\": \"$inspectdir/image/wheezy/wheezy.img\", \"sshkey\": \"$inspectdir/image/wheezy/ssh/id_rsa\", " >> $syzconfig
    fi

    echo "\"syzkaller\": \"$syzdir\", \"procs\": $numProcs, \"type\": \"qemu\", \"reproduce\": false, \"vm\": { \"count\": $numVM, \"kernel\": \"$managerwd/kernels/$kernelVersion/arch/x86/boot/bzImage\", \"cpu\": $numCPU, \"mem\": $mem }}" >> $syzconfig
}

# old style of output. changed in favor of csv
oldoutput () {
    echo "Fuzz Results: $(date +%Y-%m-%d) $(date +%H:%M:%S)" >> $outfile
    echo "Current Date: $curdate" >> $outfile
    echo "Kernel: $kernelVersion" >> $outfile
    echo "Kernel Date: $kdate" >> $outfile
    echo "Syzkaller: $syzVersion" >> $outfile
    echo "Syzkaller Date: $sdate" >> $outfile
    echo "Maximum Fuzzing Time: $maxtime minutes" >> $outfile
    echo "Time to find: $loopc minutes" >> $outfile
    echo "Crashes Found:" >> $outfile
    if [[ $(ls $inspectdir/$kallerwd/crashes) != "" ]]; then
        echo "$(tail $inspectdir/$kallerwd/crashes/*/description)" >> $outfile
    fi
    echo "$spacer" >> $outfile
}

# resets the wd and runs Syzkaller with all the params, then kills it
syzrun () {
    # reset the working directory each run
    cd $inspectdir
    if [ -d "$kallerwd" ]; then
        echo "Reseting the working directory"
        rm -rf $kallerwd
    fi
    mkdir $kallerwd
    mkdir $kallerwd/crashes

    cd $syzdir

    # fuzz time!
    echo "Running Syzbot..."
    ./bin/syz-manager -config=$syzconfig 2> $kallerout &
    syzpid=$!

    loopc=0
    out=""
    waittime=1
    out=$(grep -m 1 "$bugname" $kallerout | cat)
    # check output until bug is found or max time, then kill syzbot
    while [[ $loopc -lt $maxtime && $out == "" ]]
    do
        sleep ${waittime}m
        out=$(grep -m 1 "$bugname" $kallerout | cat)
        loopc=$(( $loopc + $waittime ))
    done

    kill $syzpid
}

# Inspects the bug at a given curdate. expects that kdate and sdate have been prepped
inspectcurdate () {
    # places to put time results
    ktimes=()
    stimes=()
    ttimes=()
    kavg=0
    savg=0
    tavg=0
    ttf=0  # time-to-find

    # empt out the crash file
    echo -n "" > $tmpbugfile

    crashes=()
    # fuzz n times for robust results
    for (( i=0; i<$fuzztimes; i++ )); do
        echo "Kernel Only try $i"

        # run syzkaller
        syzconfigprep
        syzrun

        if [[ $(ls $kallerwd/crashes) != "" ]]; then
            echo "$(cat $kallerwd/crashes/*/description)" >> $tmpbugfile
        fi

        if [[ $out != "" ]]; then
            echo "Found bug: $out"
        fi

        ktimes+=($loopc)
        echo -n ",$loopc" >> $outfile
    done

    for t in ${ktimes[@]}; do
        kavg=$(( $kavg + $t ))
    done
    kavg=$(( $kavg/${#ktimes[@]} ))
    ttf=$kavg
    echo ",$ttf" >> $outfile

    # output the crashes to the log file.
    echo "$(cat $tmpbugfile | sort | uniq -c)" >> $outfile

    # Update Syzkaller and fuzz again
    # if there is a syzkaller update for curdate, set it
    sv=$(grep "$curdate" $inspectdir/parse/sVersions.csv | grep -o "[0-9a-f]*$" | cat)
    if [[ $sv != "" ]]; then
        syzVersion=$sv
        sdate=$curdate

        echo -n "$curdate,$gccVersion,$kernelVersion,$kdate,$syzVersion,$sdate,$maxtime" >> $outfile

        # update syzkaller, but not the template yet
        syzprep 0

        echo -n "" > $tmpbugfile

        crashes=()
        # fuzz n times for robust results
        for (( i=0; i<$fuzztimes; i++ )); do
            echo "Kernel and Syzkaller try $i"

            # run syzkaller
            syzconfigprep
            syzrun

            if [[ $(ls $kallerwd/crashes) != "" ]]; then
                echo "$(cat $kallerwd/crashes/*/description)" >> $tmpbugfile
            fi

            if [[ $out != "" ]]; then
                echo "Found bug: $out"
            fi

            stimes+=($loopc)
            echo -n ",$loopc" >> $outfile
        done

        for t in ${stimes[@]}; do
            savg=$(( $savg + $t ))
        done
        savg=$(( $savg / ${#stimes[@]} ))
        echo ",$savg" >> $outfile

        echo "$(cat $tmpbugfile | sort | uniq -c)" >> $outfile
    fi

    # update the template and fuzz again
    syzprep 1

    if (( $templateupdate == 1 )); then
        echo -n "$curdate,$gccVersion,$kernelVersion,$kdate,$syzVersion,$sdate,$maxtime" >> $outfile

        echo -n "" > $tmpbugfile

        crashes=()
        # fuzz n times for robust results
        for (( i=0; i<$fuzztimes; i++ )); do
            echo "Kernel, Syzkaller, and Template try $i"

            # run syzkaller
            syzconfigprep
            syzrun

            if [[ $(ls $kallerwd/crashes) != "" ]]; then
                echo "$(cat $kallerwd/crashes/*/description)" >> $tmpbugfile
            fi

            if [[ $out != "" ]]; then
                echo "Found bug: $out"
            fi

            ttimes+=($loopc)
            echo -n ",$loopc" >> $outfile
        done

        for t in ${ttimes[@]}; do
            tavg=$(( $tavg + $t ))
        done
        tavg=$(( $tavg / ${#ttimes[@]} ))
        echo ",$tavg" >> $outfile

        echo "$(cat $tmpbugfile | sort | uniq -c)" >> $outfile
    fi

    mttf=$ttf
    cleankernel
}

# =========================================================================================================================================================
# Begin Main Program

# get the start and end dates from the arguments
while getopts "s:e:f:F:m:i:p" flag
do
    case $flag in
        s)
            hasstart=1
            startDate="${OPTARG}" ;;
        e)
            hasend=1
            endDate="${OPTARG}" ;;
        f)
            dofind=1
            findDate="${OPTARG}" ;;
        F)
            findbyhash=1
            findhash="${OPTARG}" ;;
        m)
            maxtime=${OPTARG} ;;
        i)
            id=${OPTARG} ;;
        p)
            playmusic=1 ;;
        *)
            echo "Bad Arguments!"
            exit
    esac
done

# current bug parameters
source wd-inspector-$id/bug.cfg
outfile=$managerwd/log/$curBug-log.csv
kallerout=$managerwd/$curBug-kaller-log.txt
tmpbugfile=$managerwd/crash_tmp.txt

# environment checking. the manager should have already made managerwd
if [ ! -d "$managerwd/log" ]; then
    echo "Creating log directory."
    mkdir $managerwd/log
fi

if [ ! -d "$managerwd/kernels" ]; then
    echo "Creating kernel directory."
    mkdir $managerwd/kernels
fi

if [ ! -f "$kconfig" ]; then
    echo "There is no kernel config for this bug!"
    exit
fi

if [ ! -f "$repro" ]; then
    echo "There is no reproducer for this bug!"
    exit
fi

# need to use our own syzkaller directory when there are multiple instances running
if [ ! -d $syzdir ]; then
    echo "Prepping Syzkaller"
    mkdir $syzdir
    cd $syzdir
    git init
    git pull https://github.com/google/syzkaller
fi

startport=$(( $startport + $id * ($fuzztimes + 1) ))

echo "$bugname,$buglink" > $outfile

# fuzz at the finding commit to get the maximum time
if [[ $dofind -eq 1 ]]; then
    echo "Fuzzing at the finding commit..."
    echo "Finding Commit" >> $outfile

    echo -n "" > $tmpbugfile

    curdate="$findDate"
    kdate="$findDate"
    if (( findbyhash == 1 )); then
        kernelVersion=$kpref-$findhash
    else
        findkernel
    fi

    sdate="$findDate"
    findsyz

    syzconfigprep

    prepgcc

    echo -n "$curdate,$gccVersion,$kernelVersion,$kdate,$syzVersion,$sdate,$maxtime" >> $outfile

    kernelprep

    cleangcc

    syzprep 1

    found=0
    ftimes=()
    crashes=()
    # run 3 times.
    for (( i=0; i<3; i++ )); do
        syzconfigprep
        syzrun

        if [[ $(ls $kallerwd/crashes) != "" ]]; then
            echo "$(cat $kallerwd/crashes/*/description)" >> $tmpbugfile
        fi

        echo -n ",$loopc" >> $outfile

        if (( $loopc < $maxtime )); then
            found=1
        fi

        ftimes+=($loopc)
    done
    echo "" >> $outfile

    echo "$(cat $tmpbugfile | sort | uniq -c)" >> $outfile

    # if the bug was found at least once
    if [[ $found == 1 ]]; then
        # take the maximum fuzzing time times 1.5
        maxtime=$(( $($inspectdir/helpers/findmaxtime ${ftimes[@]}) ))
        echo "Using maximum fuzzing time $maxtime"
        echo "Max Time,$maxtime" >> $outfile
    else
        echo "Bug was not found. Trying for 1 day"
        maxtime=1440

        echo "Last ditch fuzz,$maxtime" >> $outfile
        echo -n "$curdate,$gccVersion,$kernelVersion,$kdate,$syzVersion,$sdate,$maxtime" >> $outfile

        echo -n "" > $tmpbugfile

        syzconfigprep
        syzrun

        echo ",$loopc" >> $outfile

        crashes=()

        if [[ $(ls $kallerwd/crashes) != "" ]]; then
            "echo $(cat $kallerwd/crashes/*/description)" >> $tmpbugfile
        fi

        echo "$(cat $tmpbugfile | sort | uniq -c)" >> $outfile

        cleankernel

        if [[ $found == 0 ]]; then
            # if the bug was never found
            echo "This bug is too hard to find!"
            echo "This bug was not found!" >> $outfile
            if (( $playmusic == 1 )); then
                play -q $song -V1 -t alsa
            fi
            exit
        else
            echo "This bug takes a little too long to find"
            echo "This bug takes too long to find" >> $outfile
            exit
        fi
    fi

    cleankernel
else
    echo "Max Time,$maxtime" >> $outfile
fi

# Begin main fuzzing loop

if [[ $hasend -ne 1 ||  $hasstart -ne 1 ]]; then
    echo "Starting or Ending Date not given. Stopping here."
    if (( $playmusic == 1 )); then
        play -q $song -V1 -t alsa
    fi
    exit
fi

# startDate is 0, endDate is the upper bound of the range
daterange=$($inspectdir/helpers/diffdate $endDate $startDate)

if (( $daterange <= 1 )); then
    echo "Bad start and end dates given. Stopping here."
    exit
fi

# left is guilty (startDate), right is finding (endDate)
l=0
r=$daterange
m=$(( ($l + $r) / 2 ))
curdate=$($inspectdir/helpers/dpd $startDate $m)

while (( $l < $r ))
do
    kdate=$curdate
    findkernel

    sdate=$curdate
    findsyz

    syzconfigprep

    prepgcc

    echo -n "$curdate,$gccVersion,$kernelVersion,$kdate,$syzVersion,$sdate,$maxtime" >> $outfile

    kernelprep

    cleangcc

    syzprep 1

    echo -n "" > $tmpbugfile
    crashes=()
    ktimes=()
    kavg=0
    for (( i=0; i<$fuzztimes; i++ )); do
        # run syzkaller
        syzconfigprep
        syzrun

        if [[ $(ls $kallerwd/crashes) != "" ]]; then
            echo "$(cat $kallerwd/crashes/*/description)" >> $tmpbugfile
        fi

        if [[ $out != "" ]]; then
            echo "Found bug: $out"
        fi

        ktimes+=($loopc)
        echo -n ",$loopc" >> $outfile
    done
    cleankernel

    for t in ${ktimes[@]}; do
        kavg=$(( $kavg + $t ))
    done
    kavg=$(( $kavg/${#ktimes[@]} ))
    ttf=$kavg
    echo "" >> $outfile

    echo "$(cat $tmpbugfile | sort | uniq -c)" >> $outfile

    if (( $ttf < $maxtime )); then
        r=$(( $m - 1 ))
    else
        l=$(( $m + 1 ))
    fi

    m=$(( ($l + $r) / 2 ))
    curdate=$($inspectdir/helpers/dpd $startDate $m)

    echo "$spacer"
done

# Check back 10 days to verify results
backdate=$curdate
for (( i=0; i<10; i++ )); do
    loopc=0
    backdate=$($inspectdir/helpers/decdate $(date +"%Y %m %d" -d $backdate))

    # don't go past the guilty date
    if (( $($inspectdir/helpers/diffdate $backdate $startDate) < 0 )); then
        break
    fi

    # only check unchecked days
    if [[ $(grep "^$backdate" $outfile) != "" ]]; then
        continue
    fi

    kdate=$backdate
    findkernel

    sdate=$backdate
    findsyz

    syzconfigprep

    prepgcc

    echo -n "$backdate,$gccVersion,$kernelVersion,$kdate,$syzVersion,$sdate,$maxtime" >> $outfile

    kernelprep

    cleangcc

    syzprep 1

    echo -n "" > $tmpbugfile
    crashes=()
    ktimes=()
    kavg=0
    for (( i=0; i<$fuzztimes; i++ )); do
        # run syzkaller
        syzconfigprep
        syzrun

        if [[ $(ls $kallerwd/crashes) != "" ]]; then
            echo "$(cat $kallerwd/crashes/*/description)" >> $tmpbugfile
        fi

        if [[ $out != "" ]]; then
            echo "Found bug: $out"
        fi

        ktimes+=($loopc)
        echo -n ",$loopc" >> $outfile
    done
    cleankernel
    echo "" >> $outfile

    echo "$(cat $tmpbugfile | sort | uniq -c)" >> $outfile

    # if we find the bug, restart the loop
    if (( $loopc < $maxtime && $loopc != 0 )); then
        curdate=$backdate
        i=0
    fi
done

# Inspect the current date (current date is the one we converged to)
echo "Now inspecting the revealing date"
echo "Revealing Date" >> $outfile
kdate=$curdate
findkernel

sdate=$($inspectdir/helpers/decdate $(date +"%Y %m %d" -d $curdate))
findsyz

syzconfigprep

prepgcc

echo -n "$curdate,$gccVersion,$kernelVersion,$kdate,$syzVersion,$sdate,$maxtime" >> $outfile

kernelprep

cleangcc

syzprep 1

inspectcurdate

# clean up some stuff
export PATH=$TMPPATH

if (( $playmusic == 1 )); then
    play -q $song -V1 -t alsa
fi
