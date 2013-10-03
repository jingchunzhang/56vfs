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

static void test_blocking_connection(redisContext *c, char *d, char *f) 
{
	uint32_t ips[1024] = {0x0};
	get_redis(d, f, ips, sizeof(ips), c);

	int i = 0;
	uint32_t *ip = ips;
	while(i < 1024)
	{
		if (*ip == 0)
		{
			ip++;
			i++;
			continue;
		}
		char sip[16] = {0x0};
		ip2str(sip, *ip);
		fprintf(stdout, "%s\n", sip);
		ip++;
		i++;
	}
}

int main(int argc, char **argv) 
{
	if (argc != 5)
	{
		fprintf(stderr, "Usage %s ip port domain file!\n", basename(argv[0]));
		return -1;
	}
    /* Ignore broken pipe signal (for I/O error tests). */
    signal(SIGPIPE, SIG_IGN);

	uint32_t h1, h2, h3;
	get_3_hash("aa", &h1, &h2, &h3);
    redisContext *c = redis_connect(argv[1], atoi(argv[2]));;
    test_blocking_connection(c, argv[3], argv[4]);

    disconnect(c);
    return 0;
}
