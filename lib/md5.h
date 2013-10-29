/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _H_MD5_H
#define _H_MD5_H

#include <stdint.h>

struct MD5Context {
	uint32_t buf[4];
	uint32_t bits[2];
	uint8_t  in[64];
};

typedef struct MD5Context md5_t;

extern void MD5Init(struct MD5Context *);
extern void MD5Update(struct MD5Context *, unsigned char const *, unsigned);
extern void MD5Final(unsigned char digest[16], struct MD5Context *ctx);
extern void MD5Digest( const unsigned char *msg, int len, unsigned char *digest);
extern void MD5HMAC(const unsigned char *password,  unsigned pass_len,
		const unsigned char *challenge, unsigned chal_len,
		unsigned char response[16]);
extern void MD5HMAC2(const unsigned char *password,  unsigned pass_len,
		const unsigned char *challenge, unsigned chal_len,
		const unsigned char *challenge2, unsigned chal_len2,
		unsigned char response[16]);
#endif
