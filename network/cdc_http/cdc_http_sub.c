/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

enum {CDC_T_SRC = 0, CDC_T_DST};

#include "hiredis.h"
#include "cdc_redis_api.h"
#include "hotkey.h"
#include "mysql.h"
#include "mysqld_error.h"
static MYSQL  mysql0;
static MYSQL * mysql = &mysql0;
static redisContext *c;


int redis_connect(char *ip, int port) 
{
	c = NULL;
	c = redisConnect(ip, port);
    if (!c || c->err)
	{
		if (c)
			LOG(vfs_http_log, LOG_ERROR, "Connection error: %s\n", c->errstr);
		else
			LOG(vfs_http_log, LOG_ERROR, "Connection error: %s:%d:%m\n", ip, port);
		return -1;
	}
    return 0;
}

static int init_redis()
{
	char *ip = myconfig_get_value("redis_server");
	if (ip == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "config have not redis_server!\n");
		return -1;
	}
    return redis_connect(ip, myconfig_get_intval("redis_port", 16379));
}

static int add_new_task(int type, char rec[][256], int role, char *sip)
{
	list_head_t *hashlist = &taskhome; 
	t_cdc_http_task *task = NULL;
	list_head_t *l;
	int get = 0;
	list_for_each_entry_safe_l(task, l, hashlist, tlist)
	{
		list_del_init(&(task->tlist));
		get = 1;
		break;
	}
	if (get == 0)
	{
		LOG(vfs_http_log, LOG_NORMAL, "need malloc!\n");
		return 0;
		task = (t_cdc_http_task *)malloc(sizeof(t_cdc_http_task));
		if (task == NULL)
		{
			LOG(vfs_http_log, LOG_ERROR, "malloc err %m!\n");
			return -1;
		}
	}
	memset(task, 0, sizeof(t_cdc_http_task));
	INIT_LIST_HEAD(&(task->iplist));
	INIT_LIST_HEAD(&(task->tlist));

	uint32_t ip = str2ip(sip);;
	snprintf(task->file, sizeof(task->file), "%s:%s", rec[0], rec[1]);
	task->type = atoi(rec[3]);
	snprintf(task->task_status, sizeof(task->task_status), "%s", rec[4]);
	task->role = iprole[role];
	task->ip = ip;
	task->fsize = atol(rec[8]);
	task->mtime = atol(rec[7]);
	task->starttime = atol(rec[6]);
	if (type == CDC_T_SRC)
	{
		strcpy(task->srcip, sip);
		strcpy(task->dstip, rec[2]);
	}
	else
	{
		strcpy(task->srcip, rec[11]);
		strcpy(task->dstip, sip);
	}
	hashlist = &(iplist[ip&0xFF]); 
	list_add_head(&(task->iplist), hashlist);
	hashlist = &(tasklist[r5hash(task->file)&0xFF]);
	list_add_head(&(task->tlist), hashlist);
	LOG(vfs_http_log, LOG_NORMAL, "add new task ip %s %s %s\n", task->srcip, task->dstip, task->file); 
	return 0;
}

static int process_line(char *buf, int role, char *sip)
{
	char rec[20][256];
	memset(rec, 0, sizeof(rec));
	int n = sscanf(buf, "%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]", rec[0], rec[1], rec[2], rec[3], rec[4], rec[5], rec[6], rec[7], rec[8], rec[9], rec[10], rec[11], rec[12], rec[13], rec[14], rec[15]);
	if (n < 9)
	{
		LOG(vfs_http_log, LOG_ERROR, "err buf:[%s]\n", buf);
		return -1;
	}
	uint32_t ip = str2ip(sip);;
	int type = CDC_T_DST;
	uint32_t dstip = str2ip(rec[2]);
	if (dstip != ip)
		type = CDC_T_SRC;

	char file[256] = {0x0};
	snprintf(file, sizeof(file), "%s:%s", rec[0], rec[1]);
	list_head_t *hashlist = &(iplist[ip&0xFF]); 
	t_cdc_http_task *task = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(task, l, hashlist, iplist)
	{
		if (task->ip == ip && strcmp(file, task->file) == 0 && task->type == atoi(rec[3]) && strcmp(task->task_status, rec[4]) == 0)
		{
			if (type == CDC_T_SRC)
			{
				if (strcmp(task->srcip, sip) == 0 && strcmp(task->dstip, rec[2]) == 0)
					return 0;
			}
			else
			{
				if (strcmp(task->srcip, rec[11]) == 0 && strcmp(task->dstip, rec[2]) == 0)
					return 0;
			}
		}
	}
	add_new_task(type, rec, role, sip);
	return 0;
}

int do_bk_file(char *file, char *day)
{
	char bkdir[256] = {0x0};
	snprintf(bkdir, sizeof(bkdir), "%s/%s", cdc_path.bkdir, day);
	if (mkdir(bkdir, 0755) && errno != EEXIST)
	{
		LOG(vfs_http_log, LOG_ERROR, "mkdir %s err %m\n", bkdir);
		return -1;
	}

	char bkfile[256] = {0x0};
	snprintf(bkfile, sizeof(bkfile), "%s/%s", bkdir, basename(file));

	if(rename(file, bkfile))
	{
		LOG(vfs_http_log, LOG_ERROR, "rename %s to %s err %m\n", file, bkfile);
		return -1;
	}
	return 0;
}

static int do_out_file(char *file)
{
	char outfile[256] = {0x0};
	snprintf(outfile, sizeof(outfile), "%s/%s", cdc_path.outdir, basename(file));

	if(rename(file, outfile))
	{
		LOG(vfs_http_log, LOG_ERROR, "rename %s to %s err %m\n", file, outfile);
		return -1;
	}
	return 0;
}

static int get_ip_day_from_file (char *file, char *sip, char *day)
{
	char *t = strstr(file, "voss_stat_");
	if (t)
	{
		t += 10;
		snprintf(day, 16, "%.8s", t);
		return -1;
	}
	t = strstr(file, "voss_");
	if (t == NULL)
		return -1;
	t += 5;
	char *e = strchr(t, '_');
	if (e == NULL)
		return -1;
	*e = 0x0;
	strcpy(sip, t); 
	*e = '_';
	e++;
	t = strchr(e, '.');
	if (t == NULL)
		return -1;
	*t = 0x0;
	snprintf(day, 16, "%.8s", e);
	*t = '.';
	return 0;
}

int do_refresh_run_task()
{
	DIR *dp;
	struct dirent *dirp;
	char buff[2048];
	char fullfile[256];

	if ((dp = opendir(cdc_path.indir)) == NULL) 
	{
		LOG(vfs_http_log, LOG_ERROR, "opendir %s error %m!\n", cdc_path.indir);
		return -1;
	}

	FILE *fpin = NULL;
	while((dirp = readdir(dp)) != NULL) {
		if (dirp->d_name[0] == '.')
			continue;

		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", cdc_path.indir, dirp->d_name);
		int role = UNKOWN;
		if (dirp->d_name[0] == 'c')
			role = ROLE_CS;
		else if (dirp->d_name[0] == 'f')
			role = ROLE_FCS;
		else if (dirp->d_name[0] == 't')
			role = ROLE_TRACKER;
		
		char sip[16] = {0x0};
		char day[16] = {0x0};
		if (get_ip_day_from_file(dirp->d_name, sip, day))
		{
			LOG(vfs_http_log, LOG_ERROR, "file name %s err\n", fullfile);
			role = UNKOWN;
		}

		if (role != UNKOWN)
		{
			LOG(vfs_http_log, LOG_NORMAL, "process %s\n", fullfile);
			fpin = fopen(fullfile, "r");
			if (fpin == NULL) 
			{
				LOG(vfs_http_log, LOG_ERROR, "openfile %s error %m!\n", fullfile);
				continue;
			}
			memset(buff, 0, sizeof(buff));
			while (fgets(buff, sizeof(buff), fpin))
			{
				LOG(vfs_http_log, LOG_TRACE, "process line:[%s]\n", buff);
				process_line(buff, role, sip);
				memset(buff, 0, sizeof(buff));
			}
			fclose(fpin);
		}
		do_out_file(fullfile);
	}
	closedir(dp);
	return 0;
}

void refresh_run_task()
{
	list_head_t *hashlist; 
	t_cdc_http_task *task = NULL;
	list_head_t *l;
	int i = 0;
	for (; i < 256; i++)
	{
		hashlist = &(tasklist[i]);
		list_for_each_entry_safe_l(task, l, hashlist, tlist)
		{
			list_del_init(&(task->tlist));
			list_del_init(&(task->iplist));
			list_add_head(&(task->tlist), &taskhome);
		}
	}
	do_refresh_run_task();
}

static int connect_db(t_db_info *db)
{
    mysql_init(mysql);
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "gbk");
    if (NULL == mysql_real_connect(mysql, db->host, db->username, db->passwd, db->db, db->port, NULL, 0))
	{
		LOG(vfs_http_log, LOG_ERROR, "mysql_real_connect err %m\n");
		return -1;
	}
	return 0;
}

static int init_db()
{
	t_db_info db;
	memset(&db, 0, sizeof(db));
	char *v = myconfig_get_value("db_host");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "no db_host\n");
		return -1;
	}
	snprintf(db.host, sizeof(db.host), "%s", v);

	v = myconfig_get_value("db_username");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "no db_username\n");
		return -1;
	}
	snprintf(db.username, sizeof(db.username), "%s", v);

	v = myconfig_get_value("db_password");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "no db_password\n");
		return -1;
	}
	snprintf(db.passwd, sizeof(db.passwd), "%s", v);

	v = myconfig_get_value("db_db");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "no db_db\n");
		return -1;
	}
	snprintf(db.db, sizeof(db.db), "%s", v);

	db.port = myconfig_get_intval("db_port", 3306);
	return connect_db(&db) || init_redis();
}

static void add_trust_ip(uint32_t ip)
{
	uint32_t index = ip & IPMODE;
	int i = 0;
	while (i < 1024)
	{
		if (trustip[index][i] == 0)
		{
			trustip[index][i] = ip;
			break;
		}
		i++;
	}
	if (i >= 1024)
		LOG(vfs_http_log, LOG_ERROR, "index %u is full ip %u\n", index, ip);
}

static int reload_trust_ip()
{
	int ret = pthread_mutex_lock(&reload_mutex);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(vfs_http_log, LOG_ERROR, "pthread_mutex_lock err %m\n");
			return -1;
		}
	}
	char stime[16] = {0x0};
	get_strtime_by_t(stime, time(NULL) - 1800);
	char sql[512] = {0x0};
	snprintf(sql, sizeof(sql), "select ip from t_ip_status where ctime > '%s'", stime);

	if (mysql_query(mysql, sql))
	{
		LOG(vfs_http_log, LOG_ERROR, "mysql_query error:%s:[%s]", mysql_error(mysql), sql);
		pthread_mutex_unlock(&reload_mutex);
		return -1;
	}
	memset(trustip, 0, sizeof(trustip));

	MYSQL_ROW row = NULL;
	MYSQL_RES* result = mysql_store_result(mysql);
	if (result)
	{
		while(NULL != (row = mysql_fetch_row(result)))
		{
			if (row[0])
			{
				add_trust_ip(str2ip(row[0]));
			}
		}
		mysql_free_result(result);
	}

	char *s = myconfig_get_value("cdc_trust_ip");
	if (s == NULL)
		LOG(vfs_http_log, LOG_ERROR, "no cdc_trust_ip!\n");
	else
	{
		LOG(vfs_http_log, LOG_NORMAL, "cdc_trust_ip = %s!\n", s);
		char *e = NULL;
		while (1)
		{
			e = strchr(s, ',');
			if (e == NULL)
				break;
			*e = 0x0;
			add_trust_ip(str2ip(s));
			*e = ',';
			e++;
			s = e;
		}
		add_trust_ip(str2ip(s));
	}
	pthread_mutex_unlock(&reload_mutex);
	return 0;
}

int check_hotip_task(char *ip, char *f, char *d)
{
	return check_ip_redis(d, f, str2ip(ip), c);
}

int fileinfo_p_sub(char *url, char *buf, int curlen, int maxlen){
    char *t = strstr(url, "&fileinfo=");
    if (!t){
        return curlen;
    }
    char *d = t + strlen("&fileinfo=");
    t = strchr(d, ':');
    if (!t){
        return curlen;
    }
    *t = 0x0;
    char domain[64] = {0x0};
    snprintf(domain, sizeof(domain), "%s", d);
    *t = ':';
    char filepath[256] = {0x0};
    snprintf(filepath, sizeof(filepath), "%s", t+1);

    char stime[16] = {0x0};
    get_strtime(stime);

    uint32_t ips[1024] = {0x0};
    get_redis(domain, filepath, ips, 1024, c);
    int i = 0;
    uint32_t *ip = ips;
    while(i < 1024){
        if (*ip == 0)
            return curlen; 
        char sip[16] = {0x0};
        ip2str(sip, *ip);
        if (curlen >= maxlen)
            return curlen;
        curlen += snprintf(buf+curlen, maxlen-curlen, "%s %s CDC_F_OK#", sip, stime);
        ip++;
        i++;
    }
    return curlen;
}

static int check_ip_isp(uint32_t ip, t_cs_dir_info *cs)
{
	int i = 0;
	while (i < cs->index)
	{
		if (ip == cs->ip[i])
			return -1;
		i++;
	}
	return 0;
}

