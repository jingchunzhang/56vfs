/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "log.h"
#include "myconfig.h"
#include "vfs_time_stamp.h"
#include "vfs_init.h"

#include <stdio.h>

extern int glogfd;

static int fd_fcs_time_stamp = -1;
static int fd_cs_time_stamp = -1;

typedef struct {
	time_t dirtime[MAXFCS];
} t_cs_time_stamp;

static int get_dimain_int(char *domain)
{
	char *t = strstr(domain, DOMAIN_PREFIX);
	if (t == NULL)
	{
		LOG(glogfd, LOG_ERROR, "whatis a fu***k domain [%s]\n", domain);
		return -1;
	}
	t += strlen(DOMAIN_PREFIX);
	return atoi(t);
}

static void dump_cs_time_stamp()
{
	int n = lseek(fd_cs_time_stamp, 0, SEEK_SET);
	if (n < 0)
	{
		LOG(glogfd, LOG_ERROR, "lseek err %m\n");
		return ;
	}

	t_cs_time_stamp val[DIR1][DIR2];
	n = read(fd_cs_time_stamp, &val, sizeof(val));
	if (n != sizeof(val))
	{
		LOG(glogfd, LOG_ERROR, "read err %m %d:%d\n", n, sizeof(val));
		return ;
	}
	int i1 = 0;
	int i2 = 0;
	int i3 = 0;
	for (i1 = 0; i1 < DIR1; i1++)
	{
		for (i2 = 0; i2 < DIR2; i2++)
		{
			for (i3 = 0; i3 < MAXFCS; i3++)
			{
				time_t tval = val[i1][i2].dirtime[i3];
				if (tval)
					LOG(glogfd, LOG_DEBUG, "cs[%d][%d][%d] time %d:%s\n", i1, i2, i3, tval, ctime(&tval));
			}
		}
	}
	return;
}

static void dump_fcs_time_stamp()
{
	int n = lseek(fd_fcs_time_stamp, 0, SEEK_SET);
	if (n < 0)
	{
		LOG(glogfd, LOG_ERROR, "lseek err %m\n");
		return ;
	}

	time_t val[DIR1][DIR2];
	n = read(fd_fcs_time_stamp, &val, sizeof(val));
	if (n != sizeof(val))
	{
		LOG(glogfd, LOG_ERROR, "read err %m %d:%d\n", n, sizeof(val));
		return ;
	}
	int i1 = 0;
	int i2 = 0;
	for (i1 = 0; i1 < DIR1; i1++)
	{
		for (i2 = 0; i2 < DIR2; i2++)
		{
			time_t tval = val[i1][i2];
			if (tval)
				LOG(glogfd, LOG_DEBUG, "fcs[%d][%d] time %d:%s\n", i1, i2, tval, ctime(&tval));
		}
	}
	return;
}

int init_fcs_time_stamp()
{
	char *datadir = myconfig_get_value("vfs_path");
	if (datadir == NULL)
		datadir = "../data";
	char tmpfile[256] = {0x0};
	snprintf(tmpfile, sizeof(tmpfile), "%s/vfs_fcs_time_stamp", datadir);
	struct stat filestat;
	if (stat(tmpfile, &filestat))
		LOG(glogfd, LOG_ERROR, "%s not exit init it!\n", tmpfile);

	fd_fcs_time_stamp = open(tmpfile, O_CREAT | O_RDWR | O_LARGEFILE, 0644);
	if (fd_fcs_time_stamp < 0)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", tmpfile);
		return -1;
	}
	if (ftruncate(fd_fcs_time_stamp, sizeof(time_t) * DIR1 * DIR2))
	{
		LOG(glogfd, LOG_ERROR, "ftruncate %s err %m\n", tmpfile);
		return -1;
	}
	dump_fcs_time_stamp();
	return 0;
}

time_t get_fcs_time_stamp_by_int(int dir1, int dir2)
{
	int pos = (dir1 * DIR2 + dir2) * sizeof(time_t);
	int n = lseek(fd_fcs_time_stamp, pos, SEEK_SET);
	if (n < 0)
	{
		LOG(glogfd, LOG_ERROR, "lseek err %m\n");
		return -1;
	}

	time_t val = 0;
	n = read(fd_fcs_time_stamp, &val, sizeof(val));
	if (n != sizeof(val))
	{
		LOG(glogfd, LOG_ERROR, "read err %d:%d %m\n", n, sizeof(val));
		return -1;
	}
	return val;
}

time_t get_fcs_time_stamp(t_task_base *base)
{
	int dir1 = 0, dir2 = 0;
	if (get_dir1_dir2(base->filename, &dir1, &dir2))
		return -1;

	return get_fcs_time_stamp_by_int(dir1, dir2);
}

int set_fcs_time_stamp_by_int(int dir1, int dir2, time_t val)
{
	int pos = (dir1 * DIR2 + dir2) * sizeof(time_t);
	int n = lseek(fd_fcs_time_stamp, pos, SEEK_SET);
	if (n < 0)
	{
		LOG(glogfd, LOG_ERROR, "lseek err %m\n");
		return -1;
	}

	n = write(fd_fcs_time_stamp, &val, sizeof(val));
	if (n != sizeof(val))
	{
		LOG(glogfd, LOG_ERROR, "write err %d:%d %m\n", n, sizeof(val));
		return -1;
	}
	fsync(fd_fcs_time_stamp);
	return 0;
}

int set_fcs_time_stamp(t_task_base *base)
{
	time_t val = base->ctime;
	time_t oldval = get_fcs_time_stamp(base);
	if (oldval >= val)
	{
		LOG(glogfd, LOG_DEBUG, "no update O:%ld N:%ld\n", oldval, val);
		return 0;
	}

	int dir1 = 0, dir2 = 0;
	if (get_dir1_dir2(base->filename, &dir1, &dir2))
		return -1;

	return set_fcs_time_stamp_by_int(dir1, dir2, val);
}

int init_cs_time_stamp()
{
	char *datadir = myconfig_get_value("vfs_path");
	if (datadir == NULL)
		datadir = "../data";
	char tmpfile[256] = {0x0};
	snprintf(tmpfile, sizeof(tmpfile), "%s/vfs_cs_time_stamp", datadir);
	struct stat filestat;
	if (stat(tmpfile, &filestat))
		LOG(glogfd, LOG_ERROR, "%s not exit init it!\n", tmpfile);

	fd_cs_time_stamp = open(tmpfile, O_CREAT | O_RDWR | O_LARGEFILE, 0644);
	if (fd_cs_time_stamp < 0)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", tmpfile);
		return -1;
	}
	if (ftruncate(fd_cs_time_stamp, sizeof(t_cs_time_stamp) * DIR1 * DIR2))
	{
		LOG(glogfd, LOG_ERROR, "ftruncate %s err %m\n", tmpfile);
		return -1;
	}
	dump_cs_time_stamp();
	return 0;
}

time_t get_cs_time_stamp_by_int(int dir1, int dir2, int domain)
{
	if (domain >= MAXFCS)
		domain %= MAXFCS;
	off_t pos = (dir1 * DIR2 + dir2) * sizeof(t_cs_time_stamp) + domain * sizeof(time_t);
	off_t n = lseek(fd_cs_time_stamp, pos, SEEK_SET);
	if (n != pos)
	{
		LOG(glogfd, LOG_ERROR, "lseek err %m\n");
		return -1;
	}

	time_t val;
	n = read(fd_cs_time_stamp, &val, sizeof(val));
	if (n != sizeof(val))
	{
		LOG(glogfd, LOG_ERROR, "read err %d:%d %m\n", n, sizeof(val));
		return -1;
	}
	return val;
}

time_t get_cs_time_stamp(t_task_base *base)
{
	int dir1 = 0, dir2 = 0;
	if (get_dir1_dir2(base->filename, &dir1, &dir2))
		return -1;

	int domain = get_dimain_int(base->src_domain);
	if (domain < 0)
		return -1;

	return get_cs_time_stamp_by_int(dir1, dir2, domain);
}

int set_cs_time_stamp_by_int(int dir1, int dir2, int domain, time_t tval)
{
	if (self_stat != ON_LINE)
	{
		LOG(glogfd, LOG_NORMAL, "self_stat is %d not ON_LINE!\n", self_stat);
		return 0;
	}
	if (domain >= MAXFCS)
		domain %= MAXFCS;
	off_t pos = (dir1 * DIR2 + dir2) * sizeof(t_cs_time_stamp) + domain * sizeof(time_t);
	off_t n = lseek(fd_cs_time_stamp, pos, SEEK_SET);
	if (n != pos)
	{
		LOG(glogfd, LOG_ERROR, "lseek err %m\n");
		return -1;
	}

	n = write(fd_cs_time_stamp, &tval, sizeof(tval));
	if (n != sizeof(tval))
	{
		LOG(glogfd, LOG_ERROR, "write err %d:%d %m\n", n, sizeof(tval));
		return -1;
	}
	fsync(fd_cs_time_stamp);
	return 0;
}

int set_cs_time_stamp(t_task_base *base)
{
	time_t tval = base->ctime;
	time_t oldval = get_cs_time_stamp(base);
	if (oldval >= tval)
	{
		LOG(glogfd, LOG_DEBUG, "no update O:%ld N:%ld\n", oldval, tval);
		return 0;
	}

	int dir1 = 0, dir2 = 0;
	if (get_dir1_dir2(base->filename, &dir1, &dir2))
		return -1;
	int domain = get_dimain_int(base->src_domain);
	if (domain < 0)
		return -1;

	return set_cs_time_stamp_by_int(dir1, dir2, domain, tval);
}

void close_all_time_stamp()
{
	if (fd_fcs_time_stamp > 0)
		close(fd_fcs_time_stamp);
	if (fd_cs_time_stamp > 0)
		close(fd_cs_time_stamp);
}

