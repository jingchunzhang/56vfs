/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

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
#include <arpa/inet.h>
#include "pro_voss.h"

unsigned int getseq()
{
	static unsigned int seq = 0x00000001;
	return seq++;
}

int create_msg(char *outbuf, int *outlen, unsigned int cmdid, char *inbuf, int inlen)
{
	unsigned int seq = getseq();

	unsigned int totallen = HEADSIZE + inlen;
	unsigned int ntotallen = htonl(totallen);
	unsigned int ncmdid = htonl(cmdid);
	unsigned int nseq = htonl(seq);

	char *p = outbuf;
	memcpy(p, &ntotallen, sizeof(ntotallen));
	p +=  sizeof(ntotallen);

	memcpy(p, &ncmdid, sizeof(ncmdid));
	p +=  sizeof(ncmdid);

	memcpy(p, &nseq, sizeof(nseq));
	p +=  sizeof(nseq);
	*outlen = HEADSIZE;
	if (inlen == 0)
		return 0;

	memcpy(p, inbuf, inlen);
	*outlen = inlen + HEADSIZE;

	return 0;
}

int parse_msg(char *inbuf, int inlen, t_head_info *head)
{
	t_head_info nhead;
	if (inlen < HEADSIZE)
		return E_NOT_SUFFIC;
	memcpy(&nhead, inbuf, HEADSIZE);
	head->totallen = ntohl(nhead.totallen);
	head->cmdid = ntohl(nhead.cmdid);
	head->seq = ntohl(nhead.seq);

	return 0;
}

int create_voss_head(char *outbuf, unsigned int cmdid, int datalen)
{
	unsigned int seq = getseq();

	unsigned int totallen = HEADSIZE + datalen;
	unsigned int ntotallen = htonl(totallen);
	unsigned int ncmdid = htonl(cmdid);
	unsigned int nseq = htonl(seq);

	char *p = outbuf;
	memcpy(p, &ntotallen, sizeof(ntotallen));
	p +=  sizeof(ntotallen);

	memcpy(p, &ncmdid, sizeof(ncmdid));
	p +=  sizeof(ncmdid);

	memcpy(p, &nseq, sizeof(nseq));
	p +=  sizeof(nseq);
	return 0;
}
