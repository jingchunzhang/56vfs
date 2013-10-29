/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _VFS_TIME_STAMP_H_
#define _VFS_TIME_STAMP_H_
#include "vfs_task.h"

int init_fcs_time_stamp();

time_t get_fcs_time_stamp_by_int(int dir1, int dir2);

time_t get_fcs_time_stamp(t_task_base *base);

int set_fcs_time_stamp_by_int(int d1, int d2, time_t val);

int set_fcs_time_stamp(t_task_base *base);

int init_cs_time_stamp();

time_t get_cs_time_stamp_by_int(int dir1, int dir2, int domain);

time_t get_cs_time_stamp(t_task_base *base);

int set_cs_time_stamp_by_int(int d1, int d2, int domain, time_t val);

int set_cs_time_stamp(t_task_base *base);

void close_all_time_stamp();

#endif
