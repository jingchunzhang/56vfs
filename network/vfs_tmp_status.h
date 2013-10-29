/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

/*
 *该文件实现vfs系统各个角色的临时任务状态管理
 *每条记录由下列字段组成：任务状态 '0' 任务结束 '1' 任务为GET文件 '2' 任务为PUSH文件 定长 1 
 * 空格作为分隔符
 */

#ifndef _VFS_TMP_STATUS_H_
#define _VFS_TMP_STATUS_H_

#include "vfs_task.h"
#include "vfs_localfile.h"

typedef struct {
	off_t pos;
	list_head_t list;
} t_tmp_status;

#define DEFAULT_ITEMS 1024

/*
 * 服务启动时 装载临时状态文件到内存，在生成任务之前，检查本地是否已经有对应的目标文件和临时文件（断点续传）
 *
 */
int init_load_tmp_status();

/*
 *新任务 从临时文件获取一个空位
 *返回对应的空位位置 相关信息已经置位
 */
int set_task_to_tmp(t_vfs_tasklist *tasklist);


/*
 *置空对应的位置信息
 */
void set_tmp_blank(off_t pos, t_tmp_status *tmp);

#endif
