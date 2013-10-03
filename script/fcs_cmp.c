#include "common.h"
#include "util.h"
#include "mysql.h"
#include "mysqld_error.h"
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

static uint32_t get_id ()
{
	char table[64] = {0x0};
	char stime[16] = {0x0};
	get_strtime(stime);
	snprintf(table, sizeof(table), "t_alarm_msg_%.6s", stime);
	char sql[4096] = {0x0};
	snprintf(sql, sizeof(sql), "select id from %s where rule_id = 134223826", table);
    if (mysql_query(mysql, sql))
	{
		fprintf(stderr, "mysql_query %s %s\n", sql, mysql_error(mysql));
		return 0;
	}

	uint32_t ret = 0;
	
	MYSQL_ROW row = NULL;
    MYSQL_RES* result = mysql_store_result(mysql);
    if (result)
    {
        while(NULL != (row = mysql_fetch_row(result)))
        {
			ret = atoi(row[0]);
			break;
		}
        mysql_free_result(result);
    }
	return ret;
}

static void alarm_msg(char *buf)
{
	char table[64] = {0x0};
	char stime[16] = {0x0};
	get_strtime(stime);
	snprintf(table, sizeof(table), "t_alarm_msg_%.6s", stime);
	char sql[4096] = {0x0};

	uint32_t id = get_id();

	char msg[4096] = {0x0};
	snprintf(msg, sizeof(msg), "[fcs_list],[fcs_list],[VFS],[%s]", buf);
	if (id == 0)
	{
		if (strlen(buf) == 0)
			return;
		snprintf(sql, sizeof(sql), "insert into %s values(null, 134223826, 'jingchun.zhang@renren-inc.com', '%s', '', '', 0, 'fcs_list', '%s', '', '', 0, '%.6s', 1, 0, 300, 0)", table, stime, msg, stime);
		if (mysql_query(mysql, sql))
			fprintf(stderr, "mysql_query %s %s\n", sql, mysql_error(mysql));
		return;
	}

	if (strlen(buf) == 0)
	{
		snprintf(sql, sizeof(sql), "update %s set flag = 4 where id = %u", table, id);
		if (mysql_query(mysql, sql))
			fprintf(stderr, "mysql_query %s %s\n", sql, mysql_error(mysql));
		return;
	}

	snprintf(sql, sizeof(sql), "update %s set alarmmsg = '%s', lasttime = '%s' where id = %u", table, msg, stime, id);
	if (mysql_query(mysql, sql))
		fprintf(stderr, "mysql_query %s %s\n", sql, mysql_error(mysql));
}

static int connect_db(t_db_info *db)
{
	mysql_init(mysql);
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "gbk");
	if (NULL == mysql_real_connect(mysql, db->host, db->username, db->passwd, db->db, db->port, NULL, 0))
	{
		return -1;
	}
	return 0;
}

int init_db()
{
	t_db_info db;
	memset(&db, 0, sizeof(db));
	snprintf(db.host, sizeof(db.host), "monitor_db.corp.56.com");
	snprintf(db.username, sizeof(db.username), "php_moni");
	snprintf(db.passwd, sizeof(db.passwd), "InXzme5lCi0rU6VW");
	snprintf(db.db, sizeof(db.db), "monitor");
	db.port = 49710;
	return connect_db(&db);
}

void close_db()
{
    mysql_close(mysql);
}

static int fcs_cmp()
{
	char cdn_fcs[256] = {0x0};
	memset(cdn_fcs, 0, sizeof(cdn_fcs));

	char buf[4096] = {0x0};
	char *cdnfile = "/home/syncfile/fcs_list.txt";
	FILE *fp = fopen(cdnfile, "r");
	if (fp == NULL)
	{
		snprintf(buf, sizeof(buf), "open /home/syncfile/fcs_list.txt err %m");
		alarm_msg(buf);
		return -1;
	}
	memset(buf, 0, sizeof(buf));
	while(fgets(buf, sizeof(buf), fp))
	{
		if (strncmp(buf, "fcs", 3))
			continue;
		cdn_fcs[atoi(buf+3)&0xFF] = 1;
		memset(buf, 0, sizeof(buf));
	}
	fclose(fp);

	char *like = "fcs%.56.com";
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "select domainname from t_server_base_info where domainname like '%s' and groupid = 1 and status = 1 group by domainname order by  domainname;", like);
    if (mysql_query(mysql, sql))
	{
		snprintf(buf, sizeof(buf), "mysql_query err %s", mysql_error(mysql));
		alarm_msg(buf);
		return 2;
	}

	MYSQL_ROW row = NULL;
    MYSQL_RES* result = mysql_store_result(mysql);
    if (result)
    {
        while(NULL != (row = mysql_fetch_row(result)))
        {
			if (row[0] == NULL)
				continue;
			int fcs = atoi(row[0] + 3);
			if (cdn_fcs[fcs])
				cdn_fcs[fcs] = 0;
			else
				cdn_fcs[fcs] = -1;
		}
        mysql_free_result(result);
    }

	char afcs[4096] = {0x0};
	char rfcs[4096] = {0x0};
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "fcs%d.56.com;", i);
		if (cdn_fcs[i] == 1)
			strcat(rfcs, buf);
		else if (cdn_fcs[i] == -1)
			strcat(afcs, buf);
	}

	memset(buf, 0, sizeof(buf));
	if (strlen(afcs))
	{
		strcat(buf, "lack of: ");
		strcat(buf, afcs);
	}
	if (strlen(rfcs))
	{
		strcat(buf, "unknown fcs: ");
		strcat(buf, rfcs);
	}

	if (strlen(buf))
		alarm_msg(buf);
	else
		alarm_msg("");

	return 0;
}

void EnterDaemonMode(void)
{
	switch(fork())
	{
		case 0:

			break;

		case -1:

			fprintf(stderr, "fork error %m\n");
			exit(-1);
			break;

		default:

			exit(0);
			break;
	}

	setsid();
}

int main(int argc, char **argv)
{
	if (argc > 1)
	{
		if (strcasecmp(argv[1], "-v") == 0)
		{
			fprintf(stdout, "compile time [%s %s]\n", __DATE__, __TIME__);
			return -1;
		}
		return -1;
	}
	EnterDaemonMode();

	if (init_db())
	{
		fprintf(stderr, "init_db error [%s]\n", strerror(errno));
		return -1;
	}

	while (1)
	{
		fcs_cmp();
		sleep(300);
		mysql_ping(mysql);
	}
	return 0;
}

