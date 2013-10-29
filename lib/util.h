/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>
extern int str_explode(const char *ifs, char *line0, char *field[], int n);
#ifdef __cplusplus
extern "C"
{
#endif
extern uint32_t str2ip(const char *);
extern char *ip2str(char *, uint32_t);
#ifdef __cplusplus
}
#endif
extern char *ll2str(char *, int64_t);
extern int decode_hex(const char* hex, char* bin, int buflen);
extern int encode_hex(char* hex, const char* bin, int binlen);
//取机器的cpu个（核）数
extern int getcpunum();
//计算md5
extern void getmd5(const char* data, unsigned len, unsigned char* md5);
//计算文件md5
extern int getfilemd5(const char* filename, unsigned char* md5);
//计算文件可视md5
extern int getfilemd5view(const char* filename, unsigned char* md5);
//根据sock fd取对端ip地址
extern uint32_t getpeerip(int fd);
//根据网络设备名取ip地址
extern uint32_t getipbyif(const char* ifname);
__attribute__((unused))
static inline char *uint2str(char *p, uint32_t n) {
	int c;
	if(n<10) { c = 1; goto len1; }
	if(n<100) { c = 2; goto len2; }
	if(n<1000) { c = 3; goto len3; }
	if(n<10000) { c = 4; goto len4; }
	if(n<100000) { c = 5; goto len5; }
	if(n<1000000) { c = 6; goto len6; }
	if(n<10000000) { c = 7; goto len7; }
	if(n<100000000) { c = 8; goto len8; }
	if(n<1000000000) { c = 9; goto len9; }
		c = 10;
	p[9] = 0x30 | (n%10); n/=10;
len9:	p[8] = 0x30 | (n%10); n/=10;
len8:	p[7] = 0x30 | (n%10); n/=10;
len7:	p[6] = 0x30 | (n%10); n/=10;
len6:	p[5] = 0x30 | (n%10); n/=10;
len5:	p[4] = 0x30 | (n%10); n/=10;
len4:	p[3] = 0x30 | (n%10); n/=10;
len3:	p[2] = 0x30 | (n%10); n/=10;
len2:	p[1] = 0x30 | (n%10); n/=10;
len1:	p[0] = 0x30 | n;
	return p+c;
}

__attribute__((unused))
static inline char *int2str(char *p, int32_t n) {
	int c;
	if(n<0) { *p++ = '-'; n=-n; }
	if(n<10) { c = 1; goto len1; }
	if(n<100) { c = 2; goto len2; }
	if(n<1000) { c = 3; goto len3; }
	if(n<10000) { c = 4; goto len4; }
	if(n<100000) { c = 5; goto len5; }
	if(n<1000000) { c = 6; goto len6; }
	if(n<10000000) { c = 7; goto len7; }
	if(n<100000000) { c = 8; goto len8; }
	if(n<1000000000) { c = 9; goto len9; }
		c = 10;
	p[9] = 0x30 | (n%10); n/=10;
len9:	p[8] = 0x30 | (n%10); n/=10;
len8:	p[7] = 0x30 | (n%10); n/=10;
len7:	p[6] = 0x30 | (n%10); n/=10;
len6:	p[5] = 0x30 | (n%10); n/=10;
len5:	p[4] = 0x30 | (n%10); n/=10;
len4:	p[3] = 0x30 | (n%10); n/=10;
len3:	p[2] = 0x30 | (n%10); n/=10;
len2:	p[1] = 0x30 | (n%10); n/=10;
len1:	p[0] = 0x30 | n;
	return p+c;
}
#endif
