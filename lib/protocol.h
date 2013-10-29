/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _56_VFS_PROTOCOL_H_
#define _56_VFS_PROTOCOL_H_

#include <stdint.h>

#define SIG_HEADSIZE 4
#define MAX_SIG_BODY 4096
extern const char *str_cmd[];
enum {E_PACKET_ERR_CLOSE = -100, E_PRO_OK = 0, E_PRO_HEAD_LEN, E_PRO_TOTAL_LEN, E_DATA_LEN};

typedef struct {
	uint16_t bodylen;
	uint8_t cmdid;
	uint8_t status;
}t_vfs_sig_head;

typedef struct {
	char body[MAX_SIG_BODY];
}t_vfs_sig_body;

/*command id define 4 signalling*/

#define HEARTBEAT_REQ 0x01
#define HEARTBEAT_RSP 0x02

#define ADDONE_REQ 0x03
#define ADDONE_RSP 0x04

#define NEWTRACKER4CS_REQ 0x05
#define NEWTRACKER4CS_RSP 0x06

#define TRACKERLIST4CS_REQ 0x07
#define TRACKERLIST4CS_RSP 0x08

#define SYNCSIGNALLING_REQ 0x09
#define SYNCSIGNALLING_RSP 0x0A

#define TASKINFO_REQ 0x0B
#define TASKINFO_RSP 0x0C

/*stauts 4 signalling */
/* T:Tracker;
 * F:FCS;
 * C:CS;
 */
#define HB_T_2_F 0x01
#define HB_T_2_T 0x02
#define HB_C_2_T 0x03
#define HB_C_2_C 0x04
#define HB_C_2_F 0x05

/*
 * A:add new
 */
#define T_A_2_F 0x01
#define T_A_2_T 0x02
#define C_A_2_T 0x03
#define C_A_2_C 0x04
#define C_A_2_F 0x05

#define F_A_2_C 0x06   //reback sync
/*
 *task info real time
 */
#define TASKINFO_C_2_T 0x01
#define TASKINFO_T_2_C 0x02
#define TASKINFO_T_2_F 0x03
#define TASKINFO_F_2_T 0x04


/*
 *the follow 4 NEW TASK DETAIL PROTOCOL
 */

//cmdid
#define NEWTASK_REQ 0x0D
#define NEWTASK_RSP 0x0E

#define SYNC_DIR_REQ 0x0F
#define SYNC_DIR_RSP 0x10

#define SYNC_DEL_REQ 0x13
#define SYNC_DEL_RSP 0x14

//status fcs->tracker
#define TASK_DISPATCH 0x01

//status tracker->cs
#define TASK_START 0x02

//status cs->cs
#define TASK_SYNC 0x03

//return status cs->cs
//#define TASK_WAIT_SYNC 0x04

#define TASK_WAIT_OK 0x05

#define TASK_WAIT_ERR 0x06

//return status cs->tracker tracker->fcs
#define TASK_SUCCESS 0x07
#define TASK_FAILED 0x08

#define TASK_SYNC_DIR 0x09


/*
 * the follow 4 data sync
 */

/*
 * 为了简单化数据链路的报文格式，定长报文头和报文体
 */

#define CMD_GET_FILE_REQ 0x11
#define CMD_GET_FILE_RSP 0x12

#define FILE_SYNC_SRC_2_DST 0x01
#define FILE_SYNC_DST_2_SRC 0x81
#define FILE_SYNC_SRC_2_DST_ERR 0x02
#define FILE_SYNC_DST_2_SRC_ERR 0x82

#define FILE_SYNC_DST_2_SRC_E_SYNCING 0x83  /*该链路正在同步其它数据 */
#define FILE_SYNC_DST_2_SRC_E_EXIST 0x84 /*本地文件已存在，且校验通过 */
#define FILE_SYNC_DST_2_SRC_E_DISK 0x85 /*磁盘坏或者空间不够 */
#define FILE_SYNC_DST_2_SRC_E_OPENFILE 0x86 /*打开本地文件错误*/
#define FILE_SYNC_DST_2_SRC_E_MALLOC 0x87 /*分配内存错误*/

#ifdef __cplusplus
extern "C"
{
#endif

	/* parse_sig_msg:解析信息消息
	 * h:解析出的消息头
	 * b:解析出的消息体
	 * s:消息源
	 * slen:消息源的长度
	 * ret = 0,ok, other err
	 */
	int parse_sig_msg(t_vfs_sig_head *h, t_vfs_sig_body *b, char *s, int slen);

	/*create_sig_msg：组装信令消息
	 * cmdid:命令字
	 * status:状态
	 * b:消息体
	 * o:组装后的消息
	 *ret > 0 outlen, <= 0 err
	 */
	int create_sig_msg(uint8_t cmdid, uint8_t status, t_vfs_sig_body *b, char *o, uint16_t bodylen);

#ifdef __cplusplus
}
#endif


#endif
