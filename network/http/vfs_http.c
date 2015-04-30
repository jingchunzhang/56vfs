/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
#include "acl.h"

int vfs_http_log = -1;

enum {GET = 0, HEAD};

typedef struct {
	char url[1024];
	uint8_t type;
	uint8_t range;
	off_t s;
	off_t e;
} t_http_peer;

int svc_pinit()
{
	return 0;
}

int svc_init() 
{
	char *logname = myconfig_get_value("log_data_logname");
	if (!logname)
		logname = "./http_log.log";

	char *cloglevel = myconfig_get_value("log_data_loglevel");
	int loglevel = LOG_NORMAL;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_data_logsize", 100);
	int logintval = myconfig_get_intval("log_data_logtime", 3600);
	int lognum = myconfig_get_intval("log_data_lognum", 10);
	vfs_http_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_http_log < 0)
		return -1;
	LOG(vfs_http_log, LOG_DEBUG, "svc_init init log ok!\n");
	return 0;
}

int svc_initconn(int fd) 
{
	LOG(vfs_http_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(t_http_peer));
	if (curcon->user == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "malloc err %m\n");
		return RET_CLOSE_MALLOC;
	}
	memset(curcon->user, 0, sizeof(t_http_peer));
	LOG(vfs_http_log, LOG_DEBUG, "a new fd[%d] init ok!\n", fd);
	return 0;
}

static void get_range(char *q, t_http_peer *peer)
{
	char *s = strstr(q, "Range: bytes=");
	if (s == NULL)
		return;
	s += strlen("Range: bytes=");

	peer->range = 1;
	peer->s = atol(s);
	char *e = strchr(s, '-');
	if (e == NULL)
		return;
	peer->e = atol(e+1);
	if (peer->e < peer->s)
		peer->e = peer->s;
}

static int check_request(int fd, char* data, int len) 
{
	if(len < 14)
		return 0;
	struct conn *c = &acon[fd];
	t_http_peer *peer = (t_http_peer *) c->user;
		
	LOG(vfs_http_log, LOG_NORMAL, "%.*s", len, data); 
	if(!strncmp(data, "GET /", 5)) {
		char* p;
		if((p = strstr(data + 5, "\r\n\r\n")) != NULL) {
			char* q;
			int len;
			if((q = strstr(data + 5, " HTTP/")) != NULL) {
				len = q - data - 5;
				get_range(q, peer);
				if(len < 1023) {
					strncpy(peer->url, data + 5, len);	
					peer->type = GET;
					return p - data + 4;
				}
			}
			return -2;	
		}
		else
			return 0;
	}
	else if(!strncmp(data, "HEAD /", 6)) {
		char* p;
		if((p = strstr(data + 6, "\r\n\r\n")) != NULL) {
			char* q;
			int len;
			if((q = strstr(data + 6, " HTTP/")) != NULL) {
				len = q - data - 6;
				if(len < 1023) {
					strncpy(peer->url, data + 6, len);	
					peer->type = HEAD;
					return p - data + 4;
				}
			}
			return -2;	
		}
		else
			return 0;
	}
	else
		return -1;
}

static int handle_request(int cfd) 
{
	
	char httpheader[256] = {0};
	char filename[128] = {0};
	int fd;
	struct stat st;
	
	struct conn *c = &acon[cfd];
	t_http_peer *peer = (t_http_peer *) c->user;
	sprintf(filename, "./%s", peer->url);
	LOG(vfs_http_log, LOG_NORMAL, "file = %s\n", filename);
	
	fd = open(filename, O_RDONLY);
	if(fd > 0) {
		fstat(fd, &st);
		sprintf(httpheader, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %u\r\n\r\n", (unsigned)st.st_size);
		
	}
	else {
		sprintf(httpheader, "HTTP/1.1 403 File Not Found\r\n\r\n");	
	}
	//c->close_imme = 1; //发送完回复包后主动关闭连接	
	set_client_data(cfd, httpheader, strlen(httpheader));
	if(fd > 0)
	{
		if (peer->type == GET)
		{
			if (peer->range)
			{
				off_t reqend = peer->e > (st.st_size - 1) ? (st.st_size - 1) : peer->e;
				off_t len = reqend - peer->s + 1;
				set_client_fd(cfd, fd, peer->s, len); 
			}
			else
				set_client_fd(cfd, fd, 0, st.st_size); 
		}
		else
			close(fd);
	}
	return 0;
}

static int check_req(int fd)
{
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] no data!\n", fd);
		return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
	}
	int clen = check_request(fd, data, datalen);
	if (clen < 0)
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data error ,not http!\n", fd);
		return RECV_CLOSE;
	}
	if (clen == 0)
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data not suffic!\n", fd);
		return RECV_ADD_EPOLLIN;
	}
	handle_request(fd);
	consume_client_data(fd, clen);
	return RECV_SEND;
}

int svc_recv(int fd) 
{
	return check_req(fd);
}

int svc_send(int fd)
{
	return SEND_ADD_EPOLLIN;
}

void svc_timeout()
{
}

void svc_finiconn(int fd)
{
	LOG(vfs_http_log, LOG_DEBUG, "close %d\n", fd);
}
