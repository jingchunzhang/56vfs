#!/bin/sh

if [ $# -ne 4 ]
then
	echo "Usage `basename $0` dir1 dir2_min dir_max outfile!";
	exit;
fi

i=$2
CUR=`date '+%Y%m%d%H%M%S'`

while [ $i -le $3 ]
do
	echo "iplist=59.32.213.133&vfs_cmd=M_SETDIRTIME&$1,$i=$CUR" >> $4
	i=` expr $i + 1 `
done



