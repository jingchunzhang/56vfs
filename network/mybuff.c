/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include "mybuff.h"


void mybuff_init(struct mybuff* mybuff) {
	mybuff->data = NULL;
	mybuff->size = 0;
	mybuff->len = 0;
	mybuff->fd = -1;
	mybuff->foffset = mybuff->flen = 0;
}
int mybuff_setdata(struct mybuff* mybuff, const char* data, size_t len) {
	
	if(mybuff->data == NULL) {
		mybuff->data = malloc(init_buff_size);
		mybuff->size = init_buff_size;
	}
	
	if(mybuff->size - mybuff->len > len) {
		memcpy(mybuff->data + mybuff->len, data, len);
		mybuff->len += len;
	}
	else {
		mybuff->size = mybuff->len + len + 1;
		char* tmp = realloc(mybuff->data, mybuff->size);
		if(!tmp) {
			printf("out of memory, %s:%d\n", __FILE__, __LINE__);
			//_exit(-1);
		}

		mybuff->data = tmp;
		memcpy(mybuff->data + mybuff->len, data, len);
		mybuff->len += len;
	}
	return 0;
}
int mybuff_getdata(struct mybuff* mybuff, char** data, size_t * len) {
	if(mybuff->data) {
		*data = mybuff->data;
		*len = mybuff->len;
		if(*len > 0)
			return 0;
	}
	return -1;
}
void mybuff_skipdata(struct mybuff* mybuff, size_t len) {
	if(mybuff->data) {
		if(len >= mybuff->len) {
			mybuff->len = 0;
		}
		else {
			memmove(mybuff->data, mybuff->data + len, mybuff->len - len);
			mybuff->len -= len;
		}

	}
}
int mybuff_setfile(struct mybuff* mybuff, int fd, off_t offset, size_t len) {
	if(mybuff->fd >= 0)
		close(mybuff->fd);
	mybuff->fd = fd;
	mybuff->foffset = offset;
	mybuff->flen = len;	
	
	return 0;
}
int mybuff_getfile(struct mybuff* mybuff, int* fd, off_t * offset, size_t * len) {
	if(mybuff->fd >= 0 && mybuff->flen > 0) {
		*fd = mybuff->fd;
		*offset = mybuff->foffset;
		*len = mybuff->flen;
		return 0;
	}
	return -1;
}
void mybuff_skipfile(struct mybuff* mybuff, size_t len) {
	if(mybuff->fd >= 0) {
		if(mybuff->flen <= len) {
			close(mybuff->fd);
			mybuff->fd = -1;
			mybuff->foffset = mybuff->flen = 0;
		}	
		else {	
			mybuff->foffset += len;
			mybuff->flen -= len;
		}
	}
}

void mybuff_reinit(struct mybuff* mybuff) {
	if(mybuff->fd >= 0) {
		close(mybuff->fd);
		mybuff->fd = -1;
		mybuff->foffset = mybuff->flen = 0;
	}
	if(mybuff->size > (init_buff_size << 1)) {
		
		free(mybuff->data);
		mybuff->data = malloc(init_buff_size);
		
		mybuff->size = init_buff_size;
	}
	mybuff->len = 0;
}
void mybuff_fini(struct mybuff* mybuff) {
	mybuff_reinit(mybuff);
	if(mybuff->data)
		free(mybuff->data);
}
