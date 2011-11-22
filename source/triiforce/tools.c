/*******************************************************************************
 * tools.c
 *
 * Copyright (c) 2009 The Lemon Man
 * Copyright (c) 2009 Nicksasa
 * Copyright (c) 2009 WiiPower
 *
 * Distributed under the terms of the GNU General Public License (v2)
 * See http://www.gnu.org/licenses/gpl-2.0.txt for more info.
 *
 * Description:
 * -----------
 *
 ******************************************************************************/

#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <wiiuse/wpad.h>

#include "tools.h"
#include "nand.h"
#include "sha1.h"
#include "isfs.h"

#include "../globals.h"

/*
#define info_number 23

static u32 hashes[info_number][5] = {
{0x20e60607, 0x4e02c484, 0x2bbc5758, 0xee2b40fc, 0x35a68b0a},		// cIOSrev13a
{0x620c57c7, 0xd155b67f, 0xa451e2ba, 0xfb5534d7, 0xaa457878}, 		// cIOSrev13b
{0x3c968e54, 0x9e915458, 0x9ecc3bda, 0x16d0a0d4, 0x8cac7917},		// cIOS37 rev18
{0xe811bca8, 0xe1df1e93, 0x779c40e6, 0x2006e807, 0xd4403b97},		// cIOS38 rev18
{0x697676f0, 0x7a133b19, 0x881f512f, 0x2017b349, 0x6243c037},		// cIOS57 rev18
{0x34ec540b, 0xd1fb5a5e, 0x4ae7f069, 0xd0a39b9a, 0xb1a1445f},		// cIOS60 rev18
{0xd98a4dd9, 0xff426ddb, 0x1afebc55, 0x30f75489, 0x40b27ade},		// cIOS70 rev18
{0x0a49cd80, 0x6f8f87ff, 0xac9a10aa, 0xefec9c1d, 0x676965b9},		// cIOS37 rev19
{0x09179764, 0xeecf7f2e, 0x7631e504, 0x13b4b7aa, 0xca5fc1ab},		// cIOS38 rev19
{0x6010d5cf, 0x396415b7, 0x3c3915e9, 0x83ded6e3, 0x8f418d54},		// cIOS57 rev19
{0x589d6c4f, 0x6bcbd80a, 0xe768f258, 0xc53a322c, 0xd143f8cd},		// cIOS60 rev19
{0x8969e0bf, 0x7f9b2391, 0x31ecfd88, 0x1c6f76eb, 0xf9418fe6},		// cIOS70 rev19
{0x30aeadfe, 0x8b6ea668, 0x446578c7, 0x91f0832e, 0xb33c08ac},		// cIOS36 rev20
{0xba0461a2, 0xaa26eed0, 0x482c1a7a, 0x59a97d94, 0xa607773e},		// cIOS37 rev20
{0xb694a33e, 0xf5040583, 0x0d540460, 0x2a450f3c, 0x69a68148},		// cIOS38 rev20
{0xf6058710, 0xfe78a2d8, 0x44e6397f, 0x14a61501, 0x66c352cf},		// cIOS53 rev20
{0xfa07fb10, 0x52ffb607, 0xcf1fc572, 0xf94ce42e, 0xa2f5b523},		// cIOS55 rev20
{0xe30acf09, 0xbcc32544, 0x490aec18, 0xc276cee6, 0x5e5f6bab},		// cIOS56 rev20
{0x595ef1a3, 0x57d0cd99, 0x21b6bf6b, 0x432f6342, 0x605ae60d},		// cIOS57 rev20
{0x687a2698, 0x3efe5a08, 0xc01f6ae3, 0x3d8a1637, 0xadab6d48},		// cIOS60 rev20
{0xea6610e4, 0xa6beae66, 0x887be72d, 0x5da3415b, 0xa470523c},		// cIOS61 rev20
{0x64e1af0e, 0xf7167fd7, 0x0c696306, 0xa2035b2d, 0x6047c736},		// cIOS70 rev20
{0x0df93ca9, 0x833cf61f, 0xb3b79277, 0xf4c93cd2, 0xcd8eae17}		// cIOS80 rev20
};

static char infos[info_number][24] = {
{"cIOS rev13a slot 249"},
{"cIOS rev13b slot 249"},
{"cIOS37rev18 slot 249"},
{"cIOS38rev18 slot 249"},
{"cIOS57rev18 slot 249"},
{"cIOS60rev18 slot 249"},
{"cIOS70rev18 slot 249"},
{"cIOS37rev19 slot 249"},
{"cIOS38rev19 slot 249"},
{"cIOS57rev19 slot 249"},
{"cIOS60rev19 slot 249"},
{"cIOS70rev19 slot 249"},
{"cIOS36rev20 slot 249"},
{"cIOS37rev20 slot 249"},
{"cIOS38rev20 slot 249"},
{"cIOS53rev20 slot 249"},
{"cIOS55rev20 slot 249"},
{"cIOS56rev20 slot 249"},
{"cIOS57rev20 slot 249"},
{"cIOS60rev20 slot 249"},
{"cIOS61rev20 slot 249"},
{"cIOS70rev20 slot 249"},
{"cIOS80rev20 slot 249"}
};	
*/

void *allocate_memory(u32 size)
{
	void *temp;
	temp = memalign(32, (size+31)&(~31) );
	memset(temp, 0, (size+31)&(~31) );
	return temp;
}

s32 Identify_GenerateTik(signed_blob **outbuf, u32 *outlen)
{
	signed_blob *buffer   = NULL;

	sig_rsa2048 *signature = NULL;
	tik         *tik_data  = NULL;

	u32 len;

	/* Set ticket length */
	len = STD_SIGNED_TIK_SIZE;

	/* Allocate memory */
	buffer = (signed_blob *)memalign(32, len);
	if (!buffer)
		return -1;

	/* Clear buffer */
	memset(buffer, 0, len);

	/* Generate signature */
	signature       = (sig_rsa2048 *)buffer;
	signature->type = ES_SIG_RSA2048;

	/* Generate ticket */
	tik_data  = (tik *)SIGNATURE_PAYLOAD(buffer);

	strcpy(tik_data->issuer, "Root-CA00000001-XS00000003");
	memset(tik_data->cidx_mask, 0xFF, 32);

	/* Set values */
	*outbuf = buffer;
	*outlen = len;

	return 0;
}


s32 identify(u64 titleid, u32 *ios)
{
	char filepath[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	u8 *tmdBuffer = NULL;
	u32 tmdSize;
	signed_blob *tikBuffer = NULL;
	u32 tikSize;
	u8 *certBuffer = NULL;
	u32 certSize;
	
	int ret;

	fflush(stdout);
	
	sprintf(filepath, "/title/%08x/%08x/content/title.tmd", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
	ret = read_full_file_from_nand(filepath, &tmdBuffer, &tmdSize);
	if (ret < 0)
	{
		return ret;
	}

	*ios = (u32)(tmdBuffer[0x18b]);

	fflush(stdout);

	Identify_GenerateTik(&tikBuffer,&tikSize);

	fflush(stdout);

	sprintf(filepath, "/sys/cert.sys");
	ret = read_full_file_from_nand(filepath, &certBuffer, &certSize);
	if (ret < 0)
	{
		free(tmdBuffer);
		free(tikBuffer);
		return ret;
	}

	fflush(stdout);

	ret = ES_Identify((signed_blob*)certBuffer, certSize, (signed_blob*)tmdBuffer, tmdSize, tikBuffer, tikSize, NULL);
	if (ret < 0)
	{
		free(tmdBuffer);
		free(tikBuffer);
		free(certBuffer);
		return ret;
	}
	
	free(tmdBuffer);
	free(tikBuffer);
	free(certBuffer);
	return 0;
}

