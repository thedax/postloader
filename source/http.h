#ifndef _HTTP_H_
#define _HTTP_H_

#include <errno.h>
#include <ogcsys.h>
#include <stdarg.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include "dns.h"

extern const struct block emptyblock;

typedef void (*http_Callback)(void); 

typedef struct 
	{
	size_t headersize;
	size_t bytes;
	size_t size;
	
	http_Callback cb;
	}
s_http;

s_http http;

u8 *downloadfile (const char *url, u32 *size, http_Callback cb);
s32 GetConnection(char * domain);
int network_request(int connection, const char * request, char * filename);
int network_read(int connection, u8 *buf, u32 len);

#ifdef __cplusplus
}
#endif

#endif /* _HTTP_H_ */
