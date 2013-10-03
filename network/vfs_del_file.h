#ifndef __VFS_DEL_FILE_H_
#define __VFS_DEL_FILE_H_

#include "vfs_task.h"
#include <stdint.h>

typedef struct {
	char dtime[10];
	char idle;
	char file[256];
	char line;
} t_vfs_del_file;

int add_2_del_file (t_task_base *base);

int get_from_del_file (t_task_base *base, int next, time_t cur);

int find_last_index(t_task_base *base, time_t last);

#endif
