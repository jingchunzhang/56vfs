#!/bin/sh

if [ $# -ne 2 ]
then
	echo "need infile outdir !";
	exit;
fi

infile=$1;
outdir=$2;

while [ 1 ]
do
	times=`date +%Y%m%d%H%M%S`
	outfile=$outdir/$times.mp4
	ln $infile $outfile
	sleep 60
done
