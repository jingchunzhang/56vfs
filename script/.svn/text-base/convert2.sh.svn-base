#!/bin/sh

if [ $# -ne 2 ]
then
	echo "Usage convert.sh  hotip hostfile";
	exit;
fi

cat $1 |while read ip
do
	IP=$ip
	cat $2 | awk -v sip=$IP '{print sip" "$0}'
done
