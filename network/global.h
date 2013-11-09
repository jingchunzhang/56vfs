/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _GLOBAL_H_
#define _GLOBAL_H_
#include <fcntl.h>
#include <sys/poll.h>
#include "atomic.h"
#include <stdint.h>
#include "list.h"
#include "log.h"
#include "myconfig.h"

#define ID __FILE__
#define LN __LINE__
#define FUNC __FUNCTION__

extern struct conn *acon ;
extern int glogfd;

//数据缓冲区
struct mybuff {
	char* data;			//buffer指针
	size_t size;		//buffer大小
	size_t len;		//数据有效长度
	int fd;
	off_t foffset;
	size_t flen;
};

//连接对象
struct conn {
	char peerip[16];    //for debug print
	int fd;      //recheck avoid coredump
	struct mybuff send_buff;	//send buffer for client
	struct mybuff recv_buff;	//recv buffer for client
	int send_len;            //send len between call svc_send
	void* user;				//user custom data
};

#define RECV_CLOSE 0x01   //do_recv need to close socket
#define RECV_ADD_EPOLLIN 0x02  //do_recv need to add fd EPOLLIN
#define RECV_ADD_EPOLLOUT 0x04 //do_recv need to add fd EPOLLOUT
#define RECV_ADD_EPOLLALL 0x06  //do_recv need to add fd EPOLLOUT and EPOLLIN
#define RECV_SEND 0x08  //do_recv need to send at once 

#define SEND_CLOSE 0x10 //do_send need to close socket
#define SEND_ADD_EPOLLIN 0x20 //do_send need to add fd EPOLLIN
#define SEND_ADD_EPOLLOUT 0x40 //do_send need to add fd EPOLLOUT
#define SEND_ADD_EPOLLALL 0x80 //do_send need to add fd EPOLLOUT and EPOLLIN

#define RET_OK 300
#define RET_SUCCESS 301
#define RET_CLOSE_HB 302  //4 detect hb
#define RET_CLOSE_MALLOC 303  //4 malloc err
#define RET_CLOSE_DUP 304  //dup connect

#define MAXSIG 20480
#define MAXCS_ONEGRUOP 1024
#define MAXDIR_FOR_CS 1024

#define MAXTRACKER 32
#define MAXFCS 1024

#define DIR1 30
#define DIR2 30

enum ROLE {UNKOWN = 0, ROLE_FCS , ROLE_CS, ROLE_TRACKER, ROLE_VOSS_MASTER, SELF_IP, ROLE_MAX};

enum ISP {TEL = 0, CNC, EDU, TT, YD, HS, MP4, ALL = 50, ALLHOT, ISP_TRACKER, ISP_FCS, ISP_VOSS, UNKNOW_ISP=254, MAXISP};

enum MODE {CON_PASSIVE = 0, CON_ACTIVE};

enum SERVER_STAT {UNKOWN_STAT = 0, WAIT_SYNC, SYNCING, ON_LINE, OFF_LINE, STAT_MAX};

uint8_t self_stat ;

time_t self_offline_time;
#endif
