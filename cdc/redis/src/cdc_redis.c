#include "hiredis.h"
#include "cdc_redis_api.h"
#include "hotkey.h"
#include "common.h"
#include "log.h"
#include "myconfig.h"
#include "util.h"
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <time.h>
#include <libgen.h>

int fplog = -1;

static int process_line(char *buf, uint32_t ip, redisContext *c)
{
	char rec[16][256];
	memset(rec, 0, sizeof(rec));
	int n = sscanf(buf, "%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]:%[^:]", rec[0], rec[1], rec[2], rec[3], rec[4], rec[5], rec[6], rec[7], rec[8], rec[9], rec[10], rec[11], rec[12], rec[13], rec[14], rec[15]);
	if (n < 9)
	{
		LOG(fplog, LOG_ERROR, "err buf:[%s]\n", buf);
		return -1;
	}
	if (str2ip(rec[2]) != ip)
	{
		LOG(fplog, LOG_DEBUG, "not self [%s][%u]\n", buf, ip);
		return -1;
	}
	if (strcmp(rec[4], "TASK_CLEAN"))
		return 0;
	if (strcmp(rec[5], "OVER_OK"))
		return 0;
	if (atoi(rec[3]))
		return del_ip_redis(rec[0], rec[1], ip, c);
	else
		return set_into_redis(rec[0], rec[1], ip, c);
}

static int run(t_path_info *path, redisContext *c)
{
	DIR *dp;
	struct dirent *dirp;
	char buff[2048];
	char fullfile[256];
	char *indir = path->indir;

	if ((dp = opendir(indir)) == NULL) 
	{
		LOG(fplog, LOG_ERROR, "opendir %s error %m!\n", indir);
		return -1;
	}

	FILE *fpin = NULL;

	while((dirp = readdir(dp)) != NULL) {
		if (dirp->d_name[0] == '.')
			continue;

		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", indir, dirp->d_name);
		char *t = strstr(dirp->d_name, "hcs_voss_");
		if (!t)
		{
			unlink(fullfile);
			continue;
		}
		t += 9;
		char *e = strchr(t, '_');
		if (e == NULL)
		{
			LOG(fplog, LOG_ERROR, "filename err %s\n", dirp->d_name);
			continue;
		}
		*e = 0x0;
		char sip[16] = {0x0};
		snprintf(sip, sizeof(sip), "%s", t);
		*e = '_';

		LOG(fplog, LOG_NORMAL, "process %s\n", fullfile);
		fpin = fopen(fullfile, "r");
		if (fpin == NULL) 
		{
			LOG(fplog, LOG_ERROR, "openfile %s error %m!\n", fullfile);
			continue;
		}
		uint32_t ip = str2ip(sip);
		memset(buff, 0, sizeof(buff));
		while (fgets(buff, sizeof(buff), fpin))
		{
			LOG(fplog, LOG_DEBUG, "process line:[%s]\n", buff);
			process_line(buff, ip, c);
			memset(buff, 0, sizeof(buff));
		}
		fclose(fpin);
		unlink(fullfile);
	}
	closedir(dp);
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

int init_para(t_path_info * path)
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

	fplog = registerlog(logfile, loglevel, logsize, logtime, logcount);
	if (fplog < 0)
	{
		fprintf(stderr, "registerlog %s %m\n", logfile);
		return -1;
	}

	v = myconfig_get_value("path_indir");
	if (v == NULL)
	{
		LOG(fplog, LOG_ERROR, "config have not path_indir!\n");
		return -1;
	}
	snprintf(path->indir, sizeof(path->indir), "%s", v);
	uint32_t h1, h2, h3;
	get_3_hash("aa", &h1, &h2, &h3);
	return 0;
}

void disconnect(redisContext *c) {
    redisFree(c);
}

redisContext *redis_connect(char *ip, int port) 
{
    redisContext *c = NULL;
	c = redisConnect(ip, port);
    if (!c || c->err)
	{
		if (c)
			LOG(fplog, LOG_ERROR, "Connection error: %s\n", c->errstr);
		else
			LOG(fplog, LOG_ERROR, "Connection error: %s:%d:%m\n", ip, port);
		return NULL;
	}

    return c;
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

	if (myconfig_init(argc, argv))
	{
		fprintf(stderr, "myconfig_init error [%s]\n", strerror(errno));
		return -1;
	}

	EnterDaemonMode();
	t_path_info path;
	memset(&path, 0, sizeof(path));

	if (init_para(&path))
		return -1;

    /* Ignore broken pipe signal (for I/O error tests). */
    signal(SIGPIPE, SIG_IGN);

	char *ip = myconfig_get_value("redis_server");
	if (ip == NULL)
	{
		LOG(fplog, LOG_ERROR, "config have not redis_server!\n");
		return -1;
	}
    redisContext *c = redis_connect(ip, myconfig_get_intval("redis_port", 16379));
	if (c == NULL)
	{
		LOG(fplog, LOG_ERROR, "redis_connect %s:%d err %m\n", ip, myconfig_get_intval("redis_port", 16379));
		return -1;
	}

	while (1)
	{
		run(&path, c);
		sleep(2);
	}

    disconnect(c);
    return 0;
}
