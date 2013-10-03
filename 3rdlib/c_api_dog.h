#ifndef __NM_C_API_DOG_H_
#define __NM_C_API_DOG_H_
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>
#include <stddef.h>

typedef struct
{
	int flag;  //flag = 0, pid 不存在 不会重启，flag = 1 不管pid是否存在，都会重启
	int intval;  //超过设置时间，dog会杀死重启
	time_t last;  //最近更新时间戳
	char cmd[256];  //绝对路径命令
	char argv[10][64];  //启动参数
}T_WATCH_DOG;

#ifdef __cplusplus
extern "C"
{
#endif
	int reg_process(pid_t pid, T_WATCH_DOG *dog);
	int update_process(pid_t pid, T_WATCH_DOG *dog);
	int touch_process();
#ifdef __cplusplus
}
#endif
#endif
