#include "util.h"
#include "cdc_db_oper.h"
#include "myconfig.h"
#include "common.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "log.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <libgen.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <dirent.h>

static MYSQL  mysql0;
static MYSQL * mysql = &mysql0;

static int connect_db(t_db_info *db)
{
    mysql_init(mysql);
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "gbk");
    if (NULL == mysql_real_connect(mysql, db->host, db->username, db->passwd, db->db, db->port, NULL, 0))
	{
		return -1;
	}

	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "set SESSION wait_timeout=86400");
	if (mysql_query(mysql, sql))
	{
		return -1;
	}
	return 0;
}

int init_db()
{
	t_db_info db;
	memset(&db, 0, sizeof(db));
	snprintf(db.host, sizeof(db.host), "10.26.80.213");
	snprintf(db.username, sizeof(db.username), "voss_ops");
	snprintf(db.passwd, sizeof(db.passwd), "U5ndWcaacmc2L8lp");
	snprintf(db.db, sizeof(db.db), "voss");

	db.port = 49710;
	return connect_db(&db);
}

void close_db()
{
    mysql_close(mysql);
}

void drop_table(time_t last)
{
	char day[16] = {0x0};
	get_strtime_by_t(day, last);
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "drop table t_ip_task_info_%.8s;", day);
    if (mysql_query(mysql, sql))
	{
		fprintf(stderr, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		return ;
	}
}

int main(int argc, char **argv)
{
	if(getppid() != 1)
		daemon(1, 1);
	if (init_db())
	{
		fprintf(stderr, "init_db error [%s]\n", strerror(errno));
		return -1;
	}


	int bak = 120;
	int max = 365;
	int min = bak;


	while (1)
	{
		time_t cur = time(NULL);
		time_t last = 0;
		while (min < max)
		{
			last = cur - 84600 * min;
			drop_table(last);
			min++;
		}
		min = bak;
		sleep (7200);
	}

	return 0;
}

