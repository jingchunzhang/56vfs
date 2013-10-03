#ifndef _DAEMON_H_
#define _DAEMON_H_

volatile extern int stop;		//1-服务器停止，0-服务器运行中
volatile extern int restart;	//1-服务器异常退出后要自动重启，0-不自动重启
extern int daemon_start(int, char **);
extern void daemon_stop();
extern void daemon_set_title(const char *title);
#endif
