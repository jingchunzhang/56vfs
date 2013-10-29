/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _MYBUFF_H_
#define _MYBUFF_H_
#include <stdint.h>
#include "global.h"
#include "vfs_init.h"


//初始缓冲区大小
extern int init_buff_size;
/*
 * 初始化
 * mybuff		待初始化的buff
 */
extern void mybuff_init(struct mybuff* mybuff);
/*
 * 写入数据
 * mybuff		目标buff
 * data			数据指针
 * len			数据长度
 * return		0-成功，否则失败
 */
extern int mybuff_setdata(struct mybuff* mybuff, const char* data, size_t len);
/* 
 * 写入文件信息
 * mybuff		目标buff
 * fd			文件fd
 * offset		偏移量
 * len			发送长度
 * return		0-成功，否则失败
 */
extern int mybuff_setfile(struct mybuff* mybuff, int fd, off_t offset, size_t len);
/*
 * 取数据
 * mybuff		源buff
 * data			缓冲区指针
 * len			数据长度指针
 * return		0-成功，否则没数据了
 */
extern int mybuff_getdata(struct mybuff* mybuff, char** data, size_t* len);
/*
 * 设置使用数据量
 * mybuff		源buff
 * len			使用长度
 */
extern void mybuff_skipdata(struct mybuff* mybuff, size_t en);
/*
 * 取文件信息
 * mybuff		源buff
 * fd			文件fd
 * offset		偏移量
 * len			发送长度
 * return		0-成功，否则没数据了
 */
extern int mybuff_getfile(struct mybuff* mybuff, int* fd, off_t* offset, size_t * len);
/*
 * 设置使用文件数据量
 * mybuff		源buff
 * len			数据长度
 */
extern void mybuff_skipfile(struct mybuff* mybuff, size_t len);
/*
 * 重新初始化
 * mybuff		目标buff
 */
extern void mybuff_reinit(struct mybuff* mybuff);
/*
 * 释放资源
 * mybuff		目标buff
 */
extern void mybuff_fini(struct mybuff* mybuff);

#endif
