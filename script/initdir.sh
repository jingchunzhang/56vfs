#!/bin/sh

if [ $# -ne 1 ]
then
	echo "need datadir"
	exit;
fi

DIR1=30
DIR2=30

storage_dir_init ()
{
	dir1=0
	dir2=0
	while [ $dir1 -lt $DIR1 ]
	do
		while [ $dir2 -lt $DIR2 ]
		do
			subdir=$1/$dir1/$dir2
			mkdir $subdir -p
			dir2=` expr $dir2 + 1 `
		done
		dir2=0
		dir1=` expr $dir1 + 1 `
	done
}

storage_dir_init $1
