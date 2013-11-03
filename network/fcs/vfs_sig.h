/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef __VFS_SIG_SO_H
#define __VFS_SIG_SO_H
#include "list.h"
#include "vfs_file_filter.h"
#include "global.h"
#include "vfs_init.h"
#include "vfs_time_stamp.h"
#include "common.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <dirent.h>
#if __GNUC__ < 4
#include "vfs_inotify.h"
#else
#include <sys/inotify.h>
#endif

#define SELF_ROLE ROLE_FCS

enum SOCK_STAT {LOGOUT = 0, CONNECTED, LOGIN, HB_SEND, HB_RSP, IDLE, RECV_LAST, SEND_LAST};

typedef struct {
	list_head_t alist;
	list_head_t hlist;
	list_head_t cfglist;
	list_head_t tasklist;  /*用来确定下游的Tracker 当前执行的任务*/
	list_head_t tlist;  /*Tracker*/
	int fd;
	uint32_t hbtime;
	uint32_t ip;
	uint32_t cfgip;
	int taskcount;
	uint8_t role;  /*FCS, CS, TRACKER */
	uint8_t sock_stat;   /* SOCK_STAT */
	uint8_t server_stat; /* server stat*/
	uint8_t bk;
} vfs_fcs_peer;

typedef struct {
	int flag;   /*0: idle, 1: sync_dir , 2: no more task for sync*/
	int total_sync_task;
	int total_synced_task;
} t_sync_para;
#endif
