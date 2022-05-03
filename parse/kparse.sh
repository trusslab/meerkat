#!/bin/bash

# these will change depending on the reporitory
repo=bpf/bpf.git
hashpref="bpf"

ofs=-200
inc=200
year=0
month=0
day=0
stopyear=2016
snapshot=""
outfile=$hashpref-kVersions.csv
maxsearch=5


if [ -f $outfile ]; then
	rm $outfile
fi

# find the first date
echo "Looking for the first date..."
while [[ $(echo "$snapshot" | grep -m 1 -o "^[ ]*20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]" | cat) == "" ]]
do
	ofs=$((ofs + inc))
	echo "ofs = $ofs..."
	snapshot=$(lynx -dump -dont_wrap_pre -width=300 https://git.kernel.org/pub/scm/linux/kernel/git/$repo/log/?ofs\=$ofs)
done

# grab the first date and turn it to ints
firstdate=$(echo "$snapshot" | grep -m 1 -o "^[ ]*20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]" | grep -m 1 -o "20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]")
year=$((10#$(date -d $firstdate +%Y)))
month=$((10#$(date -d $firstdate +%m)))
day=$((10#$(date -d $firstdate +%d)))

# ignore the first day because it hasn't finished yet
date=$(../helpers/decdate $year $month $day)
date="2022-02-26"
year=$((10#$(date -d $date +%Y)))
month=$((10#$(date -d $date +%m)))
day=$((10#$(date -d $date +%d)))
echo "First Date To Get: $date"

while [[ $year -gt $stopyear ]]
do
	# grab the first commit for each day
	input=$(echo "$snapshot" | grep -m 1 "^[ ]*$date")
	while [[ $input != "" ]]
	do
		# if the date is on this page, print out the line
		number=$(echo "$input" | grep -o "\[[0-9]*\]" | grep -o "[0-9]*")
		line=$(echo "$snapshot" | grep -m 1 "^[ ]*$number\. ")
		khash=$(echo "$line" | grep -o "id=[0-9a-f]*$" | grep -o "[0-9a-f]*$")
		echo "$date,$hashpref-$khash" >> $outfile

		# check for the next date
		date=$(../helpers/decdate $year $month $day)
		year=$((10#$(date -d $date +%Y)))
		month=$((10#$(date -d $date +%m)))
		day=$((10#$(date -d $date +%d)))

		input=$(echo "$snapshot" | grep -m 1 "^[ ]*$date")
		loopc=0
		while [[ $input == "" && $loopc -lt $maxsearch ]]
		do
			pdate=$(../helpers/decdate $year $month $day)
			year=$((10#$(date -d $pdate +%Y)))
			month=$((10#$(date -d $pdate +%m)))
			day=$((10#$(date -d $pdate +%d)))
			input=$(echo "$snapshot" | grep -m 1 "^[ ]*$pdate")
			loopc=$(( $loopc + 1 ))
		done

		if [[ $loopc -lt $maxsearch && $loopc -gt 0 ]]; then
			date=$pdate
		fi

		year=$((10#$(date -d $date +%Y)))
		month=$((10#$(date -d $date +%m)))
		day=$((10#$(date -d $date +%d)))
	done
	# when the current page is exhausted, get the next page
	ofs=$((ofs + inc))
	echo "ofs = $ofs. Looking for $date"
    snapshot=$(lynx -dump -dont_wrap_pre -width=300 https://git.kernel.org/pub/scm/linux/kernel/git/$repo/log/?ofs\=$ofs)
done
