/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _VFS_LOCALFILE_H_
#define _VFS_LOCALFILE_H_

#include "vfs_task.h"

enum {LOCALFILE_OK = 0, LOCALFILE_TMPFILE, LOCALFILE_MD5_E, LOCALFILE_SIZE_E, LOCALFILE_DIR_E, LOCALFILE_FILE_E, LOCALFILE_OPEN_E, LOCALFILE_RENAME_E, LOCALFILE_UNLINK_E};

enum {VIDEOFILE = 0, VIDEOTMP};

enum {DISK_OK = 0, DISK_SPACE_TOO_SMALL, DISK_ERR};

/*
 *校验本地文件信息 包括文件名，大小，md5校验
 */
int check_localfile_md5(t_task_base *task, int type);

/*
 *CS作为源头PUSH文件时使用
 */
int open_localfile_4_read(t_task_base *task, int *fd);

/*
 *CS作为目的机器时，先在本地打开一个临时文件
 */
int open_tmp_localfile_4_write(t_task_base *task, int *fd);

/*
 *CS作为目的机器时，文件传输完成后，关闭临时文件，校验并且mv到输出文件```
 */
int close_tmp_check_mv(t_task_base *task, int fd);

int delete_localfile(t_task_base *task);

int check_disk_space(t_task_base *task);

void localfile_link_task(t_task_base *task);

void real_rm_file(char *file);

int get_localfile_stat(t_task_base *base);

time_t get_fcs_dir_lasttime(int d1, int d2);

time_t get_cs_dir_lasttime(int d1, int d2, int domain);

#endif
