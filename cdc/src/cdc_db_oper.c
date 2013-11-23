#include "cdc_db_oper.h"
#include "myconfig.h"
#include "common.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>

static MYSQL  mysql0;
static MYSQL * mysql = &mysql0;
static const char *sql_prefix = "CREATE TABLE if not exists ";
//static const char *sql_suffix[] = {"( ip varchar(16) not null, domain varchar(16) not null, fname varchar(256) not null, task_type varchar(2)not null, task_stime varchar(16) not null, fmd5 varchar(34) , fsize bigint , over_status varchar(16), task_ctime varchar(16) , role varchar(2), process_time varchar(16), PRIMARY KEY (ip, domain, fname, task_type, task_stime) ) ENGINE=innodb DEFAULT CHARSET=latin1;", "(ip varchar(16) not null, day varchar(10), total int, success int, fail int, process_time varchar(16), PRIMARY KEY (ip, day)) ENGINE=innodb DEFAULT CHARSET=latin1;", "(day varchar(10), total int, success int, fail int, process_time varchar(16), PRIMARY KEY (ip, day)) ENGINE=innodb DEFAULT CHARSET=latin1;", "(ip varchar(16) not null, ctime varchar(16), report_status varchar(16), run_status varchar(2), sctime varchar(16), PRIMARY KEY (ip)) ENGINE=innodb DEFAULT CHARSET=latin1;"};
static const char *sql_suffix[] = {"( id bigint(20) NOT NULL AUTO_INCREMENT, ip varchar(16) not null, domain varchar(16) not null, fname varchar(256) not null, task_type varchar(2)not null, task_stime varchar(16) not null, fmd5 varchar(34) , fsize bigint , over_status varchar(16), task_ctime varchar(16) , role varchar(2), process_time varchar(16), PRIMARY KEY (id) ) ENGINE=innodb AUTO_INCREMENT=1280000 DEFAULT CHARSET=latin1;", "(ip varchar(16) not null, day varchar(10), total int, success int, fail int, process_time varchar(16), PRIMARY KEY (ip, day)) ENGINE=innodb DEFAULT CHARSET=latin1;", "(day varchar(10), total int, success int, fail int, process_time varchar(16), PRIMARY KEY (ip, day)) ENGINE=innodb DEFAULT CHARSET=latin1;", "(ip varchar(16) not null, ctime varchar(16), report_status varchar(16), run_status varchar(2), sctime varchar(16), PRIMARY KEY (ip)) ENGINE=innodb DEFAULT CHARSET=latin1;"};
extern int cdc_db_log ;

static int connect_db(t_db_info *db)
{
    mysql_init(mysql);
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "gbk");
    if (NULL == mysql_real_connect(mysql, db->host, db->username, db->passwd, db->db, db->port, NULL, 0))
	{
		LOG(cdc_db_log, LOG_ERROR, "mysql_real_connect err %m\n");
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
		LOG(cdc_db_log, LOG_ERROR, "no db_host\n");
		return -1;
	}
	snprintf(db.host, sizeof(db.host), "%s", v);

	v = myconfig_get_value("db_username");
	if (v == NULL)
	{
		LOG(cdc_db_log, LOG_ERROR, "no db_username\n");
		return -1;
	}
	snprintf(db.username, sizeof(db.username), "%s", v);

	v = myconfig_get_value("db_password");
	if (v == NULL)
	{
		LOG(cdc_db_log, LOG_ERROR, "no db_password\n");
		return -1;
	}
	snprintf(db.passwd, sizeof(db.passwd), "%s", v);

	v = myconfig_get_value("db_db");
	if (v == NULL)
	{
		LOG(cdc_db_log, LOG_ERROR, "no db_db\n");
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
		LOG(cdc_db_log, LOG_ERROR, "mysql_query error:%s:%d[%s]\n", mysql_error(mysql), mysql_errno(mysql), sql);
		mysql_ping(mysql);
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
		LOG(cdc_db_log, LOG_ERROR, "mysql_query error:%s:%d[%s]\n", mysql_error(mysql), mysql_errno(mysql), sql);
		mysql_ping(mysql);
		return -1;
	}
	return 0;
}

static inline void get_voss_tablename(char *ip, char *stime, char *fname, char *tablename, int len, int role)
{
	snprintf(tablename, len, "t_ip_task_info_%.8s", stime);
	return;
	char curtime[16] = {0x0};
	get_strtime(curtime);
	if(!strncmp(curtime, stime, 8))
	{
		char buf[256] = {0x0};
		snprintf(buf, sizeof(buf), "%s:%s", ip, fname);
		uint32_t index = r5hash(buf) & 0x3F;
		snprintf(tablename, len, "t_ip_task_info_%.8s_%02d", curtime, index);
	}
	else
		snprintf(tablename, len, "t_ip_task_info_%.8s", stime);
}

static int create_table(char *table, int index)
{
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "%s%s%s", sql_prefix, table, sql_suffix[index]);
	if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		mysql_ping(mysql);
		return -1;
	}
	return 0;
}

void process_db_stat(char rec[][256])
{
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "select count(*) from t_ip_status where ip ='%s';", rec[1]);
	int ret = get_sel_count(sql);
	memset(sql, 0, sizeof(sql));
	if (ret > 0)
		snprintf(sql, sizeof(sql), "update t_ip_status set ctime = '%s' , report_status = '%s' where ip ='%s';", rec[0], rec[2], rec[1]);
	else
		snprintf(sql, sizeof(sql), "replace into t_ip_status values ('%s', '%s', '%s', '', '');", rec[1], rec[0], rec[2]);
    if (mysql_query(mysql, sql))
		LOG(cdc_db_log, LOG_ERROR, "mysql_query error:%s:%d[%s]\n", mysql_error(mysql), mysql_errno(mysql), sql);
}

int get_last_status(t_voss_key *k, t_voss_val *v, char *table)
{
	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "select task_ctime, over_status from %s where ip = '%s' and domain = '%s' and fname = '%s' and task_type = '%s' and task_stime = '%s' ", table, k->ip, k->domain, k->fname, k->task_type, k->task_stime);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		mysql_ping(mysql);
		return -1;
	}

	MYSQL_ROW row = NULL;
    MYSQL_RES* result = mysql_store_result(mysql);
    if (result)
    {
        while(NULL != (row = mysql_fetch_row(result)))
        {
			if (row[0])
				snprintf(v->task_ctime, sizeof(v->task_ctime), "%s", row[0]);
			if (row[1])
				snprintf(v->over_status, sizeof(v->over_status), "%s", row[1]);
			break;
		}
        mysql_free_result(result);
    }
	return 0;
}

int mydb_update_voss(t_voss_key *k, t_voss_val *v, char *table, int type)
{
	char sql[1024] = {0x0};
	char stime[16] = {0x0};
	get_strtime(stime);
	char *prop = "ip, domain, fname, task_type, task_stime, fmd5, fsize, over_status, task_ctime, role, process_time";
	snprintf(sql, sizeof(sql), "insert into %s (%s) values ('%s', '%s', '%s', '%s', '%s', '%s', %lu, '%s', '%s', '%s', '%s')", table, prop, k->ip, k->domain, k->fname, k->task_type, k->task_stime, v->fmd5, v->fsize, v->over_status, v->task_ctime, v->role, stime); 
	
	LOG(cdc_db_log, LOG_DEBUG, "REPLACE SQL[%s]\n", sql);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		if (type == 0 && mysql_errno(mysql) == 1146)
			return (create_table(table, 0) || mydb_update_voss(k, v, table, 1));
		mysql_ping(mysql);
		return -1;
	}
	return 0;
}

int mydb_get_voss(t_voss_key *k, t_voss_val *v)
{
	if (strlen(k->task_stime) < 8)
	{
		LOG(cdc_db_log, LOG_ERROR, "stime err %s\n", k->task_stime);
		return -1;
	}

	char table[64] = {0x0};
	get_voss_tablename(k->ip, k->task_stime, k->fname, table, sizeof(table), atoi(v->role));

	return mydb_update_voss(k, v, table, 0);
}

static int mydb_update_voss_s(t_voss_s_key *k, t_voss_s_val *v, char *table, int type)
{
	char sql[1024] = {0x0};
	char stime[16] = {0x0};
	get_strtime(stime);
	snprintf(sql, sizeof(sql), "replace into %s values ('%s', '%s', %d, %d, %d, '%s')", table, k->ip, k->day, v->total, v->success, v->fail, stime); 
	
	LOG(cdc_db_log, LOG_DEBUG, "REPLACE SQL[%s]\n", sql);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		if (type == 0 && mysql_errno(mysql) == 1146)
			return (create_table(table, 1) || mydb_update_voss_s(k, v, table, 1));
		mysql_ping(mysql);
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
		LOG(cdc_db_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		mysql_ping(mysql);
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
	char table[64] = {0x0};
	snprintf(table, sizeof(table), "t_ip_task_statistic_%.6s", stime);

	char sql[1024] = {0x0};
	snprintf(sql, sizeof(sql), "replace into t_task_statistic select day ,sum(total), sum(success), sum(fail), process_time from %s group by day;", table);
	LOG(cdc_db_log, LOG_DEBUG, "SQL[%s]\n", sql);
    if (mysql_query(mysql, sql))
	{
		LOG(cdc_db_log, LOG_ERROR, "mysql_query error:%s:[%s]\n", mysql_error(mysql), sql);
		mysql_ping(mysql);
		return -1;
	}
	return 0;
}

void merge_db(time_t last)
{
	return;
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
