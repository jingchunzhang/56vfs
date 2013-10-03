#!/bin/sh

if [ $# -ne 4 ]
then
	echo "Usage `basename $0` dir1 dir2_min dir_max outfile!";
	exit;
fi

i=$2

while [ $i -le $3 ]
do
	echo "$1/$i" >> $4
	i=` expr $i + 1 `
done



