/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef __VFS_SIG_TRACKER_H
#define __VFS_SIG_TRACKER_H
#include "list.h"
#include "global.h"
#include "common.h"
#include "vfs_init.h"
#include "vfs_sig_task.h"
#include "vfs_task.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <dirent.h>

#define SELF_ROLE ROLE_TRACKER

enum SOCK_STAT {LOGOUT = 0, CONNECTED, LOGIN, HB_SEND, HB_RSP, IDLE, RECV_LAST, SEND_LAST};

typedef struct {
	list_head_t alist;
	list_head_t hlist;
	list_head_t cfglist;
	list_head_t tasklist;
	int fd;
	uint32_t hbtime;
	uint32_t ip;
	uint32_t cfgip;
	int taskcount;
	uint8_t role;  /*FCS, CS, TRACKER */
	uint8_t sock_stat;   /* SOCK_STAT */
	uint8_t server_stat; /* server stat*/
	uint8_t isp;
	uint8_t archive_isp;
	uint8_t real_isp;
	uint8_t bk[2];
} vfs_tracker_peer;

int find_ip_stat(uint32_t ip, vfs_tracker_peer **dpeer);

#endif
