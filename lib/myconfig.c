#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>
#include <stddef.h>
#include "myconfig.h"
#include "common.h"
#include "list.h"

#define VAL_MAX_LEN 1024

struct myconfig {
	list_head_t list;
	list_head_t pre_list;
	uint32_t hash;		//hash value for key
	char *val;			//value 
	int intval;			//integer format for value
	char key[0];		//key
};

struct prehash {
	list_head_t list;
	char pre[20];
	int used;
};

static char cmd_argv0[4096];
static list_head_t myhash[256];
static struct prehash     prelist[256];
static const char sep[256] = { [' ']=1, ['.']=1, ['-']=1, ['_']=1 };
#define SEP(x,y) sep[*((unsigned char *)(x) + (y))]

static int inited = 0;
#define MAX_RELOAD_KEY_NUM	128
static const char* reload_key[MAX_RELOAD_KEY_NUM] = {	/* config keys could be reload */
	"log_level", 
	"log_rotate_size", 
	"log_rotate_time", 
	"epoll_batch_events",
	"recv_timeout", 
	"send_timeout",
	"keepalive_timeout", 
	"forward_timeout", 
	"forward_connect_timeout", 
	"download_speed",
	"tcp_nodelay",
	"traffic_control",
	"traffic_pending_max",
	"traffic_pending_delta",
	"traffic_pending_mode",
	"traffic_pending_send",
	NULL					/* 后面主要是存放插件需要动态reload的配置项名字 */
}; 

struct reload_cb {
	int (*func)(void);
	struct reload_cb* next;
};
static struct reload_cb* reload_cb_list = NULL;

int myconfig_delete_value(const char *prefix, const char *key) {
	list_head_t *hlist, *l;
	char *str;
	int s, pl;
	char *p;
	uint32_t hash;
	struct myconfig *m;

	pl = strlen(prefix);
	s = strlen(key) + 1;
	str = alloca(pl + s);
	if(str == NULL)
		return -1;
	if(pl)
		memcpy(mempcpy(str, prefix, pl), key, s);
	else
		memcpy(str, key, s);

	for(p = str; *p; p++)
		if(SEP(p, 0)) 
			*p = '_';

	hash = r5hash(str);
	hlist = myhash + (hash & 0xff);
	list_for_each_entry_safe_l(m, l, hlist, list) {
		if(m->hash == hash && !strcmp(m->key, str)) {
			list_del(&m->list);
			list_del_init(&m->pre_list);
			free(m);
		}
	}
	return 1;
}
static void getconfigname(char* buf, const char* argv0) {
	if(argv0!=NULL && argv0[0]!=0)
	{
		strncpy(cmd_argv0, argv0,4095);
	}

	int i;
	char* p;

	char dir[4096]={0};
	char exe[4096]={0};
	i = readlink("/proc/self/exe", buf, 4095);
	if(i > 0 && buf[0] == '/'){
		buf[i] = '\0';
		
		p = rindex(buf, '/');
		
		memcpy(dir, buf, p-buf);
		dir[p-buf]=0;

		p++;
		strncpy(exe, p, 4095);
	}
	else {
		buf[0] = '\0';
		return;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf, 4096, "%s/%s.conf", dir, exe);
	if(access(buf, R_OK) == 0)
	{
		return;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf, 4096, "%s/../conf/%s.conf", dir, exe);
	if(access(buf, R_OK) == 0)
	{
		return;
	}
	buf[0]=0;
	return ;
}

static int myconfig_loadfile0(const char *file) {
	char *buf;
	int n;
	int len;
	char *p, *pn, *k, *v;
	char *section = "";

	n = open(file, O_RDONLY);
	if(n < 0) return -1;

	len = lseek(n, 0L, SEEK_END);
	lseek(n, 0L, SEEK_SET);

	buf = mmap(0, len+1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(buf == MAP_FAILED) {
		close(n);
		return -1;
	}
	if(read(n, buf, len)<0) {
		close(n);
		return -1;
	}
	close(n);

	buf[len] = '\0';
	p = buf;

	while(p < buf+len) {
		pn = strchr(p, '\n');
		if(pn) 
			*pn++ = '\0';
		p += strspn(p, " \t\r"); /* skip leading blanks */
		if(*p == '[') {
			char *q = strchr(++p, ']');
			if(q) {
				while(SEP(q, -1))
					q--;
				*q++ = '-';
				*q = '\0';
				while(SEP(p, 0)) 
					p++;
				if(p[0] == '\0')
					section = "";
				else
					section = p;
			}
		} 
		else if(*p && *p != '#' && *p != ';'){
			k = p;
			p += strcspn(p, " \t\r=");
			v = p; /* v is end of key */
			p += strspn(p, " \t\r"); /* skip blanks between key and = */
			n = 0;
			if(*p == '=') {
				while(*++p == '=')
					n++;
				/* n is multi = */
			}
			*v = '\0';
			v = p + strspn(p, " \t\r");
			p = pn ? pn - 2 : p+strlen(p)-1;
			while(*p == ' '|| *p == '\t' || *p == '\r')
				*p-- = '\0';	/* strip trailling blanks */

			if(isalnum(k[0]) && *v) {
				if(!inited) {		//load first time
					myconfig_put_value(section, k, v);
				}
				else {				//reload，check is reloadable?
					int i;
					for(i = 0; i < MAX_RELOAD_KEY_NUM && reload_key[i]; ++i) {
						if(!strcmp(k, reload_key[i]))
							break;			
					}
					if(reload_key[i]) {
						myconfig_delete_value(section, k);
						myconfig_put_value(section, k, v);
					}
				}
			}
		}
		p = pn;
	}
	munmap(buf, len+1);
	return 0;
}
int myconfig_init(int argc, char **argv) {
	int i;
	char buf[4096], *p;

	for(i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(myhash + i);
		INIT_LIST_HEAD(&(prelist[i].list));
	}

	getconfigname(buf, argv[0]);

	if(myconfig_loadfile0(buf) < 0)
		return -1;

	for(i = 1; i < argc; i++) {
		char *k;
		k = argv[i];
		while(*k == '-')
			k++;
		if(*k == '\0'|| *k == ';' || *k == '#' || *k == '=')
			continue;
		p = strchr(k, '=');
		if(p == NULL) {
			myconfig_put_value("", k, "1");
		} 
		else {
			char *q;
			*p = '\0';
			q = p + 1;
			if(*q == '=') {
				while(*q == '=')
					q++;
				myconfig_delete_value("", k);
			}
			if(*q)
				myconfig_put_value("", k, q);
			*p = '=';
		}
	}
	
	inited = 1;
	
	return 0;
}
void myconfig_reload() {
	
	char buf[4096];
	getconfigname(buf, NULL);

	myconfig_loadfile0(buf);

	//execute all reload function 
	struct reload_cb* cb = reload_cb_list;
	while(cb) {
		cb->func();
		cb = cb->next;
	}
}
int myconfig_put_value(const char *pre, const char *key0, const char *val) {
	list_head_t *hlist;
	struct myconfig *mc;
	int s, pl, lv;
	char *p;

	pl = strlen(pre);
	s = strlen(key0) + 1;
/*	lv = strlen(val) + 1; */
	lv = VAL_MAX_LEN;
	mc = (struct myconfig *)malloc(sizeof(struct myconfig) + pl + s + lv);
	if(mc == NULL)
		return -1;
	if(pl)
		mc->val = mempcpy(mempcpy(mc->key, pre, pl), key0, s);
	else
		mc->val = mempcpy(mc->key, key0, s);
	for(p = mc->key; *p; p++) {
		if(SEP(p, 0)) 
			*p = '_';
		if(*p >= 'A' && *p <= 'Z') 
			*p += 'a' - 'A';
	}

	mc->hash = r5hash(mc->key);
	hlist = myhash+(mc->hash & 0xff);

	memcpy(mc->val, val, strlen(val)+1);
	mc->intval = -1;
	list_add_head(&mc->list, hlist);

	if(!pre)
		pre = "nopre";
	struct prehash *plist;
	int icount;
	for(icount = 0; icount < 256; icount++)
	{
		plist = &(prelist[icount]);
		if(plist->used == 0 || strncmp(plist->pre, pre, strlen(pre)) == 0)
			break;
	}
	if(icount == 256)
		return 0;
	plist->used = 1;
	strncpy(plist->pre, pre, strlen(pre));
	list_add_head(&mc->pre_list, &(plist->list));
	return 1;
}
char* myconfig_get_value(const char *key) {
	uint32_t hash;
	list_head_t *hlist, *l;
	struct myconfig *mc;

	hash = r5hash(key);
	hlist = &(myhash[hash&0xff]);
	list_for_each(l, hlist)
	{
		mc = list_entry(l, struct myconfig, list);
		if(hash == mc->hash && !strcmp(key, mc->key))
			return mc->val;
	}
	return NULL;
}

char* myconfig_get_multivalue(const char *key, int index) {
	uint32_t hash;
	list_head_t *hlist, *l;
	struct myconfig *mc;

	hash = r5hash(key);
	hlist = &(myhash[hash&0xff]);
	list_for_each_prev(l, hlist) {
		mc = list_entry(l, struct myconfig, list);
		if(hash == mc->hash && !strcmp(key, mc->key)) {
			if(index == 0)
				return mc->val;
			index--;
		}
	}
	return NULL;
}

int myconfig_get_intval(const char *key, int def) {
	uint32_t hash;
	list_head_t *hlist, *l;
	struct myconfig *mc;

	hash = r5hash(key);
	hlist = &(myhash[hash&0xff]);
	list_for_each(l, hlist)
	{
		mc = list_entry(l, struct myconfig, list);
		if(hash == mc->hash && !strcmp(key, mc->key))
		{
			if(mc->intval == -1) {
				if(isdigit(mc->val[0]) || (mc->val[0] == '-' && isdigit(mc->val[1])))
					mc->intval = atoi(mc->val);
				else if(!strcasecmp(mc->val, "On"))
					mc->intval = 1;
				else if(!strcasecmp(mc->val, "Off"))
					mc->intval = 0;
				else if(!strcasecmp(mc->val, "Yes"))
					mc->intval = 1;
				else if(!strcasecmp(mc->val, "No"))
					mc->intval = 0;
				else if(!strcasecmp(mc->val, "True"))
					mc->intval = 1;
				else if(!strcasecmp(mc->val, "False"))
					mc->intval = 0;
				else if(!strcasecmp(mc->val, "enable"))
					mc->intval = 1;
				else if(!strcasecmp(mc->val, "disable"))
					mc->intval = 0;
				else if(!strcasecmp(mc->val, "enabled"))
					mc->intval = 1;
				else if(!strcasecmp(mc->val, "disabled"))
					mc->intval = 0;
				else
					return def;
			}
			return mc->intval;
		}
	}
	return def;
}
unsigned long myconfig_get_size(const char *key, int def) {
	uint32_t hash;
	list_head_t *hlist, *l;
	struct myconfig *mc;

	hash = r5hash(key);
	hlist = &(myhash[hash&0xff]);
	list_for_each(l, hlist) {
		mc = list_entry(l, struct myconfig, list);
		if(hash == mc->hash && !strcmp(key, mc->key)) {
			if(!isdigit(mc->val[0])) 
				return def;
			char *pp;
			unsigned long v = strtoul(mc->val, &pp, 0);
			if(pp[0] == 'B' || pp[0] == 'b') {
			} 
			else if(pp[0] == 'K' || pp[0] == 'k') {
				v *= 1024;
			} 
			else if(pp[0] == 'M' || pp[0] == 'm') {
				v *= 1024*1024;
			} 
			else if(pp[0] == 'G' || pp[0] == 'g') {
				v *= 1024*1024*1024;
			}
			return v;
		}
	}
	return def;
}
double myconfig_get_decimal(const char *key) {
	uint32_t hash;
	list_head_t *hlist, *l;
	struct myconfig *mc;

	hash = r5hash(key);
	hlist = &(myhash[hash&0xff]);
	list_for_each(l, hlist) {
		mc = list_entry(l, struct myconfig, list);
		if(hash == mc->hash && !strcmp(key, mc->key)) {
			double a1, a2;
			switch(sscanf(mc->val, "%lf/%lf", &a1, &a2)) {
				case 1: return a1;
				case 2: return a1/a2;
			}
			return NAN;
		}
	}
	return NAN;
}
int myconfig_cleanup(void) {
	list_head_t *l;
	struct myconfig *mc;
	int i;

	for( i=0; i<256; i++) {
		l = myhash[i].next;
		while(l != &myhash[i]) {
			mc = list_entry(l, struct myconfig, list);
			l = l->next;
			free(mc);
		}
	}

	struct reload_cb* cb;
	while(reload_cb_list) {
		cb = reload_cb_list;
		reload_cb_list = reload_cb_list->next;
		free(cb);
	}
	
	return 0;
}
void myconfig_register_reload(int (*func)(void), char** keys, int keynum) {
	//register reload function
	struct reload_cb* cb = (struct reload_cb*)malloc(sizeof(struct reload_cb));
	cb->func = func;
	cb->next = reload_cb_list;
	reload_cb_list = cb;	
	
	//register reload config key
	if(keys && keynum > 0) {
		int i = 0;
		for(i = 0; i < MAX_RELOAD_KEY_NUM; ++i) {
			if(!reload_key[i])
				break;			
		}
		if(MAX_RELOAD_KEY_NUM - i > keynum) {
			int j; 
			for(j = 0; j < keynum; ) {
				reload_key[i++] = keys[j++];
			}
			reload_key[i] = NULL;
		}
		else {
			printf("myconfig_register_reload fail, keynum=%d\n", keynum);
		}
	}
}

/*void myconfig_print_all()
{
	uint32_t hash;
	list_head_t *hlist, *l;
	struct myconfig *mc;

	for (hash = 0; hash <= 0xff; hash++)
	{
		hlist = &(myhash[hash&0xff]);
		list_for_each(l, hlist)
		{
			mc = list_entry(l, struct myconfig, list);
				fprintf(stdout, "key=%s, value=%s\n", mc->key, mc->val);
		}
	}
}

void myconfig_print_preall()
{
	uint32_t hash;
	list_head_t *hlist, *l;
	struct myconfig *mc;

	for (hash = 0; hash <= 0xff; hash++)
	{
		hlist = &(prelist[hash&0xff].list);
		list_for_each(l, hlist)
		{
			mc = list_entry(l, struct myconfig , pre_list);
				fprintf(stdout, "key=%s, value=%s\n", mc->key, mc->val);
		}
	}
}
*/
int myconfig_update_value(const char *key, const char *value) {
	uint32_t hash;
	list_head_t *hlist, *l;
	struct myconfig *mc;

	hash = r5hash(key);
	hlist = &(myhash[hash&0xff]);
	list_for_each(l, hlist)
	{
		mc = list_entry(l, struct myconfig, list);
		if(hash == mc->hash && !strcmp(key, mc->key))
		{
			memset(mc->val, 0, VAL_MAX_LEN);
			memcpy(mc->val, value, strlen(value));
			return 1;
		}
	}

	char * p = strchr(key, '_');
	if(p)
	{
		char pre[20];
		char key0[30];
		memset(pre, 0, sizeof(pre));
		memset(key0, 0, sizeof(key0));
		strncpy(pre, key, p-key);
		pre[p-key] = '-';
		p++;
		strcpy(key0, p);
		return myconfig_put_value(pre, key0, value);
	}

	return myconfig_put_value("", key, value);
}

int myconfig_dump_to_file()
{
	char buf[4096];
	getconfigname(buf,  NULL);

	FILE *fp = fopen(buf, "w");

	if(!fp)
	{
		fprintf(stderr, "%s %d open %s error\n", __FILE__, __LINE__, buf);
		return -1;
	}

	fprintf(fp, "\n##配置文件不区分大小写!\n\n");

	char pre[20];
	uint32_t hash;
	list_head_t *hlist, *l;
	struct myconfig *mc;

	for (hash = 0; hash <= 0xff; hash++)
	{
		hlist = &(prelist[hash&0xff].list);
		if(prelist[hash&0xff].used == 0)
			break;
		if(hash > 0)
		{
			memset(pre, 0, sizeof(pre));
			strncpy(pre, prelist[hash&0xff].pre, strlen(prelist[hash&0xff].pre)-1);
			fprintf(fp, "[ %s ]\n", pre);
		}
		list_for_each(l, hlist)
		{
			mc = list_entry(l, struct myconfig , pre_list);
			if(strcmp(prelist[hash&0xff].pre, "nopre") == 0)
				fprintf(fp, "\t%s = %s\n", mc->key, mc->val);
			else
			{
				char *p = mc->key + strlen(prelist[hash&0xff].pre);
				fprintf(fp, "\t%s = %s\n", p, mc->val);
			}
		}
		fprintf(fp, "-------------------------------------------------------------\n");
	}
	fclose(fp);

	return 0;
}
