#ifndef __CDC_REDIST_API_H_
#define __CDC_REDIST_API_H_

#include "hiredis.h"
#include "hotkey.h"
#include <stdint.h>

int set_into_redis(char *domain, char *f, uint32_t ip, redisContext *c);

void get_redis(char *domain, char *f, uint32_t *ips, int maxlen, redisContext *c);

int check_ip_redis(char *domain, char *f, uint32_t ip, redisContext *c);

int del_ip_redis(char *domain, char *f, uint32_t ip, redisContext *c);

void create_key(t_hotkey *k, char *d, char *f, redisContext *c);
#endif
