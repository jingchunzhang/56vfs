#!/bin/sh

if [ $# -ne 3 ]
then
	echo "Usage `basename $0` dir1 dir2 isp[tel|cnc]"
	exit;
fi

prefix=$1"/"$2" "

csfile="/home/syncfile/new_cs_"$3".txt"
hotfile="/home/syncfile/hot_cs_"$3".txt"

iplist=`grep ^$prefix  $csfile|awk '{print $2","}'`

echo $iplist


