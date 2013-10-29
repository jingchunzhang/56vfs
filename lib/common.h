/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _NM_COMMON_H_
#define _NM_COMMON_H_
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>

#define ID __FILE__
#define FUNC __FUNCTION__
#define LN __LINE__

#define vfs_abs(value)       (((value) >= 0) ? (value) : - (value))
#define vfs_max(val1, val2)  (((val1) < (val2)) ? (val2) : (val1))
#define vfs_min(val1, val2)  (((val1) > (val2)) ? (val2) : (val1))

typedef struct {
	char indir[256];
	char bkdir[256];
	char workdir[256];
	char outdir[256];
	char fulldir[256];
	char tmpdir[256];
}t_path_info;

typedef struct {
	char host[512];
	char username[32];
	char passwd[32];
	char db[32];
	int port;
}t_db_info;

#ifdef __cplusplus
extern "C"
{
#endif
int get_ip_by_domain(char *serverip, char *domain);

void trim_in(char *s, char *d);

uint32_t r5hash(const char *p); 

int get_strtime(char *buf);

int get_strtime_by_t(char *buf, time_t now);

uint32_t get_uint32_ip(char *sip, char *s_ip);

time_t get_time_t (char *p);

void base64_encode(const char *buf, int len, char *out, int pad);

#ifdef __cplusplus
}
#endif
#endif
