#ifndef __CDC_HTTP_DB_H_
#define __CDC_HTTP_DB_H_
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
enum ROLE {UNKOWN = 0, ROLE_FCS , ROLE_CS, ROLE_TRACKER, ROLE_VOSS_MASTER, ROLE_HOT_CS, SELF_IP, ROLE_MAX};

typedef struct {
	char ip[16];
	char domain[16];
	char fname[256];
	char task_type[2];
	char task_stime[16];
}t_voss_key;

typedef struct {
	uint64_t fsize;
	char fmd5[34];
	char over_status[16];
	char task_ctime[16];
	char role[2];
}t_voss_val;

typedef struct {
	char ip[16];
	char day[10];
}t_voss_s_key;

typedef struct {
	int total;
	int success;
	int fail;
}t_voss_s_val;

#ifdef __cplusplus
extern "C"
{
#endif

int init_db();

void close_db();

int get_sel_count(char *sql);

int mydb_begin();

int mydb_commit();

int mydb_get_voss(t_voss_key *k, t_voss_val *v);

void merge_db(time_t last);

void process_db_stat(char rec[][256]);

#ifdef __cplusplus
}
#endif
#endif
