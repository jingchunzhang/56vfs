#!/bin/sh

if [ $# -ne 2 ]
then
	echo "need inputfile outdir !";
	exit;
fi

infile=$1;
outdir=$2;

dir2=0
while [ 1 ]
do
	count=`expr $RANDOM % 1000000 + 20000 `
	times=`date +%Y%m%d%H%M%S`
	tmpfile=$outdir/$dir2/.$times.flv.tmp
	outfile=$outdir/$dir2/$times.flv
	dd if=$infile bs=1024 count=$count of=$tmpfile >>/dev/null 2>&1 
	mv $tmpfile $outfile
	dir2=` expr $dir2 + 1 `
	if [ $dir2 -ge 10 ]
	then
		dir2=0
	fi
	sleep 60;
done
