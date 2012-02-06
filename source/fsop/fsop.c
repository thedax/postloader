/*////////////////////////////////////////////////////////////////////////////////////////

fsop contains coomprensive set of function for file and folder handling

en exposed s_fsop fsop structure can be used by callback to update operation status

////////////////////////////////////////////////////////////////////////////////////////*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <ogcsys.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h> //for mkdir 
#include <sys/statvfs.h>

#include "fsop.h"
#include "../debug.h"
#include "../mem2.h"

s_fsop fsop;


// read a file from disk
u8 *fsop_ReadFile (char *path, size_t bytes2read, size_t *bytesReaded)
	{
	FILE *f;
	size_t size = 0;
	
	f = fopen(path, "rb");
	if (!f)
		{
		if (bytesReaded) *bytesReaded = size;
		return NULL;
		}

	//Get file size
	fseek( f, 0, SEEK_END);
	size = ftell(f);
	if (bytesReaded) *bytesReaded = size;
	if (size == 0) return NULL;
	
	// goto to start
	fseek( f, 0, SEEK_SET);
	
	if (bytes2read < size)
		size = bytes2read;
	
	u8 *buff = malloc (size);
	size = fread (buff, 1, size, f);
	fclose (f);
	
	if (bytesReaded) *bytesReaded = size;

	return buff;
	}

// write a buffer to disk
bool fsop_WriteFile (char *path, u8 *buff, size_t len)
	{
	FILE *f;
	size_t size = 0;
	
	f = fopen(path, "wb");
	if (!f)
		{
		return false;
		}

	size = fwrite (buff, 1, len, f);
	fclose (f);

	if (size == len) return true;
	return false;
	}

// return false if the file doesn't exist
bool fsop_GetFileSizeBytes (char *path, size_t *filesize)	// for me stats st_size report always 0 :(
	{
	FILE *f;
	size_t size = 0;
	
	f = fopen(path, "rb");
	if (!f)
		{
		if (filesize) *filesize = size;
		return false;
		}

	//Get file size
	fseek( f, 0, SEEK_END);
	size = ftell(f);
	if (filesize) *filesize = size;
	fclose (f);
	
	Debug ("fsop_GetFileSizeBytes (%s) = %u", path, size);
	
	return true;
	}

/*

*/
u32 fsop_CountDirItems (char *source)
	{
	DIR *pdir;
	struct dirent *pent;
	u32 count = 0;
	
	pdir=opendir(source);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;

		count++;
		}
	
	closedir(pdir);
	
	return count;
	}

/*
Recursive fsop_GetFolderBytes
*/
u64 fsop_GetFolderBytes (char *source, fsopCallback vc)
	{
	DIR *pdir;
	struct dirent *pent;
	char newSource[300];
	u64 bytes = 0;
	
	pdir=opendir(source);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;
			
		sprintf (newSource, "%s/%s", source, pent->d_name);
		
		// If it is a folder... recurse...
		if (fsop_DirExist (newSource))
			{
			bytes += fsop_GetFolderBytes (newSource, vc);
			}
		else	// It is a file !
			{
			size_t s;
			fsop_GetFileSizeBytes (newSource, &s);
			bytes += s;
			}
		}
	
	closedir(pdir);
	
	Debug ("fsop_GetFolderBytes (%s) = %llu", source, bytes);
	
	return bytes;
	}

u32 fsop_GetFolderKb (char *source, fsopCallback vc)
	{
	u32 ret = (u32) round ((double)fsop_GetFolderBytes (source, vc) / 1000.0);

	Debug ("fsop_GetFolderKb (%s) = %u", source, ret);

	return ret;
	}

u32 fsop_GetFreeSpaceKb (char *path) // Return free kb on the device passed
	{
	struct statvfs s;
	
	statvfs (path, &s);
	
	u32 ret = (u32)round( ((double)s.f_bfree / 1000.0) * s.f_bsize);
	
	Debug ("fsop_GetFreeSpaceKb (%s) = %u", path, ret);
	
	return ret ;
	}


bool fsop_StoreBuffer (char *fn, u8 *buff, int size, fsopCallback vc)
	{
	FILE * f;
	int bytes;
	
	f = fopen(fn, "wb");
	if (!f) return false;
	
	bytes = fwrite (buff, 1, size, f);
	fclose(f);
	
	if (bytes == size) return true;
	
	return false;
	}

bool fsop_FileExist (char *fn)
	{
	FILE * f;
	f = fopen(fn, "rb");
	if (!f) return false;
	fclose(f);
	return true;
	}
	
bool fsop_DirExist (char *path)
	{
	DIR *dir;
	
	dir=opendir(path);
	if (dir)
		{
		closedir(dir);
		return true;
		}
	
	return false;
	}

bool fsop_CopyFile (char *source, char *target, fsopCallback vc)
	{
	int err = 0;
	fsop.breakop = 0;
	
	u8 *buff = NULL;
	u32 size;
	u32 bytes, rb,wb;
	u32 block = 65536;
	FILE *fs = NULL, *ft = NULL;
	
	Debug ("fsop_CopyFile (%s, %s): Started", source, target);
	
	if (strstr (source, "usb:") && strstr (target, "usb:"))
		{
		Debug ("fsop_CopyFile: buffer size changed to %dKbyte", block / 1024);
		block = 1024*1048;
		}
	
	fs = fopen(source, "rb");
	if (!fs)
		{
		Debug ("fsop_CopyFile: Unable to open source file");
		return false;
		}

	ft = fopen(target, "wt");
	if (!ft)
		{
		fclose (fs);
		Debug ("fsop_CopyFile: Unable to open target file");
		return false;
		}
	
	//Get file size
	fseek ( fs, 0, SEEK_END);
	size = ftell(fs);

	fsop.size = size;
	
	if (size == 0)
		{
		fclose (fs);
		fclose (ft);
		Debug ("fsop_CopyFile: Warning file size 0");
		return true;
		}
		
	// Return to beginning....
	fseek( fs, 0, SEEK_SET);
	
	buff = mem2_malloc (block);
	if (buff == NULL) 
		{
		fclose (fs);
		Debug ("fsop_CopyFile: ERR Unable to allocate buffers");
		return false;
		}
	
	bytes = 0;
	do
		{
		rb = fread(buff, 1, block, fs );
		wb = fwrite(buff, 1, rb, ft );
		
		if (wb != wb) err = 1;
		bytes += rb;
		
		fsop.multy.bytes += rb;
		fsop.bytes = bytes;
		
		if (vc) vc();
		if (fsop.breakop) break;
		}
	while (bytes < size && err == 0);

	fclose (fs);
	fclose (ft);
	
	mem2_free (buff);
	
	Debug ("fsop_CopyFile: bytes %u, size %u, err %d, breakop %d", bytes, size, err, fsop.breakop);
	
	if (err) unlink (target);

	if (fsop.breakop || err) return false;
	
	return true;
	}

/*
Semplified folder make
*/
int fsop_MakeFolder (char *path)
	{
	if (mkdir(path, S_IREAD | S_IWRITE) == 0) return true;
	
	return false;
	}

/*
Recursive copyfolder
*/
static bool doCopyFolder (char *source, char *target, fsopCallback vc)
	{
	DIR *pdir;
	struct dirent *pent;
	char newSource[300], newTarget[300];
	bool ret = true;
	
	// If target folder doesn't exist, create it !
	if (!fsop_DirExist (target))
		{
		fsop_MakeFolder (target);
		}

	pdir=opendir(source);
	
	while ((pent=readdir(pdir)) != NULL && ret == true) 
		{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;
			
		sprintf (newSource, "%s/%s", source, pent->d_name);
		sprintf (newTarget, "%s/%s", target, pent->d_name);
		
		// If it is a folder... recurse...
		if (fsop_DirExist (newSource))
			{
			ret = doCopyFolder (newSource, newTarget, vc);
			}
		else	// It is a file !
			{
			strcpy (fsop.op, pent->d_name);
			ret = fsop_CopyFile (newSource, newTarget, vc);
			}
		}
	
	closedir(pdir);

	return ret;
	}
	
bool fsop_CopyFolder (char *source, char *target, fsopCallback vc)
	{
	fsop.breakop = 0;
	fsop.multy.start_t = time(NULL);
	fsop.multy.bytes = 0;
	fsop.multy.size = fsop_GetFolderBytes (source, vc);
	
	Debug ("fsop_CopyFolder");
	Debug ("fsop.multy.start_t = %u", fsop.multy.start_t);
	Debug ("fsop.multy.bytes = %llu", fsop.multy.bytes);
	Debug ("fsop.multy.size = %llu (%u Mb)", fsop.multy.size, (u32)((fsop.multy.size/1000)/1000));
	
	//return false;
	
	//fsop_KillFolderTree (target, vc);
	
	return doCopyFolder (source, target, vc);
	}
	
/*
Recursive copyfolder
*/
bool fsop_KillFolderTree (char *source, fsopCallback vc)
	{
	DIR *pdir;
	struct dirent *pent;
	char newSource[300];
	
	pdir=opendir(source);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;
			
		sprintf (newSource, "%s/%s", source, pent->d_name);
		
		// If it is a folder... recurse...
		if (fsop_DirExist (newSource))
			{
			fsop_KillFolderTree (newSource, vc);
			}
		else	// It is a file !
			{
			sprintf (fsop.op, "Removing %s", pent->d_name);
			unlink (newSource);
			Debug ("fsop_KillFolderTree: removing '%s'", newSource);
			}
		}
	
	closedir(pdir);
	
	unlink (source);
	
	return true;
	}
	

// Pass  <mount>://<folder1>/<folder2>.../<folderN> or <mount>:/<folder1>/<folder2>.../<folderN>
bool fsop_CreateFolderTree (char *path)
	{
	int i;
	int start, len;
	char buff[300];
	char b[8];
	char *p;
	
	start = 0;
	
	strcpy (b, "://");
	p = strstr (path, b);
	if (p == NULL)
		{
		strcpy (b, ":/");
		p = strstr (path, b);
		if (p == NULL)
			return false; // path must contain
		}
	
	start = (p - path) + strlen(b);
	
	len = strlen(path);
	Debug ("fsop_CreateFolderTree (%s, %d, %d)", path, start, len);

	for (i = start; i <= len; i++)
		{
		if (path[i] == '/' || i == len)
			{
			strcpy (buff, path);
			buff[i] = 0;

			fsop_MakeFolder(buff);
			}
		}
	
	// Check if the tree has been created
	return fsop_DirExist (path);
	}
	
	
// Count the number of folder in a full path. It can be path1/path2/path3 or <mount>://path1/path2/path3
int fsop_CountFolderTree (char *path)
	{
	int i;
	int start, len;
	char b[8];
	char *p;
	int count = 0;
	
	start = 0;
	
	strcpy (b, "://");
	p = strstr (path, b);
	if (p == NULL)
		{
		strcpy (b, ":/");
		p = strstr (path, b);
		}
	
	if (p == NULL)
		start = 0;
	else
		start = (p - path) + strlen(b);
	
	len = strlen(path);
	if (path[len-1] == '/') len--;
	if (len <= 0) return 0;

	for (i = start; i <= len; i++)
		{
		if (path[i] == '/' || i == len)
			{
			count ++;
			}
		}
	
	// Check if the tree has been created
	return count;
	}

