/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef __VFS_SIG_TASK_H_
#define __VFS_SIG_TASK_H_
#include "list.h"
#include "global.h"
#include "common.h"
#include "protocol.h"
#include "vfs_init.h"
#include "vfs_task.h"
#include "vfs_sig.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

int do_newtask(int fd, t_vfs_sig_head *h, t_vfs_sig_body *b);

void do_task_rsp(t_vfs_tasklist *task);

void dump_task_info (char *from, int line, t_task_base *task);

int sync_task_2_group(t_vfs_tasklist *task);

void do_sync_dir_req(int fd, t_vfs_sig_body *b);

int do_dispatch_task(uint32_t ip, t_vfs_tasklist *task, vfs_cs_peer *peer);

void do_sync_del_req(int fd, t_vfs_sig_body *b);

#endif
