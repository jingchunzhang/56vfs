#!/bin/sh

if [ $# -ne 1 ]
then
	echo "Usage `basename $0` touchfile"
	exit;
fi

logfile=./sync_old.log

i=0
cat $1 |while read file 
do
	echo $file |sh
	i=` expr $i + 1 `
	if [ $i -ge 5 ]
	then
		echo 5 >> $logfile
		sleep 2
		i=0;
	fi
done

