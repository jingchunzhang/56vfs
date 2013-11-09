/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_init.h"
#include "vfs_so.h"
#include "vfs_file_filter.h"
#include "log.h"
#include "common.h"
#include "thread.h"
#include "vfs_maintain.h"
#include "acl.h"
#include <stddef.h>

volatile int maintain = 0;		//1-维护配置 0 -可以使用
static pthread_rwlock_t init_rwmutex;
static pthread_rwlock_t offline_rwmutex;
extern t_ip_info self_ipinfo;
extern uint8_t self_stat ;
static uint8_t fcslist[MAXFCS] = {0x0};
int init_buff_size = 20480;
static t_cs_dir_info  csinfo[DIR1][DIR2];

char hostname[128];

static char add_cs_isps[256][16];

const char *ispname[MAXISP] = {"tel", "cnc", "edu", "tt", "yd", "hs", "mp4"};

const char *path_dirs[] = {"indir", "outdir", "workdir", "bkdir", "tmpdir", "syncdir"};

list_head_t hothome;
list_head_t offlinehome;
static list_head_t iphome;
/* cfg list */
list_head_t cfg_iplist[256];
/*isp_list*/
list_head_t isp_iplist[256];
t_g_config g_config;

static uint32_t localip[64];

static int init_local_ip()
{
	memset(localip, 0, sizeof(localip));
	struct ifconf ifc;
	struct ifreq *ifr = NULL;
	int i;
	int nifr = 0;

	i = socket(AF_INET, SOCK_STREAM, 0);
	if(i < 0) 
		return -1;
	ifc.ifc_len = 0;
	ifc.ifc_req = NULL;
	if(ioctl(i, SIOCGIFCONF, &ifc) == 0) {
		ifr = alloca(ifc.ifc_len > 128 ? ifc.ifc_len : 128);
		ifc.ifc_req = ifr;
		if(ioctl(i, SIOCGIFCONF, &ifc) == 0)
			nifr = ifc.ifc_len / sizeof(struct ifreq);
	}
	close(i);

	int index = 0;
	for (i = 0; i < nifr; i++)
	{
		if (!strncmp(ifr[i].ifr_name, "lo", 2))
			continue;
		uint32_t ip = ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr;
		localip[index%64] = ip;
		index++;
	}
	return 0;
}

void report_err_2_nm (char *file, const char *func, int line, int ret)
{
	char val[256] = {0x0};
	snprintf(val, sizeof(val), "%s:%s:%d  ret=%d err %m", file, func, line, ret);
	SetStr(VFS_TASK_MUTEX_ERR, val);
}

int check_self_ip(uint32_t ip)
{
	int i = 0;
	for (i = 0; i < 64; i++)
	{
		if (localip[i] == 0)
			return -1;
		if (localip[i] == ip)
			return 0;
	}
	return -1;
}

void reload_config()
{
	g_config.cs_preday = myconfig_get_intval("cs_preday", 5);
	g_config.fcs_max_connects = myconfig_get_intval("fcs_max_connects", 10);
	g_config.cs_max_connects = myconfig_get_intval("cs_max_connects", 20);
	g_config.cs_max_task_run_once = myconfig_get_intval("cs_max_task_run_once", 32);
	g_config.fcs_max_task = myconfig_get_intval("fcs_max_task", 32);
	g_config.reload_time = myconfig_get_intval("reload_time", 3600);
	g_config.voss_interval = myconfig_get_intval("voss_interval", 300);
	g_config.continue_flag = myconfig_get_intval("continue_flag", 0);
	if (g_config.continue_flag)
		LOG(glogfd, LOG_NORMAL, "vfs run in continue_flag mode !\n");
	else
		LOG(glogfd, LOG_NORMAL, "vfs run in no continue_flag mode !\n");

	g_config.vfs_test = myconfig_get_intval("vfs_test", 0);
	if (g_config.vfs_test)
		LOG(glogfd, LOG_NORMAL, "vfs run in test mode !\n");
	else
		LOG(glogfd, LOG_NORMAL, "vfs run in normal mode !\n");
	g_config.real_rm_time = myconfig_get_intval("real_rm_time", 7200);
	g_config.task_timeout = myconfig_get_intval("task_timeout", 86400);
	g_config.cs_sync_dir = myconfig_get_intval("cs_sync_dir", 0);
	g_config.data_calcu_md5 = myconfig_get_intval("data_calcu_md5", 0);
	g_config.sync_dir_count = myconfig_get_intval("sync_dir_count", 10);
	LOG(glogfd, LOG_NORMAL, "task_timeout = %ld\n", g_config.task_timeout);
}

int init_global()
{
	self_stat = UNKOWN_STAT;
	g_config.sig_port = myconfig_get_intval("sig_port", 39090);
	g_config.data_port = myconfig_get_intval("data_port", 49090);
	g_config.timeout = myconfig_get_intval("timeout", 300);
	g_config.cktimeout = myconfig_get_intval("cktimeout", 5);
	g_config.lock_timeout = myconfig_get_intval("lock_timeout", 10);
	init_buff_size = myconfig_get_intval("socket_buff", 65536);
	if (init_buff_size < 20480)
		init_buff_size = 20480;

	g_config.retry = myconfig_get_intval("vfs_retry", 0) + 1;

	char *cmdnobody = "cat /etc/passwd |awk -F\\: '{if ($1 == \"nobody\") print $3\" \"$4}'";

	FILE *fp = popen(cmdnobody, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "execute  %s err %m\n", cmdnobody);
		return -1;
	}
	char buf[32] = {0x0};
	fgets(buf, sizeof(buf), fp);
	pclose(fp);
	char *t = strchr(buf, ' ');
	if (t == NULL)
	{
		fprintf(stderr, "execute  %s err %m [%s]\n", cmdnobody, buf);
		return -1;
	}
	g_config.dir_uid = atoi(buf);
	g_config.dir_gid = atoi(t+1);

	reload_config();
	char *v_domain = myconfig_get_value("domain_prefix");
	if (v_domain == NULL)
		snprintf(g_config.domain_prefix, sizeof(g_config.domain_prefix), "fcs");
	else
		snprintf(g_config.domain_prefix, sizeof(g_config.domain_prefix), "%s", v_domain);
	v_domain = myconfig_get_value("domain_suffix");
	if (v_domain == NULL)
		snprintf(g_config.domain_suffix, sizeof(g_config.domain_suffix), "56.com");
	else
		snprintf(g_config.domain_suffix, sizeof(g_config.domain_suffix), "%s", v_domain);

	int i = 0;
	char *mindisk = myconfig_get_value("disk_minfree");
	if (mindisk == NULL)
		g_config.mindiskfree = 4 << 30;
	else
	{
		uint64_t unit_size = 1 << 30;
		char *t = mindisk + strlen(mindisk) - 1;
		if (*t == 'M' || *t == 'm')
			unit_size = 1 << 20;
		else if (*t == 'K' || *t == 'k')
			unit_size = 1 << 10;

		i = atoi(mindisk);
		if (i <= 0)
			g_config.mindiskfree = 4 << 30;
		else
			g_config.mindiskfree = i * unit_size;
	}

	char *v = myconfig_get_value("vfs_path");
	if (v == NULL)
		v = "/home/vfs/path";

	char path[256] = {0x0};
	for( i = 0; i < PATH_MAXDIR; i++)
	{
		memset(path, 0, sizeof(path));
		snprintf(path, sizeof(path), "%s/%s", v, path_dirs[i]);
		if (access(path, R_OK|W_OK|X_OK) == 0)
			continue;
		if (mkdir(path, 0755))
		{
			LOG(glogfd, LOG_ERROR, "err mkdir %s %m\n", path);
			return -1;
		}
	}
	snprintf(g_config.path, sizeof(g_config.path), "%s", v);
	v = myconfig_get_value("vfs_sync_starttime");
	if (v == NULL)
		v = "01:00:00";
	snprintf(g_config.sync_stime, sizeof(g_config.sync_stime), "%s", v);
	v = myconfig_get_value("vfs_sync_endtime");
	if (v == NULL)
		v = "09:00:00";
	snprintf(g_config.sync_etime, sizeof(g_config.sync_etime), "%s", v);
	return 0;
}

static void pushcs(t_cs_dir_info *cs, char *sip, uint8_t isp, uint8_t archive_isp, int d1, int d2)
{
	sip = strtrim(sip);
	if (sip == NULL)
	{
		LOG(glogfd, LOG_ERROR, "err %s\n", sip);
		return ;
	}
	char s_ip[16] = {0x0};
	uint32_t ip = get_uint32_ip(sip, s_ip);
	if (ip == INADDR_NONE)
	{
		LOG(glogfd, LOG_ERROR, "err ip %s\n", sip);
		return;
	}
	int reuse = 0;
	int i = 0;
	for(i = 0; i < cs->index; i++)
	{
		if (cs->ip[i] == ip)
		{
			if (archive_isp == ISP_FCS)
			{
				LOG(glogfd, LOG_ERROR, "ip dup %u\n", ip);
				return;
			}
			cs->archive_isp[i] = archive_isp;
			cs->real_isp[i] = isp;
			reuse = 1;
			break;
		}
	}

	if ( reuse == 0)
	{
		cs->ip[cs->index] = ip;
		cs->isp[cs->index] = isp;
		cs->real_isp[cs->index] = isp;
		cs->archive_isp[cs->index] = archive_isp;
		cs->index++;
		if (MAXCS_ONEGRUOP == cs->index)
		{
			LOG(glogfd, LOG_ERROR, "one group too many ip!\n");
			cs->index = 0;
		}
	}

	t_ip_info *ipinfo0 = NULL;
	if (get_ip_info(&ipinfo0, sip, 0) == 0)
	{
		LOG(glogfd, LOG_TRACE, "exist ip %u\n", ip);
		ipinfo0->real_isp = isp;
		if (archive_isp != ISP_FCS)
			ipinfo0->archive = 1;
		if (reuse)
		{
			t_ip_info_list *server = (t_ip_info_list *) (ipinfo0 - offsetof(t_ip_info_list, ipinfo));
			list_del_init(&(server->archive_list));
			list_head_t *isplist = &(isp_iplist[archive_isp&MAXISP]);
			list_add_head(&(server->archive_list), isplist);
			ipinfo0->archive_isp = archive_isp;
		}
		char dirs[8] = {0x0};
		snprintf(dirs, sizeof(dirs), "%d/%d", d1, d2);
		int i = 0;
		for (i = 0; i < MAXDIR_FOR_CS; i++)
		{
			if (strcmp(ipinfo0->dirs[i], dirs) == 0)
			{
				LOG(glogfd, LOG_TRACE, "ip %u dir [%s] exist!\n", ip, ipinfo0->dirs[i]);
				return;
			}
			if (strlen(ipinfo0->dirs[i]) == 0)
			{
				snprintf(ipinfo0->dirs[i], sizeof(ipinfo0->dirs[i]), "%s", dirs);
				LOG(glogfd, LOG_TRACE, "ip %u dir [%s]!\n", ip, ipinfo0->dirs[i]);
				return;
			}
		}
		LOG(glogfd, LOG_ERROR, "too many dir for ip %u!\n", ip);
		return ;
	}
	t_ip_info ipinfo;
	memset(&ipinfo, 0, sizeof(ipinfo));
	ipinfo.isp = isp;
	ipinfo.real_isp = isp;
	ipinfo.archive_isp = archive_isp;
	if (archive_isp != ISP_FCS)
		ipinfo.archive = 1;
	ipinfo.ip = ip;
	ipinfo.role = ROLE_CS;
	snprintf(ipinfo.sip, sizeof(ipinfo.sip), "%s", sip);
	snprintf(ipinfo.s_ip, sizeof(ipinfo.s_ip), "%s", s_ip);
	snprintf(ipinfo.dirs[0], sizeof(ipinfo.dirs[0]), "%d/%d", d1, d2);
	add_ip_info(&ipinfo);
}

static void pushinfo(t_cs_dir_info *cs, char *ips, uint8_t isp, uint8_t archive_isp, int d1, int d2)
{
	char *s = ips;
	char *e = NULL;
	while (1)
	{
		e = strchr(s, ',');
		if (e == NULL)
			break;
		*e = 0x0;
		e++;
		pushcs(cs, s, isp, archive_isp, d1, d2);
		s = e;
	}
	pushcs(cs, s, isp, archive_isp, d1, d2);
}

static int process_csfile(uint8_t isp, uint8_t archive_isp, char *csfile)
{
	FILE *fp = fopen(csfile, "r");
	if (fp == NULL)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", csfile);
		return -1;
	}
	char buf[2048] = {0x0};
	while (fgets(buf, sizeof(buf), fp))
	{
		char *t = strrchr(buf, '\n');
		if (t)
			*t = 0x0;

		t = strchr(buf, '/');
		if (t == NULL)
		{
			LOG(glogfd, LOG_ERROR, "[%s]:[%s] err format\n", csfile, buf);
			continue;
		}
		uint8_t dir1, dir2;
		dir1 = atoi(buf);
		dir2 = atoi(t + 1);
		dir1 = dir1%DIR1;
		dir2 = dir2%DIR2;
		t_cs_dir_info *cs = &(csinfo[dir1][dir2]);

		char *t1 = strchr(t, ' ');
		if (t1 == NULL)
			t1 = strchr(t, '\t');
		if (t1 == NULL)
		{
			LOG(glogfd, LOG_ERROR, "[%s]:[%s] err format\n", csfile, buf);
			continue;
		}
		pushinfo(cs, t1+1, isp, archive_isp, dir1, dir2);
	}
	fclose(fp);
	return 0;
}

static int process_csfile_hot(char *csfile)
{
	FILE *fp = fopen(csfile, "r");
	if (fp == NULL)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", csfile);
		return -1;
	}
	char buf[2048] = {0x0};
	while (fgets(buf, sizeof(buf), fp))
	{
		char *t = strrchr(buf, '\n');
		if (t)
			*t = 0x0;
		t_ip_info *ipinfo0 = NULL;
		if (get_ip_info(&ipinfo0, buf, 0) == 0)
		{
			t_ip_info_list *t_ip_list =  (t_ip_info_list *) ((char *) (ipinfo0) - offsetof(t_ip_info_list, ipinfo));
			list_del_init (&(t_ip_list->hotlist));
			LOG(glogfd, LOG_NORMAL, "ip %s add int hot %s\n", buf, ispname[ipinfo0->isp]);
			list_add_head(&(t_ip_list->hotlist), &hothome);
		}
		else
			LOG(glogfd, LOG_ERROR, "ip %s not in new_cs*.txt\n", buf);
	}
	fclose(fp);
	return 0;
}

static int init_csinfo_archive()
{
	char *csdir = myconfig_get_value("iplist_cs_dir");
	if (csdir == NULL)
	{
		LOG(glogfd, LOG_ERROR, "no cs_dir!\n");
		return -1;
	}

	DIR *dp;
	struct dirent *dirp;
	char fullfile[256];
	uint8_t isp = 0;
	uint8_t archive_isp = 0;

	if ((dp = opendir(csdir)) == NULL) 
	{
		LOG(glogfd, LOG_ERROR, "opendir %s err  %m\n", csdir);
		return -1;
	}
	int ret = 0;
	while((dirp = readdir(dp)) != NULL) {
		if (dirp->d_name[0] == '.')
			continue;

		char *tmp = dirp->d_name + strlen(dirp->d_name) -4;
		if (strcmp(tmp, ".txt"))
			continue;
		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", csdir, dirp->d_name);

		*tmp = 0x0;
		char *t = strstr(dirp->d_name, "new_cs_");
		if (!t)
			continue;
		t += 7;

		isp = UNKNOW_ISP;
		archive_isp = UNKNOW_ISP;
		char *t1 = strstr(t, "_archive");
		if (t1 == NULL)
		{
			LOG(glogfd, LOG_DEBUG, "%s:%d error csfile %s ignore it\n", FUNC, LN, fullfile);
			continue;
		}
		/* start process archive csfile*/
		*t1 = 0x0;
		isp = get_isp_by_name(t);
		archive_isp = get_isp_by_name(t1 + 1);
		if (archive_isp == UNKNOW_ISP || UNKNOW_ISP == isp || isp == archive_isp)
		{
			LOG(glogfd, LOG_ERROR, "%s:%d error csfile %s ignore it\n", FUNC, LN, fullfile);
			continue;
		}

		LOG(glogfd, LOG_NORMAL, "%s:%d process cs %s\n", FUNC, LN, fullfile);
		if (process_csfile(isp, archive_isp, fullfile))
		{
			ret = -1;
			break;
		}
	}
	closedir(dp);
	return ret;
}

static int init_csinfo_hot()
{
	char *csdir = myconfig_get_value("iplist_cs_dir");
	if (csdir == NULL)
	{
		LOG(glogfd, LOG_ERROR, "no cs_dir!\n");
		return -1;
	}

	DIR *dp;
	struct dirent *dirp;
	char fullfile[256];
	uint8_t isp = 0;

	if ((dp = opendir(csdir)) == NULL) 
	{
		LOG(glogfd, LOG_ERROR, "opendir %s err  %m\n", csdir);
		return -1;
	}
	while((dirp = readdir(dp)) != NULL) {
		if (dirp->d_name[0] == '.')
			continue;

		char *t = strstr(dirp->d_name, "mp4");
		if (t)
			continue;
		t = strstr(dirp->d_name, ".txt");
		if (!t)
			continue;
		if (strcmp(dirp->d_name + strlen(dirp->d_name) -4, ".txt"))
			continue;
		t = strstr(dirp->d_name, "hot_cs_");
		if (!t)
			continue;

		t += 7;
		if (*t == 'c')
			isp = CNC;
		else if (*t == 't')
			isp = TEL;
		else
		{
			LOG(glogfd, LOG_ERROR, "err hot file %s\n", dirp->d_name);
			continue;
		}

		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", csdir, dirp->d_name);
		LOG(glogfd, LOG_NORMAL, "process cs %s\n", fullfile);
		process_csfile_hot(fullfile);
	}
	closedir(dp);
	return init_csinfo_archive();
}

static int init_fcs(uint8_t isp)
{
	char *csdir = myconfig_get_value("iplist_cs_dir");
	if (csdir == NULL)
	{
		LOG(glogfd, LOG_ERROR, "no cs_dir!\n");
		return -1;
	}
	char fcsfile[256] = {0x0};
	if (isp == ISP_FCS)
		snprintf(fcsfile, sizeof(fcsfile), "%s/fcs_list.txt", csdir);
	else
		snprintf(fcsfile, sizeof(fcsfile), "%s/fcs_list_%s.txt", csdir, ispname[isp]);

	FILE *fp = fopen(fcsfile, "r");
	if (fp == NULL)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", fcsfile);
		return -1;
	}
	t_ip_info ipinfo;
	char buf[2048] = {0x0};
	while (fgets(buf, sizeof(buf), fp))
	{
		char *t = strrchr(buf, '\n');
		if (t)
			*t = 0x0;
		memset(&ipinfo, 0, sizeof(ipinfo));
		snprintf(ipinfo.sip, sizeof(ipinfo.sip), "%s", buf);
		uint32_t ip = get_uint32_ip(ipinfo.sip, ipinfo.s_ip);
		if (ip == INADDR_NONE)
		{
			LOG(glogfd, LOG_ERROR, "err ip %s\n", ipinfo.sip);
			continue;
		}
		ipinfo.isp = isp;
		ipinfo.archive_isp = isp;
		ipinfo.role = ROLE_FCS;
		fcslist[atoi(ipinfo.sip+3)%MAXFCS] = isp;
		ipinfo.ip = ip;
		add_ip_info(&ipinfo);
		LOG(glogfd, LOG_NORMAL, "ip %s %s\n", ipinfo.sip, ipinfo.s_ip);
	}
	fclose(fp);
	return 0;
}

static int init_voss()
{
	int i = 0;
	char *vossip = NULL;
	for(i = 0; (vossip = myconfig_get_multivalue( "voss_server_ip", i)) != NULL; i++)
	{
		t_ip_info ipinfo;
		memset(&ipinfo, 0, sizeof(ipinfo));
		snprintf(ipinfo.sip, sizeof(ipinfo.sip), "%s", vossip);
		uint32_t ip = get_uint32_ip(ipinfo.sip, ipinfo.s_ip);
		if (ip == INADDR_NONE)
		{
			LOG(glogfd, LOG_ERROR, "err ip %s\n", ipinfo.sip);
			return -1;
		}
		ipinfo.isp = ISP_VOSS;
		ipinfo.archive_isp = UNKNOW_ISP;
		ipinfo.role = ROLE_VOSS_MASTER;
		ipinfo.ip = ip;
		add_ip_info(&ipinfo);
		LOG(glogfd, LOG_DEBUG, "%s:%d %s\n", FUNC, LN, ipinfo.sip);
	}

	if (i == 0)
	{
		LOG(glogfd, LOG_ERROR, "no voss_server_ip!\n");
		return -1;
	}
	return 0;
}

static int init_tracker()
{
	char* pval = NULL; int i = 0;
	t_ip_info ipinfo;
	for( i = 0; ( pval = myconfig_get_multivalue( "iplist_sig_tracker", i ) ) != NULL; i++ )
	{
		memset(&ipinfo, 0, sizeof(ipinfo));
		snprintf(ipinfo.sip, sizeof(ipinfo.sip), "%s", pval);
		uint32_t ip = get_uint32_ip(ipinfo.sip, ipinfo.s_ip);
		if (ip == INADDR_NONE)
		{
			LOG(glogfd, LOG_ERROR, "err ip %s\n", ipinfo.sip);
			return -1;
		}
		ipinfo.isp = ISP_TRACKER;
		ipinfo.archive_isp = UNKNOW_ISP;
		ipinfo.role = ROLE_TRACKER;
		ipinfo.ip = ip;
		add_ip_info(&ipinfo);
	}
	return 0;
}

static int init_csinfo()
{
	char *csdir = myconfig_get_value("iplist_cs_dir");
	if (csdir == NULL)
	{
		LOG(glogfd, LOG_ERROR, "no cs_dir!\n");
		return -1;
	}

	DIR *dp;
	struct dirent *dirp;
	char fullfile[256];
	uint8_t isp = 0;

	if ((dp = opendir(csdir)) == NULL) 
	{
		LOG(glogfd, LOG_ERROR, "opendir %s err  %m\n", csdir);
		return -1;
	}
	int ret = 0;
	while((dirp = readdir(dp)) != NULL) {
		if (dirp->d_name[0] == '.')
			continue;

		char *tmp = dirp->d_name + strlen(dirp->d_name) -4;
		if (strcmp(tmp, ".txt"))
			continue;
		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", csdir, dirp->d_name);

		*tmp = 0x0;
		char *t = strstr(dirp->d_name, "new_cs_");
		if (!t)
			continue;
		t += 7;

		isp = UNKNOW_ISP;
		if (strstr(t, "_mp4"))  /*faint ,mp4 need special process!*/
			isp = MP4;
		else
			isp = get_isp_by_name(t);
		if (isp == UNKNOW_ISP)
		{
			LOG(glogfd, LOG_DEBUG, "%s:%d error csfile %s ignore it\n", FUNC, LN, fullfile);
			continue;
		}

		LOG(glogfd, LOG_NORMAL, "%s:%d process cs %s\n", FUNC, LN, fullfile);
		if (process_csfile(isp, ISP_FCS, fullfile))
		{
			ret = -1;
			break;
		}
	}
	closedir(dp);
	if (ret)
		return -1;
	return init_csinfo_hot();
}

void do_ip_off_line(uint32_t ip, int type)
{
	if (ip == self_ipinfo.ip)
	{
		if (type == M_OFFLINE)
		{
			if (self_stat != OFF_LINE)
			{
				self_stat = OFF_LINE;
				if (self_offline_time == 0)
					self_offline_time = time(NULL);
				LOG(glogfd, LOG_NORMAL, "self off_line time %s", ctime(&self_offline_time));
			}
		}
		else
		{
			if (self_stat == OFF_LINE)
				self_stat = UNKOWN_STAT;
		}
	}
	if (get_cfg_lock())
		return ;
	t_ip_info *ipinfo = NULL;
	if (get_ip_info_by_uint(&ipinfo, ip, 0, " ", " "))
		LOG(glogfd, LOG_ERROR, "get_ip_info_by_uint %u\n", ip);
	else
		ipinfo->offline = type;
	release_cfg_lock();
}

static void refresh_offline()
{
	t_offline_list *server = NULL;
	list_head_t *l;
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	int ret = pthread_rwlock_timedrdlock(&offline_rwmutex, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_timedrdlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return ;
		}
	}
	list_for_each_entry_safe_l(server, l, &offlinehome, hlist)
	{
		do_ip_off_line(server->ip, M_OFFLINE);
	}
	if (pthread_rwlock_unlock(&offline_rwmutex))
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_unlock error %m\n", FUNC, LN);
		report_err_2_nm(ID, FUNC, LN, ret);
	}
}

void oper_ip_off_line(uint32_t ip, int type)
{
	t_offline_list *server = NULL;
	list_head_t *l;
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	int ret = pthread_rwlock_timedwrlock(&offline_rwmutex, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_timedwrlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return ;
		}
	}
	int get = 0;
	list_for_each_entry_safe_l(server, l, &offlinehome, hlist)
	{
		if (server->ip == ip)
		{
			get = 1;
			if (type == M_ONLINE)
			{
				list_del_init(&(server->hlist));
				free(server);
			}
			break;
		}
	}
	if (get == 0 && type == M_OFFLINE)
	{
		server = malloc(sizeof(t_offline_list));
		if (server == NULL)
			LOG(glogfd, LOG_ERROR, "ERR %s:%d malloc error %m\n", FUNC, LN);
		else
		{
			server->ip = ip;
			INIT_LIST_HEAD(&(server->hlist));
			list_add_head(&(server->hlist), &offlinehome);
		}
	}
	if (pthread_rwlock_unlock(&offline_rwmutex))
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_unlock error %m\n", FUNC, LN);
		report_err_2_nm(ID, FUNC, LN, ret);
	}
}

static void init_cs_isp()
{
	int i = 0;
	for ( i = MP4 + 1 ; i < MAXISP; i++)
	{
		ispname[i] = "";
	}

	memset(add_cs_isps, 0, sizeof(add_cs_isps));
	char* pval = NULL;
	for( i = 0; ( pval = myconfig_get_multivalue( "csisp_newone", i ) ) != NULL; i++ )
	{
		char *t = strchr(pval, ',');
		if (t == NULL)
		{
			LOG(glogfd, LOG_ERROR, "err csisp_newone = %s\n", pval);
			continue;
		}
		*t = 0x0;
		uint8_t isp = atoi(t+1)%256;
		snprintf(add_cs_isps[isp], 16, "%s", pval);
		ispname[isp] = add_cs_isps[isp];
		if (strncmp(pval, "archive", 7) == 0)
			init_fcs(isp);
		*t = ',';
	}
	ispname[ALL] = "all";
	ispname[ALLHOT] = "allhot";
	ispname[ISP_TRACKER] = "tracker";
	ispname[ISP_FCS] = "fcs";
	ispname[ISP_VOSS] = "voss";
}

int reload_cfg()
{
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	int ret = pthread_rwlock_timedwrlock(&init_rwmutex, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_timedwrlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -2;
		}
	}
	memset(csinfo, 0, sizeof(csinfo));
	int index = 0;
	for (index = 0; index < 256; index++)
	{
		list_head_t *hashlist = &(cfg_iplist[index&ALLMASK]);
		t_ip_info_list *server = NULL;
		list_head_t *l;
		list_for_each_entry_safe_l(server, l, hashlist, hlist)
		{
			list_del_init(&(server->hlist));
			list_del_init(&(server->hotlist));
			list_del_init(&(server->isplist));
			list_del_init(&(server->archive_list));
			list_add_head(&(server->hlist), &iphome);
		}
	}
	init_cs_isp();
	if (init_csinfo())
	{
		LOG(glogfd, LOG_ERROR, "init_csinfo error %m\n");
		return -1;
	}
	if (init_tracker())
	{
		LOG(glogfd, LOG_ERROR, "init_tracker error %m\n");
		return -1;
	}
	if (init_fcs(ISP_FCS))
	{
		LOG(glogfd, LOG_ERROR, "init_fcs error %m\n");
		return -1;
	}
	if (init_voss())
	{
		LOG(glogfd, LOG_ERROR, "init_voss error %m\n");
		return -1;
	}
	if (pthread_rwlock_unlock(&init_rwmutex))
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_unlock error %m\n", FUNC, LN);
		report_err_2_nm(ID, FUNC, LN, ret);
	}
	refresh_offline();
	return 0;
}

int get_isp_by_name(char *name)
{
	int i = 0;
	int nl = strlen(name);
	for (; i < MAXISP; i++)
	{
		int l = strlen(ispname[i]);
		if (l == 0)
			continue;
		if (l != nl)
			continue;
		if (strncmp(name, ispname[i], nl) == 0)
			return i;
	}
	return UNKNOW_ISP;
}

int vfs_init()
{
	memset(hostname, 0, sizeof(hostname));
	char *defaulthost = "/home/server/fcs.txt";
	FILE *fp = fopen(defaulthost, "r");
	if (fp)
	{
		fgets(hostname, sizeof(hostname) - 1, fp);
		fclose(fp);
		char *t = strrchr(hostname, '\n');
		if (t)
			*t = 0x0;
		if (strlen(hostname) < 5)
		{
			LOG(glogfd, LOG_ERROR, "err hostname [%s]\n", hostname);
			memset(hostname, 0, sizeof(hostname));
		}
	}
	if (strlen(hostname) == 0 && gethostname(hostname, sizeof(hostname)))
	{
		LOG(glogfd, LOG_ERROR, "gethostname error %m\n");
		return -1;
	}
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(&cfg_iplist[i]);
		INIT_LIST_HEAD(&isp_iplist[i]);
	}

	INIT_LIST_HEAD(&iphome);
	INIT_LIST_HEAD(&hothome);
	INIT_LIST_HEAD(&offlinehome);
	t_ip_info_list *ipall = (t_ip_info_list *) malloc (sizeof(t_ip_info_list) * 2048);
	if (ipall == NULL)
	{
		LOG(glogfd, LOG_ERROR, "malloc t_ip_info_list error %m\n");
		return -1;
	}
	memset(ipall, 0, sizeof(t_ip_info_list) * 2048);

	for (i = 0; i < 2048; i++)
	{
		INIT_LIST_HEAD(&(ipall->hlist));
		INIT_LIST_HEAD(&(ipall->hotlist));
		INIT_LIST_HEAD(&(ipall->isplist));
		INIT_LIST_HEAD(&(ipall->archive_list));
		list_add_head(&(ipall->hlist), &iphome);
		ipall++;
	}

	if (init_local_ip())
	{
		LOG(glogfd, LOG_ERROR, "init_local_ip error %m\n");
		return -1;
	}
	if (pthread_rwlock_init(&init_rwmutex, NULL))
	{
		report_err_2_nm(ID, FUNC, LN, 0);
		LOG(glogfd, LOG_ERROR, "pthread_rwlock_init error %m\n");
		return -1;
	}
	if (pthread_rwlock_init(&offline_rwmutex, NULL))
	{
		report_err_2_nm(ID, FUNC, LN, 0);
		LOG(glogfd, LOG_ERROR, "pthread_rwlock_init error %m\n");
		return -1;
	}
	if (reload_cfg())
	{
		LOG(glogfd, LOG_ERROR, "reload_cfg error %m\n");
		return -1;
	}
	return init_file_filter();
}

int add_ip_info(t_ip_info *ipinfo)
{
	t_ip_info *ipinfo0;
	if (get_ip_info(&ipinfo0, ipinfo->sip, 0) == 0)
	{
		LOG(glogfd, LOG_ERROR, "exist ip %s\n", ipinfo->sip);
		return 0;
	}
	if (check_self_ip(ipinfo->ip) == 0)
	{
		LOG(glogfd, LOG_DEBUG, "self ip %s\n", ipinfo->s_ip);
		ipinfo->isself = 1;
	}
	list_head_t *hashlist = &(cfg_iplist[ipinfo->ip&ALLMASK]);
	t_ip_info_list *server = NULL;
	list_head_t *l;
	int get = 0;
	list_for_each_entry_safe_l(server, l, &iphome, hlist)
	{
		list_del_init(&(server->hlist));
		list_del_init(&(server->hotlist));
		list_del_init(&(server->isplist));
		list_del_init(&(server->archive_list));
		get = 1;
		break;
	}
	if (get == 0)
	{
		server = (t_ip_info_list *)malloc(sizeof(t_ip_info_list));
		LOG(glogfd, LOG_ERROR, "MALLOC\n");
	}
	if (server == NULL)
	{
		LOG(glogfd, LOG_ERROR, "malloc err %m\n");
		return -1;
	}
	INIT_LIST_HEAD(&(server->hlist));
	INIT_LIST_HEAD(&(server->hotlist));
	INIT_LIST_HEAD(&(server->isplist));
	INIT_LIST_HEAD(&(server->archive_list));
	memcpy(&(server->ipinfo), ipinfo, sizeof(t_ip_info));
	list_add_head(&(server->hlist), hashlist);
	if (ipinfo->role == ROLE_CS)
	{
		list_head_t *isplist = &(isp_iplist[ipinfo->isp&MAXISP]);
		list_add_head(&(server->isplist), isplist);
		if (ipinfo->archive_isp != ISP_FCS)
		{
			isplist = &(isp_iplist[ipinfo->archive_isp&MAXISP]);
			list_add_head(&(server->archive_list), isplist);
		}
	}
	LOG(glogfd, LOG_NORMAL, "add ip %s %s\n", ipinfo->s_ip, ipinfo->sip);
	return 0;
}

int get_ip_info_by_uint(t_ip_info **ipinfo, uint32_t ip, int type, char *s_ip, char *sip)
{
	if (type)
	{
		if (get_cfg_lock())
			return -2;
	}
	int ret = -1;
	list_head_t *hashlist = &(cfg_iplist[ip&ALLMASK]);
	t_ip_info_list *server = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(server, l, hashlist, hlist)
	{
		if (ip == server->ipinfo.ip || strcmp(server->ipinfo.sip, sip) == 0 || strcmp(server->ipinfo.s_ip, s_ip) == 0)
		{
			if (type == 0)
				*ipinfo = &(server->ipinfo);
			else
				memcpy(*ipinfo, &(server->ipinfo), sizeof(t_ip_info));
			ret = 0;
			break;
		}
	}
	if (type)
		release_cfg_lock();
	return ret;
}

int get_ip_info(t_ip_info **ipinfo, char *sip, int type)
{
	char s_ip[16] = {0x0};
	uint32_t ip = get_uint32_ip(sip, s_ip);
	return get_ip_info_by_uint(ipinfo, ip, type, s_ip, sip);
}

int get_self_info(t_ip_info *ipinfo0)
{
	int i = 0;
	t_ip_info *ipinfo;
	for (i = 0; i < 64; i++)
	{
		if (localip[i] == 0)
			return -1;
		char ip[16] = {0x0};
		ip2str(ip, localip[i]);
		LOG(glogfd, LOG_DEBUG, "%s:%d %s\n", FUNC, LN, ip);
		if (get_ip_info(&ipinfo, ip, 0) == 0)
		{
			memcpy(ipinfo0, ipinfo, sizeof(t_ip_info));
			return 0;
		}
	}
	return -1;
}

int init_vfs_thread(t_thread_arg *arg)
{
	int iret = 0;
	if((iret = register_thread(arg->name, vfs_signalling_thread, (void *)arg)) < 0)
		return iret;
	LOG(glogfd, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	return 0;
}

void fini_vfs_thread(char *name)
{
	return;
}

int get_cs_info(int dir1, int dir2, t_cs_dir_info *cs)
{
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	int ret = pthread_rwlock_timedrdlock(&init_rwmutex, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_timedrdlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -1;
		}
	}

	memcpy(cs, &(csinfo[dir1%DIR1][dir2%DIR2]), sizeof(t_cs_dir_info));
	if (pthread_rwlock_unlock(&init_rwmutex))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_unlock error %m\n", FUNC, LN);
	return 0;
}

int get_cs_info_by_path(char *path, t_cs_dir_info *cs)
{
	int dir1, dir2;
	if (get_dir1_dir2(path, &dir1, &dir2))
		return -1;
	return get_cs_info(dir1, dir2, cs);
}

int get_dir1_dir2(char *pathsrc, int *dir1, int *dir2)
{
	char path[256] = {0x0};
	snprintf(path, sizeof(path), "%s", pathsrc);
	char *s = path;
	char *t = strrchr(path, '/');
	if (t == NULL)
	{
		LOG(glogfd, LOG_ERROR, "error path %s\n", pathsrc);
		return -1;
	}
	*t = 0x0;
	t = strrchr(s, '/');
	if (t == NULL)
	{
		LOG(glogfd, LOG_ERROR, "error path %s\n", pathsrc);
		return -1;
	}
	*t = 0x0;
	t++;
	*dir2 = atoi(t)%DIR2;

	t = strrchr(s, '/');
	if (t == NULL)
	{
		LOG(glogfd, LOG_ERROR, "error path %s\n", pathsrc);
		return -1;
	}
	t++;
	*dir1 = atoi(t)%DIR1;
	if (*dir1 < 0 || *dir2 < 0)
	{
		LOG(glogfd, LOG_ERROR, "path %s get %d %d err\n", path, *dir1, *dir2);
		return -1;
	}
	LOG(glogfd, LOG_TRACE, "path %s get %d %d\n", path, *dir1, *dir2);
	return 0;
}

int get_next_fcs(int fcs, uint8_t isp)
{
	int i; 
	for(i = fcs+1; i < MAXFCS; i++)
	{
		if (fcslist[i] == isp)
			return i;
	}
	return -1;
}

int get_cfg_lock()
{
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	int ret = pthread_rwlock_timedrdlock(&init_rwmutex, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_timedrdlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -1;
		}
	}
	return 0;
}

int release_cfg_lock()
{
	if (pthread_rwlock_unlock(&init_rwmutex))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_unlock error %m\n", FUNC, LN);
	return 0;
}

void check_self_stat()
{
	t_ip_info ipinfo;
	t_ip_info *ipinfo0 = &ipinfo;
	while (1)
	{
		int ret = get_ip_info(&ipinfo0, self_ipinfo.sip, 1);
		if (ret == -2)
		{
			sleep(1);
			continue;
		}
	
		if (ret == 0)
		{
			if (ipinfo0->offline)
				break;
			if (self_stat == OFF_LINE)
				self_stat = UNKOWN_STAT;
			return;
		}
		break;
	}
	if (self_stat != OFF_LINE)
	{
		self_stat = OFF_LINE;
		self_offline_time = time(NULL);
	}
}

