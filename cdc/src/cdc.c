#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <dirent.h>
#include "myconfig.h"
#include "util.h"
#include "cdc_hash.h"
#include "bitops.h"
#include "log.h"
#include "common.h"
#include "nm_app_vfs.h"

#define ID __FILE__
#define LN __LINE__

#ifndef CDC_TEST
#define DEFAULT_NODE 1<<24    
#else
#define DEFAULT_NODE 1<<10
#endif

static char taskfile_prefix[128];
static char prefix_len = 0;
int fplog = -1;

const char *alarm_str[] = {"cdc normal msg", "cdc too many ip", "cdc add node err", "cdc shm init err"};
static void report_2_nm(uint32_t id, char *f, int l)
{
	char msg[256] = {0x0};
	snprintf(msg, sizeof(msg), "%s in %s:%d %m", alarm_str[id - NM_STR_CDC_BASE], f, l);
	SetStr(id, msg);
}

#include "cdc_sub.c"
#include "fp.c"
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

	v = myconfig_get_value("path_workdir");
	if (v == NULL)
	{
		LOG(fplog, LOG_ERROR, "config have not path_workdir!\n");
		return -1;
	}
	snprintf(path->workdir, sizeof(path->workdir), "%s", v);

	v = myconfig_get_value("path_indir");
	if (v == NULL)
	{
		LOG(fplog, LOG_ERROR, "config have not path_indir!\n");
		return -1;
	}
	snprintf(path->indir, sizeof(path->indir), "%s", v);

	v = myconfig_get_value("path_outdir");
	if (v == NULL)
	{
		LOG(fplog, LOG_ERROR, "config have not path_outdir!\n");
		return -1;
	}
	snprintf(path->outdir, sizeof(path->outdir), "%s", v);

	v = myconfig_get_value("path_bkdir");
	if (v == NULL)
	{
		LOG(fplog, LOG_ERROR, "config have not path_bkdir!\n");
		return -1;
	}
	snprintf(path->bkdir, sizeof(path->bkdir), "%s", v);

	v = myconfig_get_value("path_fulldir");
	if (v == NULL)
	{
		LOG(fplog, LOG_ERROR, "config have not path_fulldir!\n");
		return -1;
	}
	snprintf(path->fulldir, sizeof(path->fulldir), "%s", v);

	v = myconfig_get_value("path_tmpdir");
	if (v == NULL)
	{
		LOG(fplog, LOG_ERROR, "config have not path_tmpdir!\n");
		return -1;
	}
	snprintf(path->tmpdir, sizeof(path->tmpdir), "%s", v);

	memset(taskfile_prefix, 0, sizeof(taskfile_prefix));
	v = myconfig_get_value("path_taskfile_prefix");
	if (v == NULL)
		snprintf(taskfile_prefix, sizeof(taskfile_prefix), "%s", "/home/webadm/");
	else
		snprintf(taskfile_prefix, sizeof(taskfile_prefix), "%s", v);
	prefix_len = strlen(taskfile_prefix);
	return 0;
}

static int init_shm(t_path_info * path)
{
	if (link_cdc_write() == 0)
		return 1;
	LOG(fplog, LOG_NORMAL, "link_cdc_write err %m, try init from file!\n");
	char file[256] = {0x0};
	if (get_last_sync_file(path, file, sizeof(file)))
	{
		size_t nodesize = myconfig_get_intval("shm_nodecount", DEFAULT_NODE);
		LOG(fplog, LOG_NORMAL, "get_last_sync_file err %m, try create now nodecount = %ld!\n", nodesize);
		return link_cdc_create(nodesize);
	}

	LOG(fplog, LOG_NORMAL, "prepare cdc_restore_data from %s\n", file);
	return cdc_restore_data(file);
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

	int ret = init_shm(&path);
	if (ret < 0)
	{
		LOG(fplog, LOG_ERROR, "init_shm err %m\n");
		report_2_nm(CDC_SHM_INIT_ERR, ID, LN);
		return -1;
	}

	LOG(fplog, LOG_NORMAL, "cdc start ok!\n");
	if (ret == 0 )
	{
		if (cdc_sub(&path, CDC_PLUS))
		{
			LOG(fplog, LOG_ERROR, "init_plus err %m\n");
			report_2_nm(CDC_NORMAL_ERR, ID, LN);
			return -1;
		}
	}
	LOG(fplog, LOG_NORMAL, "plus dir process ok!\n");

	time_t cur = time(NULL);
	time_t last = 0;
	int shm_sync_time = myconfig_get_intval("shm_synctime", 3600);

	while (1)
	{
		cdc_sub(&path, CDC_REALTIME);
		cur = time(NULL);
		if (cur - last >= shm_sync_time)
		{
			last = cur;
			do_sync_2_disk(&path);
		}
		sleep(10);
	}

	logclose(fplog);
	return 0;
}

