#!/bin/sh

if [ $# -ne 2 ]
then
	echo "Usage `basename $0` license dir([hc])"
	exit;
fi

add_license()
{
	infile=$2
	tmpfile=$infile.tmp
	echo $1
#	paste -s -d "\n" $1 $infile > $tmpfile
#	mv $tmpfile $infile
}

p_dir()
{
	for infile in $2/*
	do
		if [ -d $infile ]
		then
			p_dir $1 $infile
			echo "prepare $infile"
			continue;
		fi
		s=`echo $infile|awk -F\. '{print $NF}' `
		if [ $s == 'h' ]
		then
			add_license $1 $infile
		fi
		if [ $s == 'c' ]
		then
			add_license $1 $infile
		fi
	done
}

p_dir $1 $2
