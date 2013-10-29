/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "common.h"
#include "util.h"
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h> 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int get_ip_by_domain(char *serverip, char *domain)
{
	char **pptr;
	char                    str[128] = {0x0};
	struct hostent  *hptr;
	if ( (hptr = gethostbyname(domain)) == NULL) 
		return -1;

	switch (hptr->h_addrtype) {
		case AF_INET:
#ifdef  AF_INET6
		case AF_INET6:
#endif
			pptr = hptr->h_addr_list;
			for ( ; *pptr != NULL; pptr++)
			{
				inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str));
				strcpy(serverip, str);
				return 0;
			}
			break;

		default:
			return -1;
			break;
	}
	return -1;
}

void trim_in(char *s, char *d)
{
	/*skip head blank */

	while (s)
	{
		if (*s != ' ')
			break;
		s++;
	}

	int c = 0;
	while (*s)
	{
		if (*s == ' ')
		{
			c++;
			if (c == 1)
			{
				*d = *s;
				d++;
				s++;
				continue;
			}
			s++;
		}
		else
		{
			c = 0;
			*d = *s;
			d++;
			s++;
		}
	}
	return;
}

uint32_t r5hash(const char *p) 
{
	uint32_t h = 0;
	while(*p) {
		h = h * 11 + (*p<<4) + (*p>>4);
		p++;
	}
	return h;
}

int get_strtime(char *buf)
{
    struct tm tmm; 
	time_t now = time(NULL);
	localtime_r(&now, &tmm);  
	sprintf(buf, "%04d%02d%02d%02d%02d%02d", tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);
	return 0;
}

int get_strtime_by_t(char *buf, time_t now)
{
    struct tm tmm; 
	localtime_r(&now, &tmm);  
	sprintf(buf, "%04d%02d%02d%02d%02d%02d", tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);
	return 0;
}

uint32_t get_uint32_ip(char *sip, char *s_ip)
{
	uint32_t ip = str2ip(sip);
	if (ip == INADDR_NONE)
	{
		char tmpip[16] = {0x0};
		if (get_ip_by_domain(tmpip, sip))
			return INADDR_NONE;
		ip = str2ip(tmpip);
		snprintf(s_ip, 16, "%s", tmpip);
	}
	else
		snprintf(s_ip, 16, "%s", sip); 
	return ip;
}

time_t get_time_t (char *p)
{
	if (strlen(p) != 14)
		return 0;
	struct tm t;
	memset(&t, 0, sizeof(t));
	char b[8] = {0x0};
	snprintf(b, sizeof(b), "%.4s", p);
	t.tm_year = atoi(b) - 1900;
	memset(b, 0, sizeof(b));

	snprintf(b, sizeof(b), "%.2s", p+4);
	t.tm_mon = atoi(b) - 1;
	memset(b, 0, sizeof(b));

	snprintf(b, sizeof(b), "%.2s", p+6);
	t.tm_mday = atoi(b);
	memset(b, 0, sizeof(b));

	snprintf(b, sizeof(b), "%.2s", p+8);
	t.tm_hour = atoi(b);
	memset(b, 0, sizeof(b));

	snprintf(b, sizeof(b), "%.2s", p+10);
	t.tm_min = atoi(b);
	memset(b, 0, sizeof(b));

	snprintf(b, sizeof(b), "%.2s", p+12);
	t.tm_sec = atoi(b);
	memset(b, 0, sizeof(b));

	return mktime(&t);
}

#ifdef uchar
#undef uchar
#endif
#define uchar unsigned char

void base64_encode(const char *buf, int len, char *out, int pad)
{
	char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int bit_offset, byte_offset, idx, i;
	const uchar *d = (const uchar *)buf;
	int bytes = (len*8 + 5)/6;

	for (i = 0; i < bytes; i++) {
		byte_offset = (i*6)/8;
		bit_offset = (i*6)%8;
		if (bit_offset < 3) {
			idx = (d[byte_offset] >> (2-bit_offset)) & 0x3F;
		} else {
			idx = (d[byte_offset] << (bit_offset-2)) & 0x3F;
			if (byte_offset+1 < len) {
				idx |= (d[byte_offset+1] >> (8-(bit_offset-2)));
			}
		}
		out[i] = b64[idx];
	}

	while (pad && (i % 4))
		out[i++] = '=';

	out[i] = '\0';
}
