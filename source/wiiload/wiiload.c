#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ogcsys.h>
#include <network.h>
#include <malloc.h>
#include <ogc/machine/processor.h>
#include <ogc/lwp_watchdog.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include "wiiload.h"
#include "../debug.h"

#define PORT	4299
#define STACKSIZE	8192

s_wiiload wiiload;

static char ip[16];
static char incommingIP[20];
static u32 uncfilesize;
static int stopNetworkThread;

static u8 * threadStack = NULL;
static lwp_t networkthread = LWP_THREAD_NULL;

static char *tpath;
static int errors = 0;
static int firstInit = 1;
static int pauseWiiload = 0;

s32 socket;

/*

*/
void printopt(const char *text, ...)
	{
	va_list args;

	va_start(args, text);
	vsprintf(wiiload.op, text, args);
	va_end(args);

	if (wiiload.op[0] != '!')
		Debug ("wiiload.op = %s",  wiiload.op);
	
	wiiload.opUpd ++;
	}


static void NetInit(void)
	{
	s32 result;
	
	net_init ();

	result = if_config (ip, NULL, NULL, true);
	
	printopt ("if_config = %d, ip = %s", result, ip);

	if (result < 0) 
		{
		return;
		}
	return;
	}
	
static int NetRead(int connection, u8 *buf, u32 len, u32 tout) // timeout in msec
	{
	u32 read = 0;
	s32 ret = 0;
	u32 t;
	
	t = ticks_to_millisecs(gettime()) + tout;

	while (read < len)
		{
		ret = net_read(connection, buf + read, len - read);

		if (ret <= 0)
			usleep (10 * 1000);
		else
			read += ret;
			
		if (ticks_to_millisecs(gettime()) > t)
			break;
		}

	return read;
	}
	

static u8 * UncompressData (char *wiiloadVersion)
	{
	if (!wiiload.buff)
		return NULL;
		
	printopt("UncompressData");

	//Zip File
	if (wiiload.buff[0] == 'P' && wiiload.buff[1] == 'K' && wiiload.buff[2] == 0x03 && wiiload.buff[3] == 0x04)
		{
		char path[200];
		
		sprintf (path, "%s/%s", tpath, WIILOAD_ZIPFILE);
		
		// TODO: blockwrite to let other thread works...
		FILE * file = fopen(path, "wb");
		if (!file)
			{
			printopt("unable to open %s", path);
			free (wiiload.buff);
			return NULL;
			}

		int ret;
		ret = fwrite(wiiload.buff, 1, wiiload.buffsize, file);
		fclose(file);

		free (wiiload.buff);
		wiiload.buff = NULL;
		
		if (ret == wiiload.buffsize)
			wiiload.status = WIILOAD_HBZREADY;
		}
	else if ((wiiloadVersion[0] > 0 || wiiloadVersion[1] > 4) && uncfilesize != 0) //WiiLoad zlib compression
		{
		printopt("compressed file");
		
		u8 * unc = (u8 *) malloc(uncfilesize);
		if (!unc)
			{
			free (wiiload.buff);
			return NULL;
			}

		uLongf f = uncfilesize;

		if (uncompress(unc, &f, wiiload.buff, wiiload.buffsize) != Z_OK)
			{
			printopt("uncompression filed");
			
			free(unc);
			free(wiiload.buff);
			
			wiiload.buff = NULL;
			return NULL;
			}

		free(wiiload.buff);
		
		printopt("uncompression ok: newsize %u (old %u)", uncfilesize, wiiload.buffsize);

		wiiload.buff = unc;
		wiiload.buffsize = f;
		}

    return wiiload.buff;
	}
	
static bool WiiLoad (s32 connection)
	{
	char wiiloadVersion[2];
	unsigned char header[9];
	
	wiiload.status = WIILOAD_RECEIVING;
			
	printopt ("WiiLoad begin");
	
	NetRead(connection, header, 8, 250);

	if (memcmp(header, "HAXX", 4) != 0) // unsupported protocol
		{
		printopt ("unsupported protocol");
		return false;
		}

	wiiloadVersion[0] = header[4];
	wiiloadVersion[1] = header[5];

	NetRead(connection, (u8 *) &wiiload.buffsize, 4, 250);

	if (header[4] > 0 || header[5] > 4)
		{
		printopt ("compressed file !");
		NetRead(connection, (u8*) &uncfilesize, 4, 250); // Compressed protocol, read another 4 bytes
		}
	
	wiiload.buff = NULL;

	wiiload.buff = (u8 *) malloc(wiiload.buffsize);
	if (!wiiload.buff)
		{
		printopt ("Not enough memory.");
		return false;
		}

	printopt ("Receiving file (%s)...", incommingIP);

	u32 done = 0;
	u32 blocksize = 1024*4;
	int retries = 10;
	int result;

	do
		{
		if (blocksize > wiiload.buffsize - done)
			blocksize = wiiload.buffsize - done;

		result = net_read(connection, wiiload.buff+done, blocksize);
		if (result <= 0)
			{
			--retries;
			usleep (10000); // lets what 10 msec
			}
		else
			{
			retries = 10;
			done += result;
			}

		if (retries == 0)
			{
			free (wiiload.buff);
			wiiload.buff = 0;
			wiiload.buffsize = 0;
			printopt ("Transfer failed.");
			return false;
			}

		//printopt ("%d of %d bytes (r %d)", done, wiiload.buffsize, retries);
		printopt ("!%d%c", (done * 100) / wiiload.buffsize, 37);
		fflush(stdout);
		} 
	while (done < wiiload.buffsize);
	
	if (done != wiiload.buffsize)
		{
		wiiload.status = WIILOAD_IDLE;
		free (wiiload.buff);
		wiiload.buffsize = 0;
		wiiload.buff = 0;
		printopt("Filesize doesn't match.");
		return false;
		}

	// These are the arguments....
	char temp[1024];
	int ret = NetRead(connection, (u8 *) temp, 1023, 250);
	
	temp[ret] = 0;
	
	if (ret > 2 && temp[ret - 1] == '\0' && temp[ret - 2] == '\0') // Check if it is really an arg
		{
		wiiload.args = malloc (ret);
		if (wiiload.args)
			{
			memcpy (wiiload.args, temp, ret);
			wiiload.argl = ret;
			}
		}
	/*
	int i = 0;
	for (i = 0; i < ret; i++)
		{
		if (temp[i] <= 32)
			temp[i] = '_';
		}
	*/
	temp[ret] = 0;

	printopt("Filename %s (%d)", temp, ret);
	strcpy (wiiload.filename, temp);
	
	if (UncompressData (wiiloadVersion))
		wiiload.status = WIILOAD_HBREADY;
		
	return true;
	}
	
static bool StartWiiLoadServer (void)
	{
	struct sockaddr_in sin;
	struct sockaddr_in client_address;
	s32 connection;
	int err;
	
	if ( wiiload.status == WIILOAD_HBREADY || wiiload.status == WIILOAD_HBZREADY)
		{
		// This isn't an error, simply we have received valid data. Host process should set hbready mode to IDLE or stop/restart the thread
		return true;
		}

	// Clean old data, if any
	if (wiiload.buff) free (wiiload.buff);
	if (wiiload.args) free (wiiload.args);
	
	wiiload.buff = NULL;
	wiiload.args = NULL;
	wiiload.argl = 0;
	
	printopt ("StartWiiLoadServer begin");
	
	socklen_t addrlen = sizeof(client_address);

	//Open socket
	socket = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (socket == INVALID_SOCKET)
		{
		printopt ("net_socket INVALID_SOCKET");
		return false;
		}

	int one = 1;
	err = net_setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
	if (err < 0)
		{
		printopt ("net_setsockopt SO_REUSEADDR error");
		//return false;
		}
	
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	int flags = net_fcntl(socket, F_GETFL, 0);
	flags = net_fcntl(socket, F_SETFL, flags|4); // Set non blocking

	err = net_bind(socket, (struct sockaddr*)&sin, sizeof(sin));
	if (err < 0)
		{
		printopt ("net_bind error %d", err);
		net_close(socket);
		return false;
		}

	if (net_listen(socket, 10) < 0)
		{
		printopt ("net_listen error");
		net_close(socket);
		return false;
		}
	
	printopt ("net_accept");
	
	// WIILOAD_IDLE is set once. This because if an hb is executed it make no sense....
	// Maybe it can be usefull for an app is an hb is rejected. Hosting appl. can set it by itsel
	
	wiiload.status = WIILOAD_IDLE;

	do
		{
		usleep (250*1000);
		
		if (pauseWiiload)
			{
			Debug ("WiiLoad thread paused");
			pauseWiiload = 2;
			do
				{
				usleep (250*1000);
				}
			while (pauseWiiload && !stopNetworkThread);
			Debug ("WiiLoad thread resumed");
			pauseWiiload = 0;
			}
		
		connection = net_accept(socket, (struct sockaddr*)&client_address, &addrlen);
		if (connection >= 0)
			{
			sprintf(wiiload.op, "%s", inet_ntoa(client_address.sin_addr));
			if (!WiiLoad (connection))
				{
				wiiload.status = WIILOAD_IDLE;
				}
			net_close(connection);
			}
		}
	while (!stopNetworkThread);
	
	net_close(socket);
	
	return true;
	}
	
static void * WiiloadThread(void *arg)
	{
	stopNetworkThread = 0;
	errors = 0;
	
	printopt("Net thread running");
	
	LWP_SetThreadPriority(networkthread, 0);
	
	NetInit();
	
	while (!stopNetworkThread)
		{
		if (!StartWiiLoadServer ())
			errors ++;
			
		usleep (1000*1000);
			
		if (errors > 10)
			{
			stopNetworkThread = 1;
			}
		}
	
	stopNetworkThread = 2;
	
	wiiload.status = WIILOAD_STOPPED;
	
	net_deinit();
	
	return NULL;
	}

/****************************************************************************
 * InitNetworkThread with priority 0 (idle)
 ***************************************************************************/
void WiiLoad_Start(char *tempPath)
	{
	if (firstInit)
		{
		memset (&wiiload, 0, sizeof(s_wiiload));
		firstInit = 0;
		}
		
	wiiload.status = WIILOAD_INITIALIZING;
	
	if (tempPath != NULL)
		{
		tpath = malloc (strlen(tempPath) + 1);
		strcpy (tpath, tempPath);
		}
	
	threadStack = (u8 *) memalign(32, STACKSIZE);
	
	if(!threadStack)
		{
		wiiload.status = WIILOAD_STOPPED;
		return;
		}

	LWP_CreateThread (&networkthread, WiiloadThread, NULL, threadStack, STACKSIZE, 30);
	LWP_ResumeThread(networkthread);
	}

void WiiLoad_Stop(void)
	{
	if (stopNetworkThread != 1) 
		return; // It is already stopped
		
	stopNetworkThread = 1;
	
	int tout = 0;
	while (stopNetworkThread == 1 && tout < 500)
		{
		usleep (100000);
		tout++;
		}
		
	if (tout >= 500)
		LWP_SuspendThread (networkthread);
	
	free (threadStack);

	// Clean old data, if any
	if (wiiload.buff) free (wiiload.buff);
	if (wiiload.args) free (wiiload.args);
	
	wiiload.buff = NULL;
	wiiload.args = NULL;
	wiiload.argl = 0;
	
	net_wc24cleanup();
	}

void WiiLoad_Pause (void)
	{
	pauseWiiload = 1;

	int tout = 0;
	while (pauseWiiload == 1 && tout < 500)
		{
		Debug ("pauseWiiload = %d, tout = %d", pauseWiiload, tout);
		usleep (100000);
		tout++;
		}
	}
	
void WiiLoad_Resume (void)
	{
	pauseWiiload = 0;
	}