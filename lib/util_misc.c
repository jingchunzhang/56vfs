/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "util.h"
#include "md5.h"

int getcpunum() {
	char buf[16] = {0};
	int num;
	FILE* fp = popen("cat /proc/cpuinfo |grep processor|wc -l", "r");
	if(fp) {
		fread(buf, 1, sizeof(buf) - 1, fp);
		fclose(fp);
	}
	num = atoi(buf);
	if(num <= 0 || num > 16)
		num = 1;
			
	return num;
}
void getmd5(const char* data, unsigned len, unsigned char* md5) {
	md5_t ctx;
	MD5Init(&ctx);
	MD5Update(&ctx, (const unsigned char *)data, len);
	MD5Final(md5, &ctx);	
}
int getfilemd5(const char* filename, unsigned char* md5) {
	
	FILE* file = fopen64(filename, "r");
	if(!file)
		return -1;
		
	md5_t ctx;
	MD5Init(&ctx);

	const int BUFSIZE = 1<<20;
	char buf[BUFSIZE];
	int len;
	while(1) {
		len = fread(buf, 1, BUFSIZE, file);
		if(len > 0) {
			MD5Update(&ctx,(const unsigned char*)buf, len);
			if(len < BUFSIZE)
				break;
		}
		else if(feof(file))
			break;
		else {
			fclose(file);
			return -1;
		}
					  
	}
	fclose(file);
	MD5Final(md5, &ctx);
	return 0;
}

int getfilemd5view(const char* filename, unsigned char* md5) 
{
	unsigned char omd5[16] = {0x0};
	unsigned char smd5[36] = {0x0};

	if (getfilemd5(filename, omd5))
		return -1;

	char *s = (char *)smd5;
	int l = 0;
	int i = 0;
	for (i = 0; i < 16; i++)
	{
		l += snprintf(s + l, sizeof(smd5) -l , "%02x", omd5[i]);
	}
	strcpy(md5, smd5);
	return 0;
}
