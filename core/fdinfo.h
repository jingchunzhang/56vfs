/*
* Copyright (C) 2012-2014 jingchun.zhang email: jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
* Please visit the github https://github.com/jingchunzhang/56vfs for more detail.
*/

#ifndef _FDINFO_H_
#define _FDINFO_H_
#include "global.h"

extern struct conn *acon ;
extern int maxfds; 
extern int init_fdinfo(void);		//初始化全局使用的fd相关资源
extern void fini_fdinfo(void);		//释放fd使用的相关资源
#endif
