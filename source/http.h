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

u8 *downloadfile(const char *url, u32 *size);
s32 GetConnection(char * domain);
int network_request(int connection, const char * request, char * filename);
int network_read(int connection, u8 *buf, u32 len);

#ifdef __cplusplus
}
#endif

#endif /* _HTTP_H_ */
