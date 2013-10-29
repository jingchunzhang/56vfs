/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _MYCONFIG_H_
#define _MYCONFIG_H_

extern int myconfig_init(int argc, char **argv);
extern void myconfig_reload();
extern int myconfig_put_value(const char *prefix, const char *key, const char *val);
extern int myconfig_get_intval(const char *key, int def);
extern unsigned long myconfig_get_size(const char *key, int def);
extern double myconfig_get_decimal(const char *key);
extern char* myconfig_get_value(const char *key);
extern char* myconfig_get_multivalue(const char *key, int index);
extern int myconfig_cleanup(void);
/*
 * 注册配置reload的回调方法，所有模块都可以注册一个回调函数用来动态reload所属模块的配置
 * func		配置reload回调函数
 * keys		可以reload的配置key的数组，每个元素都是指向静态分配的字符串存储区的指针。NULL表示没有。
 * 比如：   static char* my_reload_keys[] = {"download_speed", "fwd_ip", "fwd_port"};
 * keynum	key的数目
 */
extern void myconfig_register_reload(int (*reload_cb_func)(void), char** keys, int keynum);
//extern void myconfig_print_all();
//extern void myconfig_print_preall();
extern int myconfig_update_value(const char *key, const char *value);
extern int myconfig_dump_to_file();
extern int myconfig_delete_value(const char *pre, const char *key0); 

#endif
