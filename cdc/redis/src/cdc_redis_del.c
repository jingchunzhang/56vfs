#include "hiredis.h"
#include "cdc_redis_api.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <time.h>
#include <libgen.h>

/* The following lines make up our testing "framework" :) */

void disconnect(redisContext *c) {
    redisFree(c);
}

redisContext *redis_connect(char *ip, int port) 
{
    redisContext *c = NULL;
	c = redisConnect(ip, port);
    if (!c || c->err) {
        printf("Connection error: %s\n", c->errstr);
        exit(1);
    }

    return c;
}

static void test_blocking_connection(redisContext *c, char *d, char *f, char *sip) 
{
	if(del_ip_redis(d, f, str2ip(sip), c) == 0)
		fprintf(stdout, "del %s %s %s ok!\n", d, f, sip);
}

int main(int argc, char **argv) 
{
	if (argc != 5 && argc != 6)
	{
		fprintf(stderr, "Usage %s ip port domain file dip!\nUsage %s ip port -f fiellist\n", basename(argv[0]), basename(argv[0]));
		return -1;
	}
	/* Ignore broken pipe signal (for I/O error tests). */
	signal(SIGPIPE, SIG_IGN);

	uint32_t h1, h2, h3;
	get_3_hash("aa", &h1, &h2, &h3);
	redisContext *c = redis_connect(argv[1], atoi(argv[2]));;
	if (argc == 6)
		test_blocking_connection(c, argv[3], argv[4], argv[5]);
	else
	{
		FILE *fp = fopen(argv[4], "r");
		if (fp == NULL)
		{
			fprintf(stderr, "fopen %s err %m!\n", argv[4]);
			return -1;
		}
		char buf[1024] = {0x0};
		while (fgets(buf, sizeof(buf), fp))
		{
			char *sep = strchr(buf, ' ');
			if (sep == NULL)
			{
				fprintf(stderr, "format err %s", buf);
				memset(buf, 0, sizeof(buf));
				continue;
			}
			char *s = sep + 1;
			*sep = 0x0;
			sep = strrchr(s, '\n');
			if (sep)
				*sep = 0x0;
			sep = strchr(s, ':');
			if (sep == NULL)
			{
				fprintf(stderr, "format err %s", buf);
				memset(buf, 0, sizeof(buf));
				continue;
			}
			*sep = 0x0;
			char *ip = buf;
			char *d = s;
			char *f = sep + 1;
			test_blocking_connection(c, d, f, ip);
			memset(buf, 0, sizeof(buf));
		}
	}

	disconnect(c);
	return 0;
}
