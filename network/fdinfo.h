#ifndef _FDINFO_H_
#define _FDINFO_H_
#include "global.h"

extern struct conn *acon ;
extern int maxfds; 
extern int init_fdinfo(void);		//初始化全局使用的fd相关资源
extern void fini_fdinfo(void);		//释放fd使用的相关资源
#endif
