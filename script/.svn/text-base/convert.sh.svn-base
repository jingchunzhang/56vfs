#!/bin/sh

if [ $# -ne 2 ]
then
	echo "Usage convert.sh  hotip hostfile";
	exit;
fi

cat $2 |awk -F\/ '{print $5":/home/webadm/htdocs/flvdownload/"$3"/"$4"/"$NF}'
