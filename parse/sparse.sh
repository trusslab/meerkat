#!/bin/bash

outfile=sVersions.csv
nextpage="https://github.com/google/syzkaller/commits/master"
year=2022
lastdate=""
stopyear=2017

if [ -f $outfile ]; then
	rm $outfile
fi

while [[ $year -gt $stopyear ]]
do
	snapshot=$(lynx -dump -dont_wrap_pre -width=300 $nextpage)
	if [[ $snapshot == "" ]]; then
		snapshot=$(lynx -dump -dont_wrap_pre -width=300 $nextpage)
	fi
	dates=$(echo "$snapshot" | grep "Commits on" | grep -o "[A-Z,a-z]* [0-9]*, [0-9]*$")
	commits=$(echo "$snapshot"| grep "^[ ]*1\. \[[0-9]*\]")
	numbers=$(echo "$commits" | grep -o "\. \[[0-9]*\]" | grep -o "[0-9]*")

	# Get the dates for this page
	datearr=()
	ndatearr=()
	readarray -t datearr <<< "$dates"
	for d in "${datearr[@]}"; do
		ndatearr+=($(date -d "$d" +"%Y-%m-%d"))
	done

	# break the numbers into an array and then get the hashes
	numarr=()
	readarray -t numarr <<< "$numbers"
	hasharr=()
	for n in "${numarr[@]}"; do
		hasharr+=("$(echo "$snapshot" | grep "^[ ]*$n\. https" | grep -o -m 1 "[0-9,a-f]*$")")
	done

	# output both the dates and the hashes
	i=0
	for d in "${ndatearr[@]}"; do
		if [[ $d != $lastdate ]]; then
			echo "$d,${hasharr[$i]}" >> $outfile
			lastdate="$d"
		fi
		i=$(( $i + 1 ))
	done

	npl=$(echo "$snapshot" | grep -o "\[[0-9]*\]Older$" | grep -o "[0-9]*")
	nextpage=$(echo "$snapshot" | grep "^[ ]*$npl\. https" | grep -o "https.*master$")
	echo "$nextpage"
	year=$(( 10#${lastdate:0:4} ))
done
