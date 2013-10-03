#!/bin/sh

DOCROOT=/home/webadm/htdocs/flvdownload
RUNLOG=/diska/check_fcs.log
ERRLOG=/diska/check_fcs.errlog
D1=30
D2=30

ONCE_MAX=5

d1=0
d2=0

while [ $d1 -lt $D1 ]
do
	while [ $d2 -lt $D2 ]
	do
		curroot=$DOCROOT/$d1/$d2/
		once=0
		for file in $curroot/*.flv
		do
			mp4file=$file.mp4
			if [ -e $mp4file ]
			then
				mc=`stat $file $mp4file |grep Modify |sort -u |wc -l` 
				if [ $mc -eq 1 ]
				then
					echo "same $file -r $mp4file" >> $RUNLOG
					continue;
				fi
				echo "touch $file -r $mp4file" >> $RUNLOG
				touch $file -r $mp4file
				once=` expr $once + 1 `
			else
				echo "$mp4file not exist" >> $ERRLOG
			fi
			if [ $once -ge $ONCE_MAX ]
			then
				sleep 2
				once=0
			fi
		done
		d2=` expr $d2 + 1 `
	done
	d2=0
	d1=` expr $d1 + 1 `
done
