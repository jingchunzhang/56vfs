/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <stdlib.h>
#include <netinet/in.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

uint32_t str2ip(const char *str) {
	uint32_t addr = 0, temp = 0;

	if(isdigit(str[0])==0)
		return INADDR_NONE;

	addr = strtoul(str, (char **)&str, 0);
	if(*str != '.') {
	    temp = addr >> 24;
	    if(temp==0 || temp >= 224)
		return INADDR_NONE;
	    addr = htonl(addr);
	} else if(isdigit(str[1])==0)
		return INADDR_NONE;
	else {
	    if(addr==0 || addr >= 224)
		return INADDR_NONE;
#if __BYTE_ORDER != __LITTLE_ENDIAN
	    addr <<= 24;
#endif
	    temp = strtoul(str+1, (char **)&str, 0);
	    if(*str != '.') {
	    	if(temp >= 1<<24)
			return INADDR_NONE;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		addr += htonl(temp);
#else
		addr += temp;
#endif
	    } else if(isdigit(str[1])==0)
			return INADDR_NONE;
	    else {
		if(temp >= 256)
			return INADDR_NONE;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		addr += temp<<8;
#else
		addr += temp<<16;
#endif

		temp = strtoul(str+1, (char **)&str, 0);
		if(*str != '.') {
		    if(temp >= 1<<16)
			return INADDR_NONE;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		    addr += htonl(temp);
#else
		    addr += temp;
#endif
		} else if(isdigit(str[1])==0)
			return INADDR_NONE;
		else {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		    addr += temp<<16;
#else
		    addr += temp<<8;
#endif
		    temp = strtoul(str+1, (char **)&str, 0);
		    if(temp >= 256)
			return INADDR_NONE;
#if __BYTE_ORDER == __LITTLE_ENDIAN
		    addr += temp<<24;
#else
		    addr += temp;
#endif
		}
	    }
	}

	return addr;
}

char *ip2str(char *str, uint32_t ip) {
	unsigned char *c = (unsigned char *)&ip;
	if(c[0]>=100) {
		*str++ = '0' + c[0]/100;
		*str++ = '0' + (c[0]/10)%10;
		*str++ = '0' + c[0]%10;
	} else if(c[0]>=10) {
		*str++ = '0' + c[0]/10;
		*str++ = '0' + c[0]%10;
	} else
		*str++ = '0' + c[0];
	*str++ = '.';

	if(c[1]>=100) {
		*str++ = '0' + c[1]/100;
		*str++ = '0' + (c[1]/10)%10;
		*str++ = '0' + c[1]%10;
	} else if(c[1]>=10) {
		*str++ = '0' + c[1]/10;
		*str++ = '0' + c[1]%10;
	} else
		*str++ = '0' + c[1];
	*str++ = '.';

	if(c[2]>=100) {
		*str++ = '0' + c[2]/100;
		*str++ = '0' + (c[2]/10)%10;
		*str++ = '0' + c[2]%10;
	} else if(c[2]>=10) {
		*str++ = '0' + c[2]/10;
		*str++ = '0' + c[2]%10;
	} else
		*str++ = '0' + c[2];
	*str++ = '.';

	if(c[3]>=100) {
		*str++ = '0' + c[3]/100;
		*str++ = '0' + (c[3]/10)%10;
		*str++ = '0' + c[3]%10;
	} else if(c[3]>=10) {
		*str++ = '0' + c[3]/10;
		*str++ = '0' + c[3]%10;
	} else
		*str++ = '0' + c[3];

	return str;
}
unsigned getpeerip(int fd) {
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	getpeername(fd, (struct sockaddr*)&addr, &addr_len);
	return (unsigned)addr.sin_addr.s_addr;	
}
unsigned getipbyif(const char* ifname) {

	if(!ifname)
		return 0;

	register int fd, intrface;
	struct ifreq buf[10];
	struct ifconf ifc;
	unsigned ip = 0; 

	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
	{
		ifc.ifc_len = sizeof(buf);
		ifc.ifc_buf = (caddr_t)buf;
		if(!ioctl(fd, SIOCGIFCONF, (char*)&ifc))
		{       
			intrface = ifc.ifc_len / sizeof(struct ifreq); 
			while(intrface-- > 0)  
			{       
				if(strcmp(buf[intrface].ifr_name, ifname) == 0)
				{       
					if(!(ioctl(fd, SIOCGIFADDR, (char *)&buf[intrface])))
						ip = (unsigned)((struct sockaddr_in *)(&buf[intrface].ifr_addr))->sin_addr.s_addr;
					break;  
				}       
			}       
		}       
		close(fd);
	}
	return ip;	
}
