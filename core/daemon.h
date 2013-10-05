/*
* Copyright (C) 2012-2014 jingchun.zhang email: jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
* Please visit the github https://github.com/jingchunzhang/56vfs for more detail.
*/

#ifndef _DAEMON_H_
#define _DAEMON_H_

volatile extern int stop;		//1-服务器停止，0-服务器运行中
volatile extern int restart;	//1-服务器异常退出后要自动重启，0-不自动重启
extern int daemon_start(int, char **);
extern void daemon_stop();
extern void daemon_set_title(const char *title);
#endif
