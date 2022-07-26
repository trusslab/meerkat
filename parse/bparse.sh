#!/bin/bash

outfile=bugs.csv
debugfile=debug.txt

link=https://syzkaller.appspot.com/upstream/fixed

while getopts "o:" flag
do
    case $flag in
        o)
            outfile="${OPTARG}" ;;
        *)
            exit
    esac
done

echo "Looking for bugs..."
snapshot=$(lynx -dump -dont_wrap_pre -width=300 $link)
alllinks=$(echo "$snapshot" | grep "https:\/\/syzkaller\.appspot\.com\/bug?id\=[0-9a-f]*$" | cat)
goodbugs=$(echo "$snapshot" | grep -E ' C | syz ' | cat)

echo "Found $(echo "$goodbugs" | wc -l) bugs to parse!"

linknums=$(echo "$goodbugs" | grep -o "^[ ]*\[[0-9]*\]" | grep -o "[0-9]*" | cat)
linknumarr=()
buglinks=()
badbugs=()
readarray -t linknumarr <<< "$linknums"

echo "Getting some links..."
for n in "${linknumarr[@]}"; do
    buglinks+=("$(echo "$alllinks" | grep "^[ ]*$n\. https" | grep -o "https:\/\/syzkaller\.appspot\.com\/bug?id\=[0-9a-f]*$" | cat)")
done
echo "Found ${#buglinks[@]} links!"

echo "Parsing the bugs..."
echo -n "" > $outfile   # clear out the output file
echo -n "" > $debugfile
bad=0
noguilty=0
count=0
bit32="amd64"
upstream="false"
for l in ${buglinks[@]}; do
    echo "==============================================================================================" >> $debugfile
    bsnapshot=$(lynx -dump -dont_wrap_pre -width=300 $l)

    # grab the bug name
    name=$(echo "$bsnapshot" | sed -n '7p' | sed 's/^[ ]*//' | cat)
    echo $name >> $debugfile

    # grab the fixing commits
    fixnums=$(echo "$bsnapshot" | grep "Fix commit: " | grep -o "\[[0-9]*\]" | grep -o "[0-9]*" | cat)
    fixhashes=()
    for n in $fixnums; do
        fixhashes+=($(echo "$bsnapshot" | grep -m 1 "^[ ]*$n\." | grep -o "[0-9a-f]*$" | cat))
    done

    echo "${#fixhashes[@]} Fixing Commits Found" >> $debugfile

    # decide on a good crash
    # get a list of the crashes
    precrash=$(echo "$bsnapshot" | grep "\[[0-9]*\]\.config[ ]*\[[0-9]*\]log[ ]*\[[0-9]*\]report" | cat)

    truefinddate=$(echo "$precrash" | sort -k3 | sort -k2 | head -n 1 | grep -o "20[0-9][0-9]\/[0-9][0-9]\/[0-9][0-9]" | cat)

    # get only crashes with syz reproducers
    precrash=$(echo "$precrash" | grep "\[[0-9]*\]syz" | cat)

    # get only crashes in upstream
    precrash=$(echo "$precrash" | grep " upstream " | cat)

    # sort crashes by date and take the oldest
    crash=$(echo "$precrash" | sort -k3 | sort -k2 | head -n 1 | cat)

    # report if the bug is 32 bit
    if [[ $(echo "$crash" | grep "\-386" | cat) == "" ]]; then
        bit32="amd64"
    else
        bit32="i386"
    fi

    # grab the syz repro
    repronum=$(echo "$crash" | grep -o "\[[0-9]*\]syz" | grep -o "[0-9]*" | cat)
    repro=$(echo "$bsnapshot" | grep "^[ ]*$repronum\. https" | grep -o -m 1 "https:\/\/syzkaller\.appspot\.com\/text?tag\=ReproSyz\&x\=[0-9a-f]*$" | cat)
    if [[ $repro != "" ]]; then
        echo "Repro Found" >> $debugfile
    fi

    # grab the config link
    confignum=$(echo "$crash" | grep -o "\[[0-9]*\]\.config " | grep -o "[0-9]*" | cat)
    config=$(echo "$bsnapshot" | grep "^[ ]*$confignum\. https" | grep -o -m 1 "https:\/\/syzkaller\.appspot\.com\/text?tag\=KernelConfig\&x\=[0-9a-f]*$" | cat)

    if [[ $config != "" ]]; then
        echo "Config Found" >> $debugfile
    fi

    # grab the finding commit
    findnum=$(echo "$crash" | grep -o "\[[0-9]*\][a-f0-9]*" | grep -m 1 -o "\[[0-9]*\]" | grep -o "[0-9]*" | cat)
    findlink=$(echo "$bsnapshot" | grep -m 1 "[ ]*$findnum\. https" | grep -o "https.*$" | cat)

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
        echo "Bad Repository: $repo" >> $debugfile
        echo "$findlink" >> $debugfile
        bad=$(($bad + 1))
        badbugs+=($l)
        kpref=""
        continue
    fi

    echo $repo >> $debugfile
    echo $kpref >> $debugfile

    # grab the finding date
    finddate=$(echo "$crash" | grep -o "20[0-9][0-9]\/[0-9][0-9]\/[0-9][0-9]" | cat)

    # get the fixdates and guilty commits/dates
    fixdates=()
    guiltyhashes=()
    fixlinks=()
    for fhash in ${fixhashes[@]}; do
        fsnapshot=$(lynx -dump -dont_wrap_pre -width=300 https://git.kernel.org/pub/scm/linux/kernel/git/$repo/commit/?id\=$fhash)
        fixdates+=($(echo "$fsnapshot" | grep -m 1 "^[ ]*committer " | grep -o "20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]" | cat))
        fixlinks+=("https://git.kernel.org/pub/scm/linux/kernel/git/$repo/commit/?id\=$fhash")
        tmphashes=$(echo "$fsnapshot" | grep -o "Fixes: [0-9a-f]* " | awk -F' ' '{ print $2; }' | cat)
        # put all the guilty hashes in one place in one format
        for s in $tmphashes; do
            guiltyhashes+=($s)
        done
    done

    echo "${#guiltyhashes[@]} Guilty Commits Found" >> $debugfile

    # check for a bad parse
    if [[ $name == "" || $crash == "" || $repro == "" || $config == "" || $findlink == "" || $finddate == "" || $repo == "" ]] || (( ${#fixhashes[@]} == 0)); then
        if [[ $name == "" ]]; then
            echo "Bad Name" >> $debugfile
        elif [[ $crash == "" ]]; then
            echo "Bad Crash" >> $debugfile
        elif [[ $repro == "" ]]; then
            echo "Bad Reproducer" >> $debugfile
        elif [[ $config == "" ]]; then
            echo "Bad Config" >> $debugfile
        elif [[ $findlink == "" ]]; then
            echo "Bad Finding Link" >> $debugfile
        elif [[ $finddate == "" ]]; then
            echo "Bad Finding Date" >> $debugfile
        elif [[ $repo == "" ]]; then
            echo "Bad Repository" >> $debugfile
        elif (( ${#fixhashes[@]} == 0)); then
            echo "No Fixing Commits" >> $debugfile
        else
            echo "Other Reason" >> $debugfile
        fi

        bad=$(($bad + 1))
        badbugs+=($l)
        continue
    else
        # make sure the guilty commit exists
        if (( ${#guiltyhashes[@]} == 0 )); then
            echo "No Guilty Commits" >> $debugfile
            noguilty=$(($noguilty + 1))
            continue
        else
            # continue grabbing guilty commit stuff
            guiltylinks=()
            guiltydates=()
            for ghash in ${guiltyhashes[@]}; do
                snapshot=$(lynx -dump -dont_wrap_pre -width=300 https://git.kernel.org/pub/scm/linux/kernel/git/$repo/log/?qt\=range\&q\=$ghash)
                fullghash=$(echo "$snapshot" | grep -o "$ghash[0-9a-f]*$" | cat)
                snapshot=$(lynx -dump -dont_wrap_pre -width=300 https://git.kernel.org/pub/scm/linux/kernel/git/$repo/commit/?id\=$fullghash)
                guiltylinks+=("https://git.kernel.org/pub/scm/linux/kernel/git/$repo/commit/?id\=$fullghash")
                guiltydates+=($(echo "$snapshot" | grep -m 1 "^[ ]*committer " | grep -o "20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]" | cat))
            done
        fi

        if (( ${#guiltylinks[@]} == 0 || ${#guiltydates[@]} == 0 )); then
            if (( ${#guiltylinks[@]} == 0 )); then
                echo "No Guilty Links" >> $debugfile
            elif (( ${#guiltydates[@]} == 0 )); then
                echo "No Guilty Dates" >> $debugfile
            fi
            bad=$(($bad + 1))
            badbugs+=($l)
            continue
        fi

        # find earliest guilty and latest fixing
        echo "${guiltydates[@]}" >> $debugfile
        earlyguilty=0
        for (( i=1; i<${#guiltydates[@]}; i++ )); do
            if (( $(../helpers/diffdate ${guiltydates[$earlyguilty]} ${guiltydates[$i]}) > 0 )); then
                earlyguilty=$i
            fi
        done

        guiltydate=${guiltydates[$earlyguilty]}
        guiltylink=${guiltylinks[$earlyguilty]}
        if [[ $guiltylink == "" ]]; then
            echo "Bad Guilty Link" >> $debugfile
            bad=$(($bad + 1))
            badbugs+=($l)
            continue
        fi
        echo "$guiltydate" >> $debugfile

        echo "${fixdates[@]}" >> $debugfile
        latefix=0
        for (( i=1; i<${#fixdates[@]}; i++ )); do
            if (( $(../helpers/diffdate ${fixdates[$latefix]} ${fixdates[$i]}) < 0 )); then
                latefix=$i
            fi
        done

        fixlink=${fixlinks[$latefix]}
        fixdate=${fixdates[$latefix]}
        if [[ $fixlink == "" ]]; then
            echo "Bad Fixing Link" >> $debugfile
            bad=$(($bad + 1))
            badbugs+=($l)
            continue
        fi
        echo "$fixdate" >> $debugfile

        echo "$l,$name,$truefinddate,$fixlink,$fixdate,$repro,$config,$findlink,$finddate,$guiltylink,$guiltydate,$bit32" >> $outfile
        count=$(( $count + 1 ))
    fi

done

echo "=============================================================================================="
echo "$count bugs parsed successfully!"
echo "$noguilty bugs found without guilty commits."
echo "$bad bad parses."
