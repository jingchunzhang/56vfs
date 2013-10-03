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
