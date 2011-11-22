#ifndef _UMSSTORAGE_H_
#define _UMSSTORAGE_H_

#ifdef __cplusplus
extern "C" {
#endif
    /* Prototypes */
    s32  UMSStorage_GetCapacity(u32 *);
    s32  UMSStorage_Init(void);
    void UMSStorage_Deinit(void);
    s32 UMSStorage_Watchdog(u32 on_off);
    s32  UMSStorage_ReadSectors(u32, u32, void *);
    s32  UMSStorage_WriteSectors(u32, u32, const void *);
    extern const DISC_INTERFACE __io_wiiums;
#ifdef __cplusplus
}
#endif

#endif
