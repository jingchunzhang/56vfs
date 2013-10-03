#!/bin/sh

cd /home/jingchun.zhang/vfs/

files="hot_cs_cnc.txt new_cs_cnc.txt new_cs_edu.txt new_cs_tel_mp4.txt new_cs_yd.txt hot_cs_tel.txt new_cs_cnc_mp4.txt new_cs_tel.txt new_cs_tt.txt"


while [ 1 ]
do
	for i in $files
	do
		wget http://113.105.245.83:8090/vfs/csdir/$i
		mv $i csdir/
	done
	sleep 600;
done

