#include "cdc_redis_api.h"
#include "GeneralHashFunctions.h"
#include "hotkey.h"
#include <stdint.h>

#define DB_MOD 0xF

static void db_switch(redisContext *c, t_hotkey *k)
{
	char cmd[128] = {0x0};
	snprintf(cmd, sizeof(cmd), "SELECT %d", k->h1&DB_MOD);

    redisReply *reply;
	reply = redisCommand(c, cmd);
	freeReplyObject(reply);
}

void create_key(t_hotkey *k, char *d, char *f, redisContext *c)
{
	char s[256] = {0x0};
	snprintf(s, sizeof(s), "%s:%s", d, f);
	get_3_hash(s, &(k->h1), &(k->h2), &(k->h3));
	db_switch(c, k);
}

static int check_ip_redis_inter(t_hotkey *k, uint32_t ip, redisContext *c)
{
	int get = -1;
	redisReply *reply = redisCommand(c, "GET %b", k, sizeof(t_hotkey));
	if (reply->type == REDIS_REPLY_STRING)
	{
		uint32_t *cip = (uint32_t *)reply->str;
		int i = reply->len / sizeof(uint32_t);
		while (i-- > 0)
		{
			if (*cip == ip)
			{
				get = 0;
				break;
			}
			cip++;
		}
	}
	freeReplyObject(reply);
	return get;
}

int check_ip_redis(char *domain, char *f, uint32_t ip, redisContext *c)
{
	t_hotkey k;
	create_key(&k, domain, f, c);
	return check_ip_redis_inter(&k, ip, c);
}

int del_ip_redis(char *domain, char *f, uint32_t ip, redisContext *c)
{
	t_hotkey k;
	create_key(&k, domain, f, c);
	int get = -1;
	redisReply *reply = redisCommand(c, "GET %b", &k, sizeof(t_hotkey));
	if (reply->type == REDIS_REPLY_STRING)
	{
		uint32_t *cip = (uint32_t *)reply->str;
		int i = reply->len / sizeof(uint32_t);
		while (i-- > 0)
		{
			if (*cip == ip)
			{
				char *s = (char *) cip;
				char *d = s + sizeof(uint32_t);
				int mlen = reply->len - (d - reply->str);
				if (mlen > 0)
					memmove(s, d, mlen);
				else
					*cip = 0x0;
				int slen = reply->len - sizeof(uint32_t);
				redisReply *reply1 = redisCommand(c, "SET %b %b", &k, sizeof(t_hotkey), reply->str, slen);
				freeReplyObject(reply1);
				get = 0;
				break;
			}
			cip++;
		}
	}
	freeReplyObject(reply);
	return get;
}

int set_into_redis(char *domain, char *f, uint32_t ip, redisContext *c)
{
	t_hotkey k;
	create_key(&k, domain, f, c);
	if (check_ip_redis_inter(&k, ip, c) == 0)
		return 0;
    redisReply *reply;
	reply = redisCommand(c,"APPEND %b %b", &k, sizeof(t_hotkey), &ip, sizeof(ip));
	freeReplyObject(reply);
	return 0;
}

void get_redis(char *domain, char *f, uint32_t *ips, int maxlen, redisContext *c)
{
	t_hotkey k;
	create_key(&k, domain, f, c);
    redisReply *reply;
	reply = redisCommand(c, "GET %b", &k, sizeof(t_hotkey));
	if (reply->type == REDIS_REPLY_STRING)
	{
		int validlen = maxlen <= reply->len ? maxlen : reply->len;
		memcpy(ips, reply->str, validlen);
	}
	freeReplyObject(reply);
}
