/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <stdio.h>
#include <string.h>
#include "acl.h"


int check_referer(const char* referer, const char* referer_list, int allow_null_referer) {

	char* ref = NULL;
	if(referer == NULL || (ref = strchr(referer, ':')) == NULL) {
		if(allow_null_referer)
			return 0;
		else
			return -1;
	}

	ref++;
	while(*ref == '/')
		ref++;

	char* p;
	char* dots_pos[10] = {0};   //域名点的位置
	int dots_num = 0;           //域名点的个数
	for(p = ref; *p; p++) {
		if(*p == '/' || *p == ':')
			break;
		else if(*p == '.') {
			dots_pos[dots_num++] = p;
			if(dots_num >= 10)
				return -1;
		}
	}

	//最后一个点不算,此时p指向域名的最后一个字符
	if(*(--p) == '.')
		dots_pos[dots_num--] = 0;

	//小于一个点的域名无效
	if(dots_num < 1)
		return -1;

	int suffix_len = p - dots_pos[dots_num - 1];
	char key[128] = {0};
	int key_len = 0;
	char* q;
	bool retry = true;

	//域名后缀长度为3，比如com,net,org等等
	if(suffix_len == 3) {

		if(dots_num >= 2) { //取域名后两段

			key_len = p - dots_pos[dots_num - 2];
			if(key_len >= 128)
				key_len = 127;
			strncpy(key, dots_pos[dots_num - 2] + 1, key_len);
			key[key_len] = '\0';
		}
		else {              //取整个域名
			retry = false;
			key_len = p - ref + 1;
			if(key_len >= 128)
				key_len = 127;
			strncpy(key, ref, key_len);
			key[key_len] = '\0';
		}
match_tag:		
		q = strcasestr(referer_list, key);
		//此处要避免部分匹配的情况，比如 so.so.com, 取两段是so.com，可能匹配了soso.com
		if(q && (q == referer_list || q[-1] == ' '))
			return 0;
		else if(retry) {
			if(dots_num == 2) {		//尝试整个域名
				key_len = p - ref + 1;
				if(key_len >= 128)
					key_len = 127;
				strncpy(key, ref, key_len);
				key[key_len] = '\0';	
			}
			else {					//尝试域名后三段
				key_len = p - dots_pos[dots_num - 3];
				if(key_len >= 128)
					key_len = 127;
				strncpy(key, dots_pos[dots_num - 3] + 1, key_len);
				key[key_len] = '\0';
			}
			retry = false;
			goto match_tag;
		}
		else
			return -1;
	}
	//域名后缀长度为2，比如cn,hk等等
	else if(suffix_len == 2) {

		if(dots_num <= 2) {     //取整个域名

			key_len = p - ref + 1;
			if(key_len >= 128)
				key_len = 127;
			strncpy(key, ref, key_len);
			key[key_len] = '\0';
		}
		else {                  //取三段

			key_len = p - dots_pos[dots_num - 3];
			if(key_len >= 128)
				key_len = 127;
			strncpy(key, dots_pos[dots_num - 3] + 1, key_len);
			key[key_len] = '\0';
		}

		if(strcasestr(referer_list, key))
			return 0;
		else if(dots_num >= 2) {   //匹配失败，取两段尝试

			key_len = p - dots_pos[dots_num - 2];
			if(key_len >= 128)
				key_len = 127;
			strncpy(key, dots_pos[dots_num - 2] + 1, key_len);
			key[key_len] = '\0';
			q = strcasestr(referer_list, key);
			//此处要避免部分匹配的情况，比如 abc.net.cn, 取两段是net.cn，可能匹配了xyz.net.cn
			if(q && (q == referer_list || q[-1] == ' '))
				return 0;
			else
				return -1;
		}
	}

	return -1;
}


int check_path( char* path )
{
	if( *path != '/' ) return 0;

	/*检查是否在当前目录之下*/
	char* str = path; unsigned short dirs = 0, udirs = 0;
	while( *str != 0x0 && ( udirs == 0 || udirs < dirs ) )
	{
		if( *str == '/' )
		{
			while( *str == '/' ) str++;
			if( *str == '.' && *(str + 1) == '.' && *(str + 2) == '/' )
			{
				udirs++;
				str += 3;
				continue;
			}
			else if( *str == '.' && *(str + 1) == '/' )
			{
				str += 2;
				continue;
			}
			dirs++;
		}
		str++;
	}
	return udirs < dirs;
}

int partnership( char* currval, char* prefix, char* suffix, char* pattern )
{
	int size = strlen( currval ); char* str = NULL;
	if( ( prefix[0] == 0x0 || strncmp( prefix, currval, size ) == 0 ) && \
		( suffix[0] == 0x0 || ( ( str = strstr( currval, suffix ) ) != NULL && *( str + size ) == 0x0 ) ) && \
		( pattern[0] == 0x0 || strstr( currval, pattern ) != NULL ) ) return 1;
	return 0;
}

struct matchitem* getmatch( char* str )
{
	char* val[1024] = {NULL};
	splitstr( str, "*", val, sizeof( val ) / sizeof( char* ) );
	int i = 0, m = 0; struct matchitem* item = NULL, *last = NULL;
	for( i = 0; i < sizeof( val ) / sizeof( char* ) && val[i] != NULL; i++ )
	{
		char* vals[1024] = {NULL};
		splitstr( val[i], "?", vals, sizeof( vals ) / sizeof( char* ) );
		for( m = 0; m < sizeof( val ) / sizeof( char* ) && vals[m] != NULL; m++ )
		{
			struct matchitem* match = (struct matchitem*)malloc( sizeof( struct matchitem ) );
			if( match == NULL ) return NULL;
			match->flag = m > 0 ? 0x02 : ( i == 0 ? 0x00 : 0x01 );
			match->val = vals[m]; match->next = NULL;
			if( last != NULL ) last->next = match;
			if( item == NULL ) item = match;
			last = match;
		}
	}
	return item;
}

int strmatch( struct matchitem* item, const char* dest )
{
	const char* pmstr = dest;
	while( item != NULL )
	{
		if( item->flag == 0x00 )
		{
			if( *item->val == 0x0 && item->next == NULL ) return 1;
			if( strncmp( item->val, pmstr, strlen( item->val ) ) != 0 ) return 0;
			pmstr += strlen( item->val );
		}
		else if( item->flag == 0x01 )
		{
			const char* str = pmstr;
			while( str != NULL && *str != 0x0 )
			{
				str = strstr( str, item->val );
				if( str == NULL ) return 0;
				if( *item->val == 0x0 && item->next == NULL ) return 1;
				if( strmatch( item->next, str + strlen( item->val ) ) ) return 1;
				str++;
			}
		}
		else if( item->flag == 0x02 )
		{
			pmstr++;
			if( *item->val != 0x0 && strncmp( item->val, pmstr, strlen( item->val ) ) != 0 ) return 0;
			pmstr += strlen( item->val );
		}
		item = item->next;
	}
	return *pmstr == 0x0 && item == NULL;
}

char* strtrim( char* str )
{
	if( str == NULL )
		return NULL;

	while( *str != 0x0 && *str == ' ') str++;
	char* val = str + strlen( str ) - 1;
	while( val > str && *val != 0x0 && *val == ' ' ) *val-- = 0x0;
	return str;
}

char* strpcpy( char* dest, const char* src )
{
	while( ( *dest = *src++ ) != 0x0 )
		dest++;

	return dest;
}

void splitstr( char* str, char* pstr, char** val, unsigned short count )
{
	int i = 0; char* pnext = NULL;
	for( i = 0; i < count; i++ )
	{
		val[i] = str;
		pnext = strstr( str, pstr );
		if( pnext == NULL ) return;
		*pnext = 0x0;
		str = pnext + strlen( pstr );
	}
}

void getval( char* str, char* pstr, char* end, char** val, unsigned short count )
{
	char* pnext = NULL; unsigned short i = 0;
	for( i = 0; i < count && str != NULL; i++ )
	{
		val[i] = strtrim( strtok_r( str, pstr, &pnext ) );
		if( val[i] == NULL || ( end != NULL && strcmp( val[i], end ) == 0 ) ) break;
		str = pnext;
	}
}

char* getvalue( char** val, unsigned short count, char* str, short len )
{
	unsigned short i = 0;
	for( i = 0; i < count && val[i] != NULL; i++ )
	{
		if( strncasecmp( val[i], str, len ) == 0 )
			return val[i] + len;
	}
	return NULL;
}


/*-----------------------------
注意，本函数只能转义成<= +1之前字符串，否则可能core
------------------------------*/

const char* decodeurlstring[][2] = { { "20", " " }, { "21", "!" } };
char* decode_url( char* url )
{
	char* purl = url;
	while( *url != 0x0 )
	{
		if( *url == '%' )
		{
			int i = 0; url++;
			for( i = 0; i < sizeof( decodeurlstring ) / ( sizeof( const char* ) * 2 ); i++ )
			{
				size_t len = strlen( decodeurlstring[i][0] );
				if( strncmp( decodeurlstring[i][0], url, len ) == 0 )
				{
					size_t nlen = strlen( decodeurlstring[i][1] );
					if( nlen > 0 ) memcpy( url - 1, decodeurlstring[i][1], nlen );
					if( nlen != len + 1 ) strcpy( url + nlen - 1, url + len );
					url += nlen - 1; break;
				}
			}
		}
		else url++;
	}
	return purl;
}

void decodeurl( const char *url, char *filename, int len )
{
	//modify end
	char *p;
	char *e = filename + len;
	int ch;

	//add by stanwang 2008-12-10
	//const char *urlp = url;
	//add end
	/* url[0] and filename[0] always be '/' */
	p = filename;
	*p++ = '/';
	while(p<e && (ch=*url++)) {
//		if(ch=='?' || ch=='&') break;
		if(ch=='%') {
			if(isxdigit(url[0]) && isxdigit(url[1])) {
				if(isdigit(url[0]))
					ch = url[0] - '0';
				else
					ch = (url[0]|0x20) - 'a' + 10;
				if(isdigit(url[1]))
					ch = (ch<<4) + url[1] - '0';
				else
					ch = (ch<<4) + (url[1]|0x20) - 'a' + 10;
				url += 2;
			}
		}
		if(ch=='/') {
			/* p always > filename & started with '/' */
			if(p[-1]=='/') {
				/* // */
				p--;
			} else if(p[-1]=='.') {
				if(p[-2]=='/') {
					/* /./ */
					p -= 2;
				} else if(p[-2]=='.' && p[-3]=='/') {
					/* /../ */
					p -= 3;
					if(p > filename)
						while(*--p != '/')
							/* NULL */;
				}
			}
		}
		*p++ = ch;
	}
	if(p[-1]=='.'){
		/* /. */
		if(p[-2]=='/')
			p--;
		else if(p[-2]=='.' && p[-3]=='/') {
			/* /.. */
			p -= 3;
			if(p > filename)
				while(--p > filename && *p != '/')
					/* NULL */;
			p++;
		}
	}
}
