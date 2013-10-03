#ifndef _HTTP_ACL_H_
#define _HTTP_ACL_H_
/*
 * referer检查
 * referer				referer字符串，比如 "http://www.qq.com/index.htm"
 * referer_list			referer白名单字符串，比如 "qq.com paipai.com tencent.com soso.com qzone.net"
 * allow_null_referer	0-允许空referer，1-不允许空referer
 * return				0-检查通过，1-拒绝
 */

#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#define bool int
#define true 1
#define false 0

struct matchitem
{
	char*				val;
	char				flag;
	struct matchitem*	next;
};

extern int check_referer(const char* referer, const char* referer_list, int allow_null_referer);
extern int check_path( char* path );
extern int partnership( char* currval, char* prefix, char* suffix, char* pattern );
extern struct matchitem* getmatch( char* str );
extern int strmatch( struct matchitem* item, const char* dest );
extern char* strtrim( char* str );
extern char* strpcpy( char* dest, const char* src );
extern void splitstr( char* str, char* pstr, char** val, unsigned short count );
extern void getval( char* str, char* pstr, char* end, char** val, unsigned short count );
extern char* getvalue( char** val, unsigned short count, char* str, short len );
extern char* decode_url( char* url );
extern void decodeurl( const char *url, char *filename, int len );
#endif

