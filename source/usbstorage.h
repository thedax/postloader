#ifndef _UMSSTORAGE_H_
#define _UMSSTORAGE_H_ 

#include <ogcsys.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Prototypes */
u32 UMSStorage_GetCapacity(u32 *_sector_size);
s32  UMSStorage_SetWatchdog(u32);
s32  UMSStorage_Init(void);
void UMSStorage_Deinit(void);
s32 UMSStorage_ReadSectors(u32 sector, u32 numSectors, void *buffer);
s32 UMSStorage_WriteSectors(u32 sector, u32 numSectors, const void *buffer);

extern const DISC_INTERFACE __io_wiiums;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
