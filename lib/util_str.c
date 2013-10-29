/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <stdlib.h>
#include <string.h>
#include "util.h"

static const char default_ifs[256] = { [9]=1, [10]=1, [13]=1, [32]=1 };

void inline build_ifs(char *tifs, const char *ifs0) {
	const unsigned char *ifs = (const unsigned char *)ifs0;
	memset(tifs, 0, 256);
	while(*ifs) tifs[*ifs++] = 1;
}

/*
 * NULL IFS: default blanks
 * first byte is NULL, IFS table
 * first byte is NOT NULL, IFS string
 */
int str_explode(const char *ifs, char *line0, char *field[], int n) {
	unsigned char *line = (unsigned char *)line0;
	int i;

	if(ifs==NULL) {
		ifs = default_ifs;
	} else if(*ifs) {
		char *implicit_ifs = alloca(256);
		build_ifs(implicit_ifs, ifs);
		ifs = implicit_ifs ;
	}

	i = 0;
	while(1) {
		while(ifs[*line]) line++;
		if(!*line) break;
		if(line[0]=='"' || line[0]=='\'') {
		    field[i++] = (char *)line+1;
		    line = (unsigned char *)strchr((char *)line+1, line[0]);
		    if(line==NULL) break;
		    *line++ = '\0';
		    if(i>=n) break;
		} else {
		    field[i++] = (char *)line;
		    if(i>=n) {
			line += strlen((char *)line)-1;
			while(ifs[*line]) line--;
			line[1] = '\0';
			break;
		    }
		    while(*line && !ifs[*line]) line++;
		    if(!*line) break;
		    *line++ = '\0';
		}
	}
	return i;
}
