#include "util.h"

int decode_hex(const char* hex, char* bin, int buflen)
{
	char c, s;
	int i = 0;

	while(i < buflen && *hex) {
		s = 0x20 | (*hex++);
		if(s >= '0' && s <= '9')
			c = s - '0';
		else if(s >= 'a' && s <= 'f')
			c = s - 'a' + 10;
		else
			break;

		c <<= 4;
		s = 0x20 | (*hex++);
		if(s >= '0' && s <= '9')
			c += s - '0';
		else if(s >= 'a' && s <= 'f')
			c += s - 'a' + 10;
		else
			break;
		bin[i++] = c;
	}
	if(i<buflen) bin[i] = '\0';
	return i;
}

int encode_hex(char* hex, const char* bin, int binlen)
{
	int i;
	for(i=0; i<binlen; i++) {
		*hex++ = "0123456789abcdef"[((unsigned char *)bin)[i] >> 4];
		*hex++ = "0123456789abcdef"[((unsigned char *)bin)[i] & 15];
	}
	*hex = '\0';
	return binlen*2;
}
