#ifndef _CDC_HTTP_DB_H_
#define _CDC_HTTP_DB_H_

int init_db();

int get_ip_list_db(uint32_t *uips, int maxlen, char *d, char *f);

#endif
