#ifndef _APPLOADER_H_
#define _APPLOADER_H_

/* Entry point */
typedef void (*entry_point)(void);

/* Prototypes */
s32 Apploader_Run(entry_point *, bool, u8, GXRModeObj *vmode, bool, bool, bool, const u8 *, u32, u8, u32, u8, char *altDolDir);

#endif
