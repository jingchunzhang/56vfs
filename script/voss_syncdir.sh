#!/bin/sh

voss=172.16.245.45
voss2=172.16.245.125

target=`/sbin/ifconfig | /bin/grep $voss`
if [ -z "$target" ]
then
	target=$voss
else
	target=$voss2
fi

while true
do
	for file in `find /diska/vfs/path/syncdir -type f`
	do
		bn=`basename $file`
		/usr/bin/rsync --port=9860 --password-file=/home/dingqin.lv/voss_sync.passwd $file voss@$target::voss_sync 
		rm -f $file
	done
	sleep 5
done
