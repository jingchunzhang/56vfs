/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _56_VFS_INIT_H_
#define _56_VFS_INIT_H_
#include "nm_app_vfs.h"
#include "list.h"
#include "vfs_so.h"
#include "global.h"
#include "util.h"
#include <stdint.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h> 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define ALLMASK 0xFF
#define DEFAULT_SCORE 50
extern int glogfd;
extern list_head_t hothome;
extern list_head_t isp_iplist[256];
extern list_head_t cfg_iplist[256];
extern const char *ispname[MAXISP];
extern int init_buff_size ;
enum {PATH_INDIR = 0, PATH_OUTDIR, PATH_WKDIR, PATH_BKDIR, PATH_TMPDIR, PATH_SYNCDIR, PATH_MAXDIR};
extern const char *path_dirs[];

typedef struct {
	uint32_t ip;
	char sip[128]; /*sip or domain */
	char s_ip[16]; /*sip*/
	uint8_t isp;  /*if have */
	uint8_t role;
	uint8_t archive_isp;
	uint8_t real_isp;  /*for some arichive machine reuse :) */
	unsigned char isself:1;
	unsigned char ishot:1;
	unsigned char offline:1;
	unsigned char archive:1;
	unsigned char reserver:4;
	char dirs[MAXDIR_FOR_CS][8];  /* if cs , every cs have about 10 dirs */
} t_ip_info;

typedef struct {
	t_ip_info ipinfo;
	list_head_t hlist;
	list_head_t hotlist;
	list_head_t isplist;
	list_head_t archive_list;
} t_ip_info_list;

typedef struct {
	uint32_t ip;
	list_head_t hlist;
} t_offline_list;

typedef struct {
	uint32_t ip[MAXCS_ONEGRUOP];
	uint8_t isp[MAXCS_ONEGRUOP];
	uint8_t archive_isp[MAXCS_ONEGRUOP];
	uint8_t real_isp[MAXCS_ONEGRUOP];
	uint16_t index;
} t_cs_dir_info;

typedef struct {
	time_t task_timeout;
	time_t real_rm_time;
	uint64_t mindiskfree;
	char path[256];
	char sync_stime[12];
	char sync_etime[12];
	char domain_prefix[32];
	char domain_suffix[32];
	uint16_t sig_port;
	uint16_t data_port;
	uint16_t timeout;
	uint16_t cktimeout;
	uint16_t cs_preday;
	uint16_t fcs_max_connects;
	uint16_t cs_max_connects;
	uint16_t cs_max_task_run_once;
	uint16_t cs_max_task;
	uint16_t fcs_max_task;
	uint16_t tracker_max_task;
	uint16_t dir_gid;
	uint16_t dir_uid;
	uint16_t lock_timeout;
	uint16_t reload_time;
	uint16_t voss_interval;
	uint8_t vfs_test;
	uint8_t cs_sync_dir;
	uint8_t data_calcu_md5;
	int8_t retry;
	uint8_t sync_dir_count;
	uint8_t continue_flag;
} t_g_config;

extern t_g_config g_config;

int vfs_init();

void oper_ip_off_line(uint32_t ip, int type);

void do_ip_off_line(uint32_t ip, int type);

int add_ip_info(t_ip_info *ipinfo);

int get_ip_info(t_ip_info **ipinfo, char *sip, int type);

int get_ip_info_by_uint(t_ip_info **ipinfo, uint32_t ip, int type, char *s_ip, char *sip);

int init_global();

int check_self_ip(uint32_t ip);

int get_self_info(t_ip_info *ipinfo0);

int init_vfs_thread(t_thread_arg *name);

void fini_vfs_thread(char *threadname);

int get_cs_info(int dir1, int dir2, t_cs_dir_info *cs);

int get_cs_info_by_path(char *path, t_cs_dir_info *cs);

int get_dir1_dir2(char *fcsfile, int *dir1, int *dir2);

int get_next_fcs(int fcs, uint8_t isp);

void reload_config();

int reload_cfg();

int get_cfg_lock();

int release_cfg_lock();

void report_err_2_nm (char *file, const char *func, int line, int ret);

void check_self_stat();

int get_isp_by_name(char *name);

#endif
