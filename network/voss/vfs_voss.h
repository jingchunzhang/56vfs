/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef __VFS_SIG_SO_H
#define __VFS_SIG_SO_H
#include "list.h"
#include "global.h"
#include "pro_voss.h"
#include "vfs_init.h"
#include "common.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <dirent.h>

enum SOCK_STAT {LOGOUT = 0, CONNECTED, LOGIN, HB_SEND, HB_RSP, IDLE, RECV_LAST, SEND_LAST, PREPARE_SYNCFILE, SYNCFILEING, SYNCFILE_POST, SYNCFILE_OK, PREPARE_SENDFILE, SENDFILEING, SENDFILE_OK};

extern const char *sock_stat_cmd[] ;

typedef struct {
	int recvlen;
	int datalen;
	time_t opentime;
	char outfile[256];
} t_voss_data_info;

typedef struct {
	list_head_t alist;
	list_head_t hlist;
	list_head_t cfglist;
	char ip[16];
	uint32_t cfgip;
	uint32_t con_ip;
	int fd;
	int local_in_fd; /* 接受vfs侧数据时，写到本地的文件句柄*/
	uint32_t hbtime;
	t_voss_data_info datainfo;
	uint8_t sock_stat;
	uint8_t server_stat;
	uint8_t role;
	uint8_t bk;
} vfs_voss_peer;

extern char *iprole[];
#endif
