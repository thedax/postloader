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
#include <ogc/usbgecko.h>

#include "tools.h"
#include "nand.h"
#include "isfs.h"

extern const u8 cert_sys[];
extern const u32 cert_sys_size;

#define LBSIZE 8192
char * logbuff = NULL;
static int lbsize;

static char debugbuff[1024];
void debug(const char *text, ...)
	{
	static int gecko = 0;
	
	if (gecko == 0)
		{
		if (usb_isgeckoalive (EXI_CHANNEL_1))
			gecko = 2;
		else
			gecko = 1;
		}
		
	va_list args;

	va_start(args, text);
	vsprintf(debugbuff, text, args);

	va_end(args);
	
	if (gecko == 2)
		{
		usb_sendbuffer( EXI_CHANNEL_1, debugbuff, strlen(debugbuff) );
		usb_flush(EXI_CHANNEL_1);
		}
	/*
	printf(debugbuff);
	
	if (strstr (debugbuff, "ERR:"))
		sleep (5);
	*/
	
	if (logbuff == NULL) // allocate space for 
		{
		logbuff = calloc (LBSIZE,1);
		lbsize = 0;
		}
	else
		{
		int blen;
		blen = strlen(debugbuff);
		if (lbsize + blen < LBSIZE)
			{
			strcat (logbuff, debugbuff);
			lbsize += blen;
			}
		}
	}

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

	debug("Reading TMD...");
	fflush(stdout);
	
	sprintf(filepath, "/title/%08x/%08x/content/title.tmd", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
	ret = read_full_file_from_nand(filepath, &tmdBuffer, &tmdSize);
	if (ret < 0)
	{
		debug("ERR: (identify) Reading TMD failed\n");
		return ret;
	}
	debug("done\n");

	*ios = (u32)(tmdBuffer[0x18b]);

	debug("Generating fake ticket...");
	fflush(stdout);

	Identify_GenerateTik(&tikBuffer,&tikSize);
	debug("done\n");

	debug("Reading certs...");
	fflush(stdout);

	sprintf(filepath, "/sys/cert.sys");
	ret = read_full_file_from_nand(filepath, &certBuffer, &certSize);
	if (ret < 0)
		{
		debug("Reading certs failed, trying to use embedded one\n");

		certBuffer = allocate_memory(cert_sys_size);
		certSize = cert_sys_size;
		
		if (certBuffer != NULL)
			memcpy (certBuffer, cert_sys, cert_sys_size);
		else
			{
			free(tmdBuffer);
			free(tikBuffer);
			return ret;
			}
		}
	debug("done\n");
	
	debug("ES_Identify...");
	fflush(stdout);

	ret = ES_Identify((signed_blob*)certBuffer, certSize, (signed_blob*)tmdBuffer, tmdSize, tikBuffer, tikSize, NULL);
	if (ret < 0)
	{
		switch(ret)
		{
			case ES_EINVAL:
				debug("ERR: (identify) ES_Identify (ret = %d;) Data invalid!\n", ret);
				break;
			case ES_EALIGN:
				debug("ERR: (identify) ES_Identify (ret = %d;) Data not aligned!\n", ret);
				break;
			case ES_ENOTINIT:
				debug("ERR: (identify) ES_Identify (ret = %d;) ES not initialized!\n", ret);
				break;
			case ES_ENOMEM:
				debug("ERR: (identify) ES_Identify (ret = %d;) No memory!\n", ret);
				break;
			default:
				debug("ERR: (identify) ES_Identify (ret = %d)\n", ret);
				break;
		}
		free(tmdBuffer);
		free(tikBuffer);
		free(certBuffer);
		sleep (5);
		return ret;
	}
	debug("done\n");
	
	free(tmdBuffer);
	free(tikBuffer);
	free(certBuffer);
	return 0;
}

void tell_cIOS_to_return_to_channel(void)
	{
    if (TITLE_UPPER(old_title_id) > 1 && TITLE_LOWER(old_title_id) > 2) // Don't change anything for system menu or no title id
        {
		static u64 sm_title_id  ATTRIBUTE_ALIGN(32);
		sm_title_id = old_title_id; // title id to be launched in place of the system menu

		int ret;
		
		static ioctlv vector[0x08] ATTRIBUTE_ALIGN(32);

		vector[0].data = &sm_title_id;
		vector[0].len = 8;

		int es_fd = IOS_Open("/dev/es", 0);
		if (es_fd < 0)
			{
			debug("Couldn't open ES module(2)\n");
			return;
			}
		
		ret = IOS_Ioctlv(es_fd, 0xA1, 1, 0, vector);
		
		debug ("tell_cIOS_to_return_to_channel = %d\n", ret);

		IOS_Close(es_fd);
        }
	}
