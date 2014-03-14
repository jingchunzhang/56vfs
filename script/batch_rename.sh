#!/bin/sh

if [ $# -ne 1 ]
then
	echo "Usage `basename $0` indir"
	exit;
fi

for file in $1/vfs*
do
	newfile=`echo $file|sed 's/vfs/yfs/g' `
	echo "git mv $file $newfile"
done
