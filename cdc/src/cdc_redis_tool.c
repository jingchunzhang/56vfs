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
		fprintf(stderr, "mysql_real_connect error:%m");
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

static void output(char *ip, char *d, char *f, char *outdir)
{
	char outfile[256] = {0x0};
	snprintf(outfile, sizeof(outfile), "%s/hcs_voss_%s_20130417.master", outdir, ip);
	FILE *fp = fopen(outfile, "a+");
	if (fp == NULL)
	{
		fprintf(stderr, "fopen %s err %m\n", outfile);
		return ;
	}
	fprintf(fp, "%s:%s:%s:0:TASK_CLEAN:OVER_OK:0:TASK_CLEAN:OVER_OK:0:TASK_CLEAN:OVER_OK\n", d, f, ip);
	fclose(fp);
}

int mydb_redis(char *outdir)
{
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "select * from t_hotip_task_info where task_ctime < '2013040101'");

    if (mysql_query(mysql, sql))
	{
		fprintf(stderr, "mysql_query error:%s:[%s]", mysql_error(mysql), sql);
		return -1;
	}

	MYSQL_ROW row = NULL;
    MYSQL_RES* result = mysql_store_result(mysql);
    if (result)
    {
        while(NULL != (row = mysql_fetch_row(result)))
        {
			if (strcmp(row[7], "OVER_OK"))
				continue;
			output(row[0], row[1], row[2], outdir);
		}
        mysql_free_result(result);
    }
	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		fprintf(stdout, " tool outdir!\n");
		return -1;
	}
	if (init_db())
	{
		fprintf(stderr, "init_db err %m\n");
		return -1;
	}
	mydb_redis(argv[1]);
	return 0;
}

