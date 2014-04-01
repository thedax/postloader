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
#include "../zip/infdef.h"
#include "../fsop/fsop.h"
//#include "../multithread.h"

#define PORT	4299
#define STACKSIZE	8192

#define PRIO_IDLE 8
#define PRIO_RCV 32

#define SET(a, b) a = b; DCFlushRange(&a, sizeof(a));

s_wiiload wiiload;

static char incommingIP[20];
static u32 uncfilesize;
static volatile int stopNetworkThread;
static volatile int stopGeckoThread;

static u8 * threadStackG = NULL;
static u8 * threadStack = NULL;
static lwp_t networkthread = LWP_THREAD_NULL;
static lwp_t geckothread = LWP_THREAD_NULL;

static char tpath[256];
static int errors = 0;
static int firstInit = 1;
static int started = 0;
static volatile int pauseWiiload = 0;
static int threadStartSleep;

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
			{
			read += ret;
			}
			
		if (ticks_to_millisecs(gettime()) > t)
			break;
		}

	return read;
	}
	
static int GeckoRead(int connection, u8 *buf, u32 len, u32 tout) // timeout in msec
	{
	u32 read = 0;
	s32 ret = 0;
	u32 t;
	
	t = ticks_to_millisecs(gettime()) + tout;

	while (read < len)
		{
		ret = usb_recvbuffer_safe_ex(connection,  buf + read, len - read, 500);
		
		if (ret > 0)
			{
			t = ticks_to_millisecs(gettime()) + tout;
			read += ret;
			}
		else
			usleep (1000);
			
		if (ticks_to_millisecs(gettime()) > t)
			break;
		}
	
	return read;
	}
	

static bool UncompressData (char *wiiloadVersion, int isZip)
	{
	//mt_Lock();
	
	printopt("UncompressData");

	char source[256];
	char target[256];

	if (isZip)
		{
		sprintf (source, "%s/%s", tpath, WIILOAD_TMPFILE);
		sprintf (target, "%s/%s", tpath, WIILOAD_ZIPFILE);
		
		unlink (target);
		if (rename (source, target) == 0) //success
			{
			wiiload.status = WIILOAD_HBZREADY;
			}
		}
	else if ((wiiloadVersion[0] > 0 || wiiloadVersion[1] > 4) && uncfilesize != 0) //WiiLoad zlib compression
		{
		printopt("compressed file");
		
		sprintf (source, "%s/%s", tpath, WIILOAD_TMPFILE);
		sprintf (target, "%s/%s", tpath, wiiload.filename);
		
		int ret = zlib_inf (source, target);
		
		printopt ("zlib_inf = %d", ret);
		
		// OLD MODE buff = fsop_ReadFile (target, 0, &buffsize);
		strcpy (wiiload.fullpath, target);
		wiiload.status = WIILOAD_HBREADY;
		}
	else
		{
		sprintf (source, "%s/%s", tpath, WIILOAD_TMPFILE);
		sprintf (target, "%s/%s", tpath, wiiload.filename);
		
		unlink (target);
		if (rename (source, target) == 0) //success
			{
			strcpy (wiiload.fullpath, target);
			wiiload.status = WIILOAD_HBREADY;
			}
		}

    //mt_Unlock();
	return true;
	}

int __usb_checkrecv(s32 chn);

static bool GeckoLoad (void)
	{
	int compressed = 0;
	int ret;
	char wiiloadVersion[2];
	unsigned char header[16];
	size_t buffsize;
	u8 *buff;
	
	memset (header, 0, sizeof (header));
	ret = GeckoRead(EXI_CHANNEL_1, header, 16, 1000);
	if (ret < 16)
		return false;

	if (memcmp(header, "HAXX", 4) != 0) // unsupported protocol
		{
		usb_flush (EXI_CHANNEL_1);
		return false;
		}
		
	LWP_SetThreadPriority (geckothread, PRIO_RCV);

	wiiloadVersion[0] = header[4];
	wiiloadVersion[1] = header[5];
	
	memcpy ((u8 *) &buffsize, &header[8], 4);
	if (header[4] > 0 || header[5] > 4)
		{
		//printopt ("compressed file !");
		compressed = 1;
		memcpy ((u8 *) &uncfilesize, &header[12], 4);
		}
	
	buff = NULL;

	buff = (u8 *) malloc(buffsize);
	if (!buff)
		{
		usb_flush (EXI_CHANNEL_1);
		printopt ("Not enough memory.");
		return false;
		}

	LWP_SetThreadPriority(geckothread, 64);
	//printopt ("Receiving file (%s)...", incommingIP);

	u32 done = 0;
	u32 blocksize = 1024*16;
	int result;
	
	if (!compressed) // if the file isn't compressed, 4 bytes of file are in the header vect...
		{
		memcpy ((u8 *) buff, &header[12], 4);
		done += 4;
		}

	do
		{
		if (blocksize > buffsize - done)
			blocksize = buffsize - done;

		result = GeckoRead(EXI_CHANNEL_1, buff+done, blocksize, 1000);
		if (result > 0)
			done += result;
		else
			break;

		printopt ("!%d%c", (done * 100) / buffsize, 37);
		} 
	while (done < buffsize);
	
	LWP_SetThreadPriority(geckothread, 8);
	
	if (done != buffsize)
		{
		wiiload.status = WIILOAD_IDLE;
		free (buff);
		buffsize = 0;
		buff = 0;
		printopt("Filesize doesn't match.");
		return false;
		}

	// These are the arguments....
	char temp[1024];
	ret = GeckoRead(EXI_CHANNEL_1, (u8 *) temp, 1023, 1000);
	
	temp[ret] = 0;
	
	if (ret > 2 && temp[ret - 1] == '\0' && temp[ret - 2] == '\0') // Check if it is really an arg
		{
		wiiload.args = malloc (ret);
		if (wiiload.args)
			{
			memcpy (wiiload.args, temp, ret);
			
			// convert arguments in ; separated items
			int i;
			for (i = 0; i < ret; i++)
				{
				if (wiiload.args[i] == '\0' && wiiload.args[i+1] != '\0')
					wiiload.args[i] = ';';
				}

			wiiload.argl = ret;
			}
		}
	temp[ret] = 0;

	printopt("Filename %s (%d)", temp, ret);
	strcpy (wiiload.filename, temp);
	
	int isZip = 0;
	if (buff[0] == 'P' && buff[1] == 'K' && buff[2] == 0x03 && buff[3] == 0x04)
		isZip = 1;

	// to keep compatibility with wiiload over network, we must store the file on device...
	char path[256];
	sprintf (path, "%s/%s", tpath, WIILOAD_TMPFILE);

	//mt_Lock();
	ret = fsop_WriteFile (path, buff, buffsize);
	//mt_Unlock();
	
	free (buff);
	buff = NULL;
	
	if (ret)
		{
		UncompressData (wiiloadVersion, isZip);
		}
		
	free (buff);
	
	LWP_SetThreadPriority (geckothread, PRIO_IDLE);
	
	return true;
	}
	
static bool WiiLoad (s32 connection)
	{
	char wiiloadVersion[2];
	unsigned char header[9];
	size_t buffsize;

	wiiload.status = WIILOAD_RECEIVING;
			
	printopt ("WiiLoad begin");
	
	NetRead(connection, header, 8, 250);

	if (memcmp(header, "HAXX", 4) != 0) // unsupported protocol
		{
		printopt ("unsupported protocol");
		return false;
		}
		
	LWP_SetThreadPriority (networkthread, PRIO_RCV);

	wiiloadVersion[0] = header[4];
	wiiloadVersion[1] = header[5];

	NetRead(connection, (u8 *) &buffsize, 4, 250);

	if (header[4] > 0 || header[5] > 4)
		{
		printopt ("compressed file !");
		NetRead(connection, (u8*) &uncfilesize, 4, 250); // Compressed protocol, read another 4 bytes
		}
	
	/*
	buff = (u8 *) malloc(buffsize);
	if (!buff)
		{
		printopt ("Not enough memory.");
		return false;
		}
	*/
	
	printopt ("Receiving file (%s)...", incommingIP);

	u32 done = 0;
	u32 blocksize = 4096;
	
	u8 *buff = malloc (blocksize);

	int retries = 10;
	int result;
	int isZip = 0;
	
	FILE *f;
	char path[300];
	sprintf (path, "%s/%s", tpath, WIILOAD_TMPFILE);
	f = fopen (path, "wb");

	do
		{
		if (blocksize > buffsize - done)
			blocksize = buffsize - done;

		result = net_read(connection, buff, blocksize);
		if (result <= 0)
			{
			--retries;
			usleep (100000); // lets wait 10 msec
			}
		else
			{
			if (f) fwrite (buff, 1, result, f);
			
			retries = 10;
			
			if (done == 0 && (buff[0] == 'P' && buff[1] == 'K' && buff[2] == 0x03 && buff[3] == 0x04))
				{
				Debug ("This is a ZIP file");
				isZip = 1;
				}
				
			done += result;
			}

		if (retries == 0)
			{
			free (buff);
			buff = 0;
			buffsize = 0;
			if (f) fclose(f);
			printopt ("Transfer failed.");
			return false;
			}

		//printopt ("%d of %d bytes (r %d)", done, buffsize, retries);
		printopt ("!%d%c", (done * 100) / buffsize, 37);
		//fflush(stdout);
		} 
	while (done < buffsize);
	
	if (f) fclose(f);
	
	if (done != buffsize)
		{
		wiiload.status = WIILOAD_IDLE;
		//free (buff);
		//buffsize = 0;
		//buff = 0;
		printopt("Filesize doesn't match.");
		return false;
		}

	// These are the arguments....
	char temp[1024];
	int ret = NetRead(connection, (u8 *) temp, 1023, 250);
	temp[ret] = 0;
	
	//Debug_hexdump (temp, ret);
	
	if (ret > 2 && temp[ret - 1] == '\0' && temp[ret - 2] == '\0') // Check if it is really an arg
		{
		wiiload.args = malloc (ret);
		if (wiiload.args)
			{
			memcpy (wiiload.args, temp, ret);

			// convert arguments in ; separated items
			int i;
			for (i = 0; i < ret; i++)
				{
				if (wiiload.args[i] == '\0' && wiiload.args[i+1] != '\0')
					wiiload.args[i] = ';';
				}

			wiiload.argl = ret;
			}
		}

	temp[ret] = 0;

	printopt("Filename %s (%d)", temp, ret);
	strcpy (wiiload.filename, temp);
	
	wiiload.status = WIILOAD_IDLE;
	UncompressData (wiiloadVersion, isZip);
	
	free (buff);
	
	LWP_SetThreadPriority (networkthread, PRIO_IDLE);
		
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
	if (wiiload.args) free (wiiload.args);
	
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
		printopt ("net_setsockopt %d");
		return false;
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
		int i;
		
		for (i = 0; i < 10; i++)
			{
			usleep (50000);
			if (pauseWiiload) break;
			}
		
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
			
		usleep (100);
		}
	while (!stopNetworkThread);
	
	net_close(socket);
	
	return true;
	}
	
static void * GeckoThread(void *arg)
	{
	stopNetworkThread = 0;
	errors = 0;
	
	LWP_SetThreadPriority(geckothread, 8);
	
	printopt("gecko thread running, ready !");

	if (usb_isgeckoalive (EXI_CHANNEL_1))
		{
		wiiload.gecko = 1;
		while (!stopGeckoThread)
			{
			if (!GeckoLoad ())
				usb_flush (EXI_CHANNEL_1);
				
			usleep (100);
			}
		}
	else
		while (!stopGeckoThread)
			{
			usleep (250000);
			}
		
	printopt("exiting gecko thread...");

	stopGeckoThread = 2;

	return 0;
	}

static void * WiiloadThread(void *arg)
	{
	int netInit = 0;
	int netReady = 0;
	int wc24cleared = 0;
	
	stopNetworkThread = 0;
	errors = 0;
	
	strcpy(wiiload.ip, "<unknown>");
	
	printopt("Net thread running, ready !");
	
	while (!stopNetworkThread)
		{
		if (!netInit)
			{
			s32 res;
			
			res = net_init_async(NULL, NULL);
			Debug ("net_init_async %d", res);

			if (res != 0)
				{
				errors ++;
				continue;
				}
				
			netInit = 1;
			}
			
		if (netInit)
			{
			if (errors > 5 && !wc24cleared)
				{
				Debug ("Cleareing  net_wc24cleanup");
				
				net_deinit ();
				net_wc24cleanup();
				
				errors = 0;
				netInit = 0;
				wc24cleared = 1;
				}
			
			s32 res;
			res = net_get_status();
			Debug ("net_get_status %d", res);

			if (res == 0)
				{
				struct in_addr hostip;
				hostip.s_addr = net_gethostip();

				if (hostip.s_addr)
					{
					strcpy(wiiload.ip, inet_ntoa(hostip));
					netReady = 1;
					}
				}
			else
				errors ++;
			}
			
		if (netReady)
			{
			if (!StartWiiLoadServer ())
				errors ++;
			}
			
		sleep (1);
			
		if (errors > 10)
			{
			Debug ("too many errors");
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
void WiiLoad_Start(char *tempPath, int startSleep)
	{
	threadStartSleep = startSleep;
	
	if (firstInit)
		{
		memset (&wiiload, 0, sizeof(s_wiiload));
		firstInit = 0;
		}
		
	wiiload.status = WIILOAD_INITIALIZING;
	
	if (tempPath != NULL)
		{
		strcpy (tpath, tempPath);
		}
	
	threadStack = (u8 *) memalign(32, STACKSIZE);
	
	if(!threadStack)
		{
		wiiload.status = WIILOAD_STOPPED;
		return;
		}
	else
		LWP_CreateThread (&networkthread, WiiloadThread, NULL, threadStack, STACKSIZE, 8);

	threadStackG = (u8 *) memalign(32, STACKSIZE);
	if (!threadStackG)
		{
		return;
		}
	else
		LWP_CreateThread (&geckothread, GeckoThread, NULL, threadStackG, STACKSIZE, 8);

	started = 1;
	}

void WiiLoad_Stop(void)
	{
	int tout;
	
	Debug ("WiiLoad_Stop");
	if (!started || stopNetworkThread == 1)
		{
		Debug ("WiiLoad_Stop: nothing to do...");
		return; // 
		}
		
	stopNetworkThread = 1;	
	stopGeckoThread = 1;
	
	tout = 0;
	while ((stopNetworkThread == 1 || stopGeckoThread == 1) && tout < 50)
		{
		usleep (100000);
		tout++;
		}
		
	if (tout < 50)
		{
		LWP_JoinThread (networkthread, NULL);
		LWP_JoinThread (geckothread, NULL);
		free (threadStack);
		free (threadStackG);

		// Clean old data, if any
		if (wiiload.args) free (wiiload.args);
		}
	else	
		Debug ("WiiLoad_Stop: Something gone wrong ! %d %d", stopNetworkThread, stopGeckoThread );

	
	wiiload.args = NULL;
	wiiload.argl = 0;

	Debug ("WiiLoad_Stop: done...");
	}

void WiiLoad_Pause (void)
	{
	pauseWiiload = 1;

	int tout = 0;
	while (pauseWiiload == 1 && tout < 50)
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