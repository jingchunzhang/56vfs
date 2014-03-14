#!/bin/sh

if [ $# -ne 2 ]
then
	echo "need inputfile datadir !";
	exit;
fi

infile=$1;
outdir=$2;

getdir ()
{
	dir1=`expr $RANDOM % 30 `
	sleep 1;
	dir2=`expr $RANDOM % 30 `
	subdir=$outdir/$dir1/$dir2
	echo $subdir
}

while [ 1 ]
do
	count=`expr $RANDOM % 100 + 1 `
	times=`date +%Y%m%d%H%M%S`
	getdir|read curdir
	echo $curdir
	tmpfile=$curdir/.$times.flv.tmp
	outfile=$curdir/$times.flv
	dd if=$infile bs=1024 count=$count of=$tmpfile >>/dev/null 2>&1 
	mv $tmpfile $outfile
	sleep 5
done
