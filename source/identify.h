#ifndef _PATCHER_H_
#define _PATCHER_H_

#define PATCH_DI_READLIMIT 0
#define PATCH_ISFS_PERMISSIONS 1
#define PATCH_ES_SETUID 2
#define PATCH_ES_IDENTIFY 3
#define PATCH_HASH_CHECK 4
#define PATCH_NEW_HASH_CHECK 5

int Patcher_BruteTMD(tmd *ptmd);
void Patcher_ForgeTMD(signed_blob *signedTmd);

int Patcher_BruteTicket(tik *ticket);
void Patcher_ForgeTicket(signed_blob *signedTicket);

int Patcher_PatchFakeSign(u8 *buf, u32 size);
int Patcher_PatchEsIdentity(u8 *buf, u32 size);
int Patcher_PatchNandPermissions(u8 *buf, u32 size);
void Patcher_ZeroSignature(signed_blob *sig);

u32 IOSPATCH_Apply(void);
int IOSTATCH_Get(int id);

s32 Identify(const u8 *certs, u32 certs_size, const u8 *idtmd, u32 idtmd_size, const u8 *idticket, u32 idticket_size);
s32 Identify_SU(int force);

#endif
