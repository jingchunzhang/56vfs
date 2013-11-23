#include <stdio.h>
#include <stdint.h>
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
#include "util.h"
#include "log.h"
#include "myconfig.h"
#include "common.h"
#include "cdc_db_oper.h"

#define ID __FILE__
#define LN __LINE__

t_path_info cdc_path;
char *redisdir = NULL;
int cdc_db_log = -1;
int process_curday = 1;
#include "cdc_db_sub.c"

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

	cdc_db_log = registerlog(logfile, loglevel, logsize, logtime, logcount);
	if (cdc_db_log < 0)
	{
		fprintf(stderr, "registerlog %s %m\n", logfile);
		return -1;
	}

	v = myconfig_get_value("path_indir");
	if (v == NULL)
	{
		LOG(cdc_db_log, LOG_ERROR, "config have not path_indir!\n");
		return -1;
	}
	snprintf(path->indir, sizeof(path->indir), "%s", v);

	v = myconfig_get_value("path_bkdir");
	if (v == NULL)
	{
		LOG(cdc_db_log, LOG_ERROR, "config have not path_bkdir!\n");
		return -1;
	}
	snprintf(path->bkdir, sizeof(path->bkdir), "%s", v);

	redisdir = myconfig_get_value("path_cdc_redis");
	if (redisdir == NULL)
	{
		LOG(cdc_db_log, LOG_ERROR, "config have not path_cdc_redis!\n");
		return -1;
	}
	process_curday = myconfig_get_intval("process_curday", 1);
	return init_db();
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

	memset(&cdc_path, 0, sizeof(cdc_path));

	if (init_para(&cdc_path))
		return -1;

	time_t cur = time(NULL);
	time_t last = 0;
	int merge_time = myconfig_get_intval("db_merge", 3600);

	while (1)
	{
		do_refresh_run_task();
		cur = time(NULL);
		if (cur - last >= merge_time)
		{
			last = cur;
			merge_db(cur - merge_time);
		}
		sleep (5);
	}
}

