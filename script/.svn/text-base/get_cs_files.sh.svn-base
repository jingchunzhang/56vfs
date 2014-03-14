#!/bin/sh

if [ $# -ne 4 ]
then
	echo "Usage `basename $0` ip_pre startday endday outfile "
	exit
fi

day=$2
while [ $day != $3 ]
do
	someday=`echo "date +%Y%m%d --date '$day days ago'" |sh`
	day=` expr $day + 1 `
	table=t_ip_task_info_$someday;
	sql="select domain, fname from $table where ip like '$1%'"
	echo $sql | mysql -h 10.26.80.213  -u voss_ops -pU5ndWcaacmc2L8lp voss >> $4
done

