#!/bin/bash

outfile=managers-per-bug.csv
debugfile=debug2.txt

link=https://syzkaller.appspot.com/upstream/fixed

managers=( "ci-qemu-upstream" "ci-qemu-upstream-386" "ci-qemu2-arm32" "ci-qemu2-arm64" "ci-qemu2-arm64-compat" "ci-qemu2-arm64-mte" "ci-qemu2-riscv64" "ci-upstream-bpf-kasan-gce" "ci-upstream-bpf-next-kasan-gce" "ci-upstream-gce-arm64" "ci-upstream-gce-leak" "ci-upstream-kasan-badwrites-root" "ci-upstream-kasan-gce" "ci-upstream-kasan-gce-386" "ci-upstream-kasan-gce-root" "ci-upstream-kasan-gce-selinux-root" "ci-upstream-kasan-gce-smack-root" "ci-upstream-kmsan-gce" "ci-upstream-kmsan-gce-386" "ci-upstream-linux-next-kasan-gce-root" "ci-upstream-net-kasan-gce" "ci-upstream-net-this-kasan-gce" "ci2-upstream-fs" "ci2-upstream-kcsan-gce" "ci2-upstream-usb" "other" )
mcount=( 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )

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
alllinks=$(echo "$snapshot" | grep -o "https:\/\/syzkaller\.appspot\.com\/bug?[0-9a-z=]*$" | cat)
echo "Found $(echo "$alllinks" | wc -l) bugs to parse!"

buglinks=()
readarray -t buglinks <<< "$alllinks"
echo "Found ${#buglinks[@]} links!"

echo "Parsing the bugs..."
echo -n "Number" > $outfile
for m in ${managers[@]}; do
    echo -n ",$m" >> $outfile
done
echo "" >> $outfile

echo -n "" > $debugfile
count=1
for l in ${buglinks[@]}; do
    echo "==============================================================================================" >> $debugfile
    bsnapshot=$(lynx -dump -dont_wrap_pre -width=300 $l)

    # grab the bug name
    name=$(echo "$bsnapshot" | sed -n '7p' | sed 's/^[ ]*//' | cat)

    # get a list of the managers
    # managers -> mans -> men lol
    men=$(echo "$bsnapshot" | awk '/Crashes \(/,0' | grep "\[\([0-9]\+\)\]\.config[ ]*\[\([0-9]\+\)\]console\|strace log[ ]*\[\([0-9]\+\)\]report" | grep -o "ci[2]*-[a-z23468\-]*" | cat)
    mcount=( 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 )
    for bm in ${men[@]}; do
        i=0
        f=0
        for m in ${managers[@]}; do
            if [[ $bm == $m ]]; then
                mcount[$i]=$(( ${mcount[$i]} + 1 ))
                f=1
                break
            fi
            i=$(( $i + 1 ))
        done
        if (( $f == 0 )); then
            mcount[-1]=$(( ${mcount[-1]} + 1 ))
        fi
    done

    echo "number: $count" >> $debugfile
    echo $name >> $debugfile
    echo "$l" >> $debugfile

    echo -n "$count" >> $outfile
    i=0
    for n in ${mcount[@]}; do
        echo -n ",$n" >> $outfile
        echo "${managers[$i]}: $n" >> $debugfile
        i=$(( $i + 1 ))
    done
    echo "" >> $outfile

    count=$(( $count + 1 ))
done

echo "=============================================================================================="
echo "$count bugs parsed successfully!"
