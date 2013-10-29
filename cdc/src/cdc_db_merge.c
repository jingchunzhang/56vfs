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

int cdc_db_tool_log = -1;
static MYSQL  mysql0;
static MYSQL * mysql = &mysql0;
static const char *sql_prefix = "CREATE TABLE if not exists ";
static const char *sql_suffix[] = {"(id BIGINT NOT NULL AUTO_INCREMENT, ip varchar(16) not null, domain varchar(16) not null, fname varchar(256) not null, task_type varchar(2)not null, task_stime varchar(16) not null, fmd5 varchar(34) , fsize bigint , over_status varchar(16), task_ctime varchar(16) , role varchar(2), process_time varchar(16), PRIMARY KEY (id)) ENGINE=innodb DEFAULT CHARSET=latin1;", "(ip varchar(16) not null, day varchar(10), total int, success int, fail int, process_time varchar(16), PRIMARY KEY (ip, day)) ENGINE=innodb DEFAULT CHARSET=latin1;", "(day varchar(10), total int, success int, fail int, process_time varchar(16), PRIMARY KEY (ip, day)) ENGINE=innodb DEFAULT CHARSET=latin1;", "(ip varchar(16) not null, ctime varchar(16), report_status varchar(16), run_status varchar(2), sctime varchar(16), PRIMARY KEY (ip)) ENGINE=innodb DEFAULT CHARSET=latin1;"};

static int connect_db(t_db_info *db)
{
    mysql_init(mysql);
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "gbk");
    if (NULL == mysql_real_connect(mysql, db->host, db->username, db->passwd, db->db, db->port, NULL, 0))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_real_connect err %m\n");
		return -1;
	}

	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "set SESSION wait_timeout=86400");
	if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		return -1;
	}
	return 0;
}

int init_db()
{
	t_db_info db;
	memset(&db, 0, sizeof(db));
	char *v = myconfig_get_value("db_host");
	if (v == NULL)
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "no db_host\n");
		return -1;
	}
	snprintf(db.host, sizeof(db.host), "%s", v);

	v = myconfig_get_value("db_username");
	if (v == NULL)
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "no db_username\n");
		return -1;
	}
	snprintf(db.username, sizeof(db.username), "%s", v);

	v = myconfig_get_value("db_password");
	if (v == NULL)
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "no db_password\n");
		return -1;
	}
	snprintf(db.passwd, sizeof(db.passwd), "%s", v);

	v = myconfig_get_value("db_db");
	if (v == NULL)
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "no db_db\n");
		return -1;
	}
	snprintf(db.db, sizeof(db.db), "%s", v);

	db.port = myconfig_get_intval("db_port", 3306);
	return connect_db(&db);
}

void close_db()
{
    mysql_close(mysql);
}

int get_sel_count(char *sql)
{
	int ret = -1;
	if (mysql_query(mysql, sql))
		return ret;
	MYSQL_RES* result = mysql_store_result(mysql);
	if (result) 
	{
		MYSQL_ROW row = NULL;
		if (NULL != (row = mysql_fetch_row(result)))
		{
			ret = atoi(row[0]);
		}

		mysql_free_result(result);
	}
    
    return ret;
}

int mydb_begin()
{
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "begin;");
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:%d[%s]\n", mysql_error(mysql), mysql_errno(mysql), sql);
		return -1;
	}
	return 0;
}

int mydb_commit()
{
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "commit;");
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:%d[%s]\n", mysql_error(mysql), mysql_errno(mysql), sql);
		return -1;
	}
	return 0;
}

static inline void get_voss_tablename(char *ip, char *stime, char *tablename, int len)
{
	uint32_t index = r5hash(ip) & 0x3F;
	snprintf(tablename, len, "t_ip_task_info_%.8s_%02d", stime, index);
}

static int create_table(char *table, int index)
{
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "%s%s%s", sql_prefix, table, sql_suffix[index]);
	if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		return -1;
	}
	return 0;
}

void process_db_stat(char rec[][256])
{
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "replace into t_ip_status values ('%s', '%s', '%s', '', '');", rec[1], rec[0], rec[2]);
    if (mysql_query(mysql, sql))
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:%d[%s]\n", mysql_error(mysql), mysql_errno(mysql), sql);
}

int mydb_update_voss(t_voss_key *k, t_voss_val *v, char *table, int type)
{
	char sql[1024] = {0x0};
	char stime[16] = {0x0};
	get_strtime(stime);
	snprintf(sql, sizeof(sql), "replace into %s values ('%s', '%s', '%s', '%s', '%s', '%s', %lu, '%s', '%s', '%s', '%s')", table, k->ip, k->domain, k->fname, k->task_type, k->task_stime, v->fmd5, v->fsize, v->over_status, v->task_ctime, v->role, stime); 
	
	LOG(cdc_db_tool_log, LOG_DEBUG, "REPLACE SQL[%s]\n", sql);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		if (type == 0 && mysql_errno(mysql) == 1146)
			return (create_table(table, 0) || mydb_update_voss(k, v, table, 1));
		return -1;
	}
	return 0;
}

int mydb_get_voss(t_voss_key *k, t_voss_val *v)
{
	if (strlen(k->task_stime) < 8)
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "stime err %s\n", k->task_stime);
		return -1;
	}

	char table[64] = {0x0};
	get_voss_tablename(k->ip, k->task_stime, table, sizeof(table));

	return mydb_update_voss(k, v, table, 0);
}

static int mydb_update_voss_s(t_voss_s_key *k, t_voss_s_val *v, char *table, int type)
{
	char sql[1024] = {0x0};
	char stime[16] = {0x0};
	get_strtime(stime);
	snprintf(sql, sizeof(sql), "replace into %s values ('%s', '%s', %d, %d, %d, '%s')", table, k->ip, k->day, v->total, v->success, v->fail, stime); 
	
	LOG(cdc_db_tool_log, LOG_DEBUG, "REPLACE SQL[%s]\n", sql);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		if (type == 0 && mysql_errno(mysql) == 1146)
			return (create_table(table, 1) || mydb_update_voss_s(k, v, table, 1));
		return -1;
	}
	return 0;
}

static int mydb_get_voss_s(t_voss_s_key *k, t_voss_s_val *v)
{
	char table[64] = {0x0};
	snprintf(table, sizeof(table), "t_ip_task_statistic_%.6s", k->day);
	return mydb_update_voss_s(k, v, table, 0);
}

void drop_table(char *table)
{
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "drop table %s;", table);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		return ;
	}
	LOG(cdc_db_tool_log, LOG_DEBUG, "SQL[%s] ok!\n", sql);
}

void merge_db_sub(char *t, char *stime)
{
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "select a.ip, a.ok_count, b.fail_count from  (select count(over_status) as ok_count,ip from %s where over_status = 'OVER_OK' group by ip) a left join (select count(over_status) as fail_count,ip from %s where over_status  != 'OVER_OK' group by ip) b on a.ip = b.ip;", t, t);

	t_voss_s_key k;
	t_voss_s_val v;
	memset(&k, 0, sizeof(k));
	memset(&v, 0, sizeof(v));

	snprintf(k.day, sizeof(k.day), "%.8s", stime);

    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		return ;
	}

	MYSQL_ROW row = NULL;
    MYSQL_RES* result = mysql_store_result(mysql);
    if (result)
    {
        while(NULL != (row = mysql_fetch_row(result)))
        {
			memset(k.ip, 0, sizeof(k.ip));
			snprintf(k.ip, sizeof(k.ip), "%s", row[0]);
			v.success = atoi(row[1]);
			v.fail = (row[2] != NULL ) ? atoi(row[2]) : 0;
			v.total = v.success + v.fail;
			mydb_get_voss_s(&k, &v);
		}
        mysql_free_result(result);
    }

	char outtable[64] = {0x0};
	snprintf(outtable,  sizeof(outtable), "t_ip_task_info_%.8s", stime);
	if(create_table(outtable, 0))
		return;

	char *prop = "ip, domain, fname, task_type, task_stime, fmd5, fsize, over_status, task_ctime, role, process_time";
	memset(sql, 0, sizeof(sql));
	snprintf(sql, sizeof(sql), "insert into %s (%s) select %s from %s ;", outtable, prop, prop, t);
	LOG(cdc_db_tool_log, LOG_DEBUG, "SQL[%s]\n", sql);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		return ;
	}
	char stime0[16] = {0x0};
	get_strtime(stime0);
	if (strncmp(stime0, stime, 8))
		drop_table(t);
}

static void statistic_someday(char *stime)
{
	char t[64] = {0x0};
	snprintf(t,  sizeof(t), "t_ip_task_info_%.8s", stime);
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "select a.ip, a.ok_count, b.fail_count from  (select count(over_status) as ok_count,ip from %s where over_status = 'OVER_OK' group by ip) a left join (select count(over_status) as fail_count,ip from %s where over_status  != 'OVER_OK' group by ip) b on a.ip = b.ip;", t, t);

	t_voss_s_key k;
	t_voss_s_val v;
	memset(&k, 0, sizeof(k));
	memset(&v, 0, sizeof(v));

	snprintf(k.day, sizeof(k.day), "%.8s", stime);

    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		return ;
	}

	MYSQL_ROW row = NULL;
    MYSQL_RES* result = mysql_store_result(mysql);
    if (result)
    {
        while(NULL != (row = mysql_fetch_row(result)))
        {
			memset(k.ip, 0, sizeof(k.ip));
			snprintf(k.ip, sizeof(k.ip), "%s", row[0]);
			v.success = atoi(row[1]);
			v.fail = (row[2] != NULL ) ? atoi(row[2]) : 0;
			v.total = v.success + v.fail;
			mydb_get_voss_s(&k, &v);
		}
        mysql_free_result(result);
    }
}

static int update_global(char *stime)
{
	statistic_someday(stime);
	char table[64] = {0x0};
	snprintf(table, sizeof(table), "t_ip_task_statistic_%.6s", stime);

	const char *nopre = "fcs%";
	char sql[1024] = {0x0};
	char *tracker = "211.151.181.21%";
	snprintf(sql, sizeof(sql), "replace into t_task_statistic select day ,sum(total), sum(success), sum(fail), process_time from %s where ip not like '%s' and ip not like '%s' group by day;", table, nopre, tracker);
	LOG(cdc_db_tool_log, LOG_DEBUG, "SQL[%s]\n", sql);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		return -1;
	}

	memset(sql, 0, sizeof(sql));
	snprintf(sql, sizeof(sql), "replace into %s select domain, '%.8s', count(1), 0, 0, max(process_time) from t_ip_task_info_%.8s where ip like '%s' group by domain;", table, stime, stime, nopre);
	LOG(cdc_db_tool_log, LOG_DEBUG, "SQL[%s]\n", sql);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		return -1;
	}

	char *domain_table = "t_domain_statistic";
	create_table(domain_table, 1);
	memset(sql, 0, sizeof(sql));
	snprintf(sql, sizeof(sql), "replace into %s select domain, '%.8s', count(1), 0, 0, max(process_time) from t_ip_task_info_%.8s where ip like '%s' group by domain;", domain_table, stime, stime, tracker);
	LOG(cdc_db_tool_log, LOG_DEBUG, "SQL[%s]\n", sql);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_tool_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		return -1;
	}
	return 0;
}

void merge_db(time_t last)
{
	char stime[16] = {0x0};
	get_strtime_by_t(stime, last);
	int i = 0;
	char tablename[64];
	for (i = 0; i < 64; i++)
	{
		memset(tablename, 0, sizeof(tablename));
		snprintf(tablename,  sizeof(tablename), "t_ip_task_info_%.8s_%02d", stime, i);
		merge_db_sub(tablename, stime);
	}

	update_global(stime);
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

int init_tool()
{
	char *v = myconfig_get_value("log_logname");
	if (v == NULL)
	{
		fprintf(stderr, "config have not logname!\n");
		return -1;
	}
	char *logfile = v;
	int loglevel = getloglevel(myconfig_get_value("log_loglevel"));
	int logsize = myconfig_get_intval("log_logsize", 100);
	int logtime = myconfig_get_intval("log_logtime", 3600);
	int logcount = myconfig_get_intval("log_logcount", 10);

	if (init_log())
	{
		fprintf(stderr, "init log error %m\n");
		return -1;
	}

	cdc_db_tool_log = registerlog(logfile, loglevel, logsize, logtime, logcount);
	if (cdc_db_tool_log < 0)
	{
		fprintf(stderr, "registerlog %s %m\n", logfile);
		return -1;
	}
	return init_db();
}

static void create_new_table()
{
	int i = 0;
	time_t cur = time(NULL);
	while (i < 4)
	{
		time_t future = cur + i * 86400;
		char stime[16] = {0x0};
		get_strtime_by_t(stime, future);
		char outtable[64] = {0x0};
		snprintf(outtable,  sizeof(outtable), "t_ip_task_info_%.8s", stime);
		create_table(outtable, 0);
		i++;
	}
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
	}
	EnterDaemonMode();
	if (myconfig_init(argc, argv))
	{
		fprintf(stderr, "myconfig_init error [%s]\n", strerror(errno));
		return -1;
	}

	if (init_tool())
	{
		fprintf(stderr, "init_tool error [%s]\n", strerror(errno));
		return -1;
	}

	time_t last = 0;

	int max = 4;
	int min = 1;


	while (1)
	{
		time_t cur = time(NULL);
		while (min < max)
		{
			last = cur - 84600 * min;
			merge_db(last);
			min++;
		}
		min = 1;
		sleep (30);
		create_new_table();
	}

	return 0;
}

