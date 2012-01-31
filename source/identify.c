#include <gccore.h> 
#include <ogc/machine/processor.h>
#include <string.h>
#include <malloc.h>
#include "identify.h"
#include "sha1.h"
#include "isfs.h"

extern const u8 certs_dat[];
extern const u32 certs_dat_size;
extern const u8 fake_su_ticket_dat[];
extern const u32 fake_su_ticket_dat_size;
extern const u8 fake_su_tmd_dat[];
extern const u32 fake_su_tmd_dat_size;

void Debug (const char *text, ...);

#define HAVE_AHBPROT ((*(vu32*)0xcd800064 == 0xFFFFFFFF) ? 1 : 0)
#define MEM_REG_BASE 0xd8b4000
#define MEM_PROT (MEM_REG_BASE + 0x20a)

static void disable_memory_protection() {
    write32(MEM_PROT, read32(MEM_PROT) & 0x0000FFFF);
}

static u32 apply_patch(char *name, const u8 *old, u32 old_size, const u8 *patch, u32 patch_size, u32 patch_offset) {
    u8 *ptr = (u8 *)0x93400000;
    u32 found = 0;
    u8 *location = NULL;
    while ((u32)ptr < (0x94000000 - patch_size)) {
        if (!memcmp(ptr, old, old_size)) {
            found++;
            location = ptr + patch_offset;
            u8 *start = location;
            u32 i;
            for (i = 0; i < patch_size; i++) {
                *location++ = patch[i];
            }
            DCFlushRange((u8 *)(((u32)start) >> 5 << 5), (patch_size >> 5 << 5) + 64);
        }
        ptr++;
    }
    if (found) {
      Debug ("apply_patch '%s': found", name);
    } else {
      Debug ("apply_patch '%s': not found", name);
    }
    return found;
}

static const u8 di_readlimit_old[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x0A, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
    0x7E, 0xD4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08
};
static const u8 di_readlimit_patch[] = { 0x7e, 0xd4 };

const u8 isfs_permissions_old[] = { 0x42, 0x8B, 0xD0, 0x01, 0x25, 0x66 };
const u8 isfs_permissions_patch[] = { 0x42, 0x8B, 0xE0, 0x01, 0x25, 0x66 };
static const u8 setuid_old[] = { 0xD1, 0x2A, 0x1C, 0x39 };
static const u8 setuid_patch[] = { 0x46, 0xC0 };
const u8 es_identify_old[] = { 0x28, 0x03, 0xD1, 0x23 };
const u8 es_identify_patch[] = { 0x00, 0x00 };
const u8 hash_old[] = { 0x20, 0x07, 0x23, 0xA2 };
const u8 hash_patch[] = { 0x00 };
const u8 new_hash_old[] = { 0x20, 0x07, 0x4B, 0x0B };

static int isPatched = 0;
static int isSU = -1;

static int patchStatus[6];

u32 IOSPATCH_Apply(void) 
	{
    u32 count = 0;
    if (HAVE_AHBPROT) 
		{
		isPatched = 1;
		
        disable_memory_protection();
        patchStatus[0] = apply_patch("di_readlimit", di_readlimit_old, sizeof(di_readlimit_old), di_readlimit_patch, sizeof(di_readlimit_patch), 12);
        patchStatus[1] = apply_patch("isfs_permissions", isfs_permissions_old, sizeof(isfs_permissions_old), isfs_permissions_patch, sizeof(isfs_permissions_patch), 0);
        patchStatus[2] = apply_patch("es_setuid", setuid_old, sizeof(setuid_old), setuid_patch, sizeof(setuid_patch), 0);
        patchStatus[3] = apply_patch("es_identify", es_identify_old, sizeof(es_identify_old), es_identify_patch, sizeof(es_identify_patch), 2);
        patchStatus[4] = apply_patch("hash_check", hash_old, sizeof(hash_old), hash_patch, sizeof(hash_patch), 1);
        patchStatus[5] = apply_patch("new_hash_check", new_hash_old, sizeof(new_hash_old), hash_patch, sizeof(hash_patch), 1);
		}
    return count;
	}
	
int IOSTATCH_Get (int id)
	{
	return (patchStatus[id]);
	}
	
/*****************************************************************************************************************************************************
Patcher From dop-mi
*****************************************************************************************************************************************************/

int Patcher_BruteTMD(tmd *ptmd)
{
        u16 fill;
        for (fill=0; fill<65535; fill++)
        {
                ptmd->fill3=fill;
                sha1 hash;
                SHA1((u8 *)ptmd, TMD_SIZE(ptmd), hash);
                if (hash[0]==0) return 0;
        }
        return -1;
}

void Patcher_ForgeTMD(signed_blob *signedTmd)
{
        Patcher_ZeroSignature(signedTmd);
        Patcher_BruteTMD((tmd*)SIGNATURE_PAYLOAD(signedTmd));
}

int Patcher_BruteTicket(tik *ticket)
{
        u16 fill;
        for (fill=0; fill<65535; fill++)
        {
                ticket->padding=fill;
                sha1 hash;
                SHA1((u8*)ticket, sizeof(ticket), hash);
                if (hash[0]==0) return 0;
        }
        return -1;
}

void Patcher_ForgeTicket(signed_blob *signedTicket)
{
        Patcher_ZeroSignature(signedTicket);
        Patcher_BruteTicket((tik*)SIGNATURE_PAYLOAD(signedTicket));
}

int Patcher_PatchFakeSign(u8 *buf, u32 size)
{
        u32 matchCount = 0;
        u8 hash1[] = {0x20,0x07,0x23,0xA2}; // oldHashCheck
        u8 hash2[] = {0x20,0x07,0x4B,0x0B};     // newHashCheck

        u32 i;
		for (i= 0; i<size-4; i++)
        {
                if (!memcmp(buf + i, hash1, sizeof(hash1)))
                {
                        Debug ("\n\t- Found Hash check @ 0x%X, patching... ", i);
                        buf[i+1] = 0;
                        i += 4;
                        matchCount++;
                        continue;
                }

                if (!memcmp(buf + i, hash2, sizeof(hash2)))
                {
                        Debug ("\n\t- Found Hash check @ 0x%X, patching... ", i);
                        buf[i+1] = 0;
                        i += 4;
                        matchCount++;
                        continue;
                }
        }
        return matchCount;
}

int Patcher_PatchEsIdentity(u8 *buf, u32 size)
{
        u32 matchCount = 0;
        u8 identifyCheck[] = { 0x28, 0x03, 0xD1, 0x23 };

        u32 i;
		for (i = 0; i < size - 4; i++)
        {
                if (!memcmp(buf + i, identifyCheck, sizeof(identifyCheck)))
                {
                        Debug ("Found ES_Identify check @ 0x%X, patching... ", i);
						
                        buf[i+2] = 0;
                        buf[i+3] = 0;
                        i += 4;
                        matchCount++;
                        continue;
                }
        }
        return matchCount;

}

int Patcher_PatchNandPermissions(u8 *buf, u32 size)
{
        u32 i;
        u32 matchCount = 0;
        u8 oldTable[] = {0x42, 0x8B, 0xD0, 0x01, 0x25, 0x66};
        u8 newTable[] = {0x42, 0x8B, 0xE0, 0x01, 0x25, 0x66};

        for (i=0; i< size - sizeof(oldTable); i++)
        {
                if (!memcmp(buf + i, oldTable, sizeof(oldTable)))
                {
                        Debug ("Found NAND Permission check @ 0x%X, patching... ", i);
                        memcpy(buf + i, newTable, sizeof(newTable));
                        i += sizeof newTable;
                        matchCount++;
                        continue;
                }
        }
        return matchCount;
}

void Patcher_ZeroSignature(signed_blob *sig)
{
        u8 *psig = (u8*)sig;
        memset(psig + 4, 0, SIGNATURE_SIZE(sig)-4);
}

/*****************************************************************************************************************************************************
Identify From dop-mi
*****************************************************************************************************************************************************/
void* Tools_AllocateMemory(u32 size)
{
    return (void*)memalign(32, (size+31)&(~31));
}
int Identify_AsSuperUser(void)
{
	int ret = -1;
	u32 keyId = 0;
	u8 *cert = NULL;
	u32 certSize;

	/* Try prebuilt tmd/ticket first, otherwise try building a forged tmd/ticket */
	/* Some IOSes unpatched will work with the prebuilt tmd/ticket, others don't. */
	/* Basically gotta love a buggy IOS */
	Debug ("SU: Reading cert.sys");
	ISFS_Initialize();
	read_full_file_from_nand("/sys/cert.sys", &cert, &certSize);
	ISFS_Deinitialize();
	Debug ("SU: Reading cert.sys -> %d", certSize);
	
	if (cert)
		{
		Debug ("SU:Identify cert.sys");
		ret = Identify(cert, certSize, fake_su_tmd_dat, fake_su_tmd_dat_size, fake_su_ticket_dat, fake_su_ticket_dat_size);
		if (ret > -1) 
			{
			Debug ("SU:Identify cert.sys -> %d", ret);
			return ret;
			}
		}
	
	Debug ("SU:Identify cert.sys failed... brute forcing...");
	
	keyId = 0;

	signed_blob *stmd = NULL;
	signed_blob *sticket = NULL;
	tmd *ptmd = NULL;
	tik *pticket = NULL;
	u64 titleId = 0x100000002ULL;

	stmd = (signed_blob*)Tools_AllocateMemory(520);
	sticket = (signed_blob*)Tools_AllocateMemory(STD_SIGNED_TIK_SIZE);

	memset(stmd, 0, 520);
	memset(sticket, 0, STD_SIGNED_TIK_SIZE);

	stmd = &stmd[0];
	sticket = &sticket[0];
	*stmd = *sticket = 0x10001;
	ptmd = (tmd*)SIGNATURE_PAYLOAD(stmd);
	pticket = (tik*)SIGNATURE_PAYLOAD(sticket);

	strcpy(ptmd->issuer, "Root-CA00000001-CP00000004");
	ptmd->title_id = titleId;
	ptmd->num_contents = 1;
	Patcher_ForgeTMD(stmd);

	strcpy(pticket->issuer, "Root-CA00000001-XS00000003");
	pticket->ticketid =  0x000038A45236EE5FULL;
	pticket->titleid = titleId;

	memset(pticket->cidx_mask, 0xFF, 0x20);
	Patcher_ForgeTicket(sticket);

	Debug ("SU:Identify brute forcing");
	ret = ES_Identify((signed_blob*)cert, certSize, stmd, 520, sticket, STD_SIGNED_TIK_SIZE, &keyId);
	Debug ("SU:Identify brute forcing -> %d", ret);

	free (stmd); stmd = NULL;
	free (sticket); sticket = NULL;
	return ret;
	}

/*****************************************************************************************************************************************************
Identify From dop-mi
*****************************************************************************************************************************************************/

s32 Identify(const u8 *certs, u32 certs_size, const u8 *idtmd, u32 idtmd_size, const u8 *idticket, u32 idticket_size) {
	s32 ret;
	u32 keyid = 0;
	ret = ES_Identify((signed_blob*)certs, certs_size, (signed_blob*)idtmd, idtmd_size, (signed_blob*)idticket, idticket_size, &keyid);
	if (ret < 0){
		switch(ret){
			case ES_EINVAL:
				Debug("Identify Error! ES_Identify (ret = %d;) Data invalid!\n", ret);
				break;
			case ES_EALIGN:
				Debug("Identify Error! ES_Identify (ret = %d;) Data not aligned!\n", ret);
				break;
			case ES_ENOTINIT:
				Debug("Identify Error! ES_Identify (ret = %d;) ES not initialized!\n", ret);
				break;
			case ES_ENOMEM:
				Debug("Identify Error! ES_Identify (ret = %d;) No memory!\n", ret);
				break;
			default:
				Debug("Identify Error! ES_Identify (ret = %d)\n", ret);
				break;
		}
	}
	else
		Debug("Identify OK!\n");
	return ret;
}

s32 Identify_SU(int force) 
	{
	if (!isPatched)
		{
		IOSPATCH_Apply ();
		}
	if (isSU < 0 || force)
		{
		Debug("Identify_SU...");
		isSU = Identify_AsSuperUser ();
		}
	else
		Debug("Identify_SU (already done)...");

	return isSU;
	}
	
//int PatchOS 