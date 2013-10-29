/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "protocol.h"

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

const char *str_cmd[] = {"ERROR", "HEARTBEAT_REQ", "HEARTBEAT_RSP", "ADDONE_REQ", "ADDONE_RSP", "NEWTRACKER4CS_REQ", "NEWTRACKER4CS_RSP", "TRACKERLIST4CS_REQ", "TRACKERLIST4CS_RSP", "SYNCSIGNALLING_REQ", "SYNCSIGNALLING_RSP", "TASKINFO_REQ", "TASKINFO_RSP", "NEWTASK_REQ", "NEWTASK_RSP", "CMD_PUSH_FILE_REQ", "CMD_PUSH_FILE_RSP", "CMD_GET_FILE_REQ", "CMD_GET_FILE_RSP"};

const char *protocol_err[] = {"ok", "head len err", "total len err", "error data len, close socket!"};

int parse_sig_msg(t_vfs_sig_head *h, t_vfs_sig_body *b, char *s, int slen)
{
	if (slen < SIG_HEADSIZE)
		return E_PRO_HEAD_LEN;
	memcpy(h, s, sizeof(t_vfs_sig_head));
	h->bodylen = ntohs(h->bodylen);
	if (h->bodylen >= MAX_SIG_BODY)
		return E_PACKET_ERR_CLOSE;
	if (slen < SIG_HEADSIZE + h->bodylen)
		return E_PRO_TOTAL_LEN;
	if (slen == SIG_HEADSIZE)
		return 0;
	memcpy(b->body, s + SIG_HEADSIZE, h->bodylen);
	return 0;
}

int create_sig_msg(uint8_t cmdid, uint8_t status, t_vfs_sig_body *b, char *o, uint16_t bodylen)
{
	t_vfs_sig_head nh;
	nh.bodylen = htons(bodylen);
	nh.cmdid = cmdid;
	nh.status = status;

	char *p = o;
	memcpy(p, &nh, sizeof(nh));
	p += sizeof(nh);
	if (bodylen == 0)
		return sizeof(nh);

	memcpy(p, b->body, bodylen);
	p += bodylen;

	return p - o;
}

