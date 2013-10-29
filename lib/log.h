/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _LOG_H_
#define _LOG_H_
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#define nowtime() time(0)

enum loglevel {LOG_TRACE = 0, LOG_DEBUG, LOG_NORMAL, LOG_ERROR, LOG_FAULT, LOG_NONE}; 

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * 注册日志
 * le		日志配置相关参数
 * return	>0返回日志fd，否则失败
 */
int registerlog(const char* filename, int level, int rotatesize, int rotateintval, int filenum);
int getloglevel(const char* llstr) ;
void logclose(int fd);
/*
 * 打印日志
 * fd		日志文件fd
 * level	日志级别
 * fmt		格式字符串
 */
void LOG(int fd, int level, const char* fmt, ...); 

int init_log();
#ifdef __cplusplus
}
#endif
#endif
