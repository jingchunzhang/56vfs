/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_

#define VOSSPREFIX "voss_"
#define VOSSSUFFIX ".master"

#define CMD_MASK 0x80000000

#define REQ_AUTH 0x00000001 //连接授权请求
#define RSP_AUTH 0x80000001 //连接授权应答

#define REQ_SUBMIT 0x00000002 //提交末端机信息请求
#define RSP_SUBMIT 0x80000002 //提交末端机信息应答

#define REQ_CONF_UPDATE 0x00000003   //更新vfs配置项请求
#define RSP_CONF_UPDATE 0x80000003   //更新VFS配置项应答

#define REQ_SYNC_DIR 0x00000004   //FCS接受来自VOSS的目录同步请求 该请求为异步请求
#define RSP_SYNC_DIR 0x80000004   //FCS接受来自VOSS的目录同步应答

#define REQ_SYNC_FILE 0x00000006 //TRACKER 接受来自VOSS的 小运营商文件同步请求
#define RSP_SYNC_FILE 0x80000006 //TRACKER 接受来自VOSS的 小运营商文件同步应答

#define REQ_HEARTBEAT 0x00000005
#define RSP_HEARTBEAT 0x80000005

#define REQ_VFS_CMD 0x00000008  //VFS接受来自VOSS请求
#define RSP_VFS_CMD 0x80000008  //VFS应答给VOSS的报文

#define REQ_STOPVFS 0x00000009 //VFS接受来自VOSS的停止请求
#define RSP_STOPVFS 0x80000009

#define HEADSIZE 12
#define MAXBODYSIZE 20480

enum LINKSTAT {CONNETED = 0, LOGON, RUN, START, EXE_TASK, END, LINKSEND, LINKTEST};

enum RETCODE {OK = 0, E_NOT_SUFFIC, E_TOO_LONG};  /*too long need close , reset socket*/

typedef struct {
	char buf[MAXBODYSIZE];
}t_body_info;

typedef struct {
	unsigned int totallen;
	unsigned int cmdid;
	unsigned int seq;
}t_head_info;

/* create_msg:构造请求消息
 *outbuf:输出缓冲区，存放构造好的消息
 *outlen:输出缓冲区长度
 *cmdid:命令字
 *inbuf:body的缓冲区
 *inlen:body的长度
 *返回值0:OK  -1:ERROR
 */

int create_msg(char *outbuf, int *outlen, unsigned int cmdid, char *inbuf, int inlen);

/*parse_msg:解析消息
 *inbuf:输入缓冲区
 *inlen:输入缓冲区长度
 *head:解析出的报文head
 *返回值0:OK  other:ERROR
 */

int parse_msg(char *inbuf, int inlen, t_head_info *head);

int create_voss_head(char *outbuf, unsigned int cmdid, int datalen);
#endif
