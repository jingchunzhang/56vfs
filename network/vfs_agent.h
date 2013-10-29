/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _VFS_AGENT_H_
#define _VFS_AGENT_H_

int init_vfs_agent();
typedef struct {
	unsigned int mtime;
	char file[256];
}fileinfo;

#define BUFF_DATA 0x01  /*发送缓冲区有数据*/
#define FILE_DATA 0x02  /*发送缓冲区有文件数据*/

#define SOCK_CLOSE 0x10 /* peer close or active close */
#define SOCK_SEND  0x20 /* more data need send at next loop */
#define SOCK_SEND_FILE  0x40 /* more file data need send at next loop */
#define SOCK_COMP  0x80 /*socker data send over */

#endif
