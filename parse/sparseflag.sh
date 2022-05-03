#!/bin/bash

outfile=sMutFlags.csv
nextmutpage="https://github.com/google/syzkaller/commits/master/prog/mutation.go"
nexttempage="https://github.com/google/syzkaller/commits/master/sys/linux"
year=2022
lastdate=""
stopyear=2016

if [ -f $outfile ]; then
	rm $outfile
fi

while [[ $year -gt $stopyear ]]
do
	snapshot=$(lynx -dump -dont_wrap_pre -width=300 $nextmutpage)
	if [[ $snapshot == "" ]]; then
		# sometimes you gotta retry
		snapshot=$(lynx -dump -dont_wrap_pre -width=300 $nextmutpage)
	fi
	dates=$(echo "$snapshot" | grep "Commits on" | grep -o "[A-Z,a-z]* [0-9]*, [0-9]*$")

	# Get the dates for this page
	datearr=()
	ndatearr=()
	readarray -t datearr <<< "$dates"
	for d in "${datearr[@]}"; do
		ndatearr+=($(./ntod "$d"))
	done

	# output both the dates and the hashes
	for d in "${ndatearr[@]}"; do
		if [[ $d != $lastdate ]]; then
			echo "$d,Mutation" >> $outfile
			lastdate="$d"
		fi
	done

	npl=$(echo "$snapshot" | grep -o "\[[0-9]*\]Older$" | grep -o "[0-9]*")
	nextmutpage=$(echo "$snapshot" | grep -m 1 "^[ ]*$npl\. https" | grep -o "https.*path\[\]=mutation.go$")
	echo "$nextpage"
	year=$(( 10#${lastdate:0:4} ))
done
