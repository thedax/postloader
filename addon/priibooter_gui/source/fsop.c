/*////////////////////////////////////////////////////////////////////////////////////////

fsop contains coomprensive set of function for file and folder handling

en exposed s_fsop fsop structure can be used by callback to update operation status

////////////////////////////////////////////////////////////////////////////////////////*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h> //for mkdir 

#include "fsop.h"

s_fsop fsop;

int error = FSOP_OK;

int fsop_GetLastErr (void)
	{
	return error;
	}

u8 *fsop_GetBuffer (char *fn, int *size, fsopCallback vc)
	{
	u8 *buff;
	FILE * f;
	int bytes;
	
	error = FSOP_OK;
	
	f = fopen(fn, "rb");
	if (!f) 
		{
		error = FSOP_FOPEN;
		return NULL;
		}
	
	//Get file size
	fseek( f, 0, SEEK_END);
	bytes = ftell(f);
	if (size) *size = bytes;

	fseek( f, 0, SEEK_SET);
	
	if (bytes == 0)
		{
		error = FSOP_FILEISEMPTY;
		return NULL;
		}
		
	buff = malloc (bytes);
	if (!buff)
		{
		error = FSOP_CANNOTMALLOC;
		return NULL;
		}
	
	bytes = fread (buff, 1, bytes, f);
	fclose(f);
	
	return buff;
	}

bool fsop_StoreBuffer (char *fn, u8 *buff, int size, fsopCallback vc)
	{
	FILE * f;
	int bytes;
	
	f = fopen(fn, "wb");
	if (!f) return FALSE;
	
	bytes = fwrite (buff, 1, size, f);
	fclose(f);
	
	if (bytes == size) return TRUE;
	
	return false;
	}

bool fsop_FileExist (char *fn)
	{
	FILE * f;
	f = fopen(fn, "rb");
	if (!f) return FALSE;
	fclose(f);
	return TRUE;
	}
	
bool fsop_DirExist (char *path)
	{
	DIR *dir;
	
	dir=opendir(path);
	if (dir)
		{
		closedir(dir);
		return TRUE;
		}
	
	return FALSE;
	}

bool fsop_CopyFile (char *source, char *target, fsopCallback vc)
	{
	u8 *buff = NULL;
	int size;
	int bytes, rb;
	int block = 65536*4; // (256Kb)
	FILE *fs = NULL, *ft = NULL;
	
	ft = fopen(target, "wt");
	if (!ft)
		return FALSE;
	
	fs = fopen(source, "rb");
	if (!fs)
		return FALSE;

	//Get file size
	fseek( fs, 0, SEEK_END);
	size = ftell(fs);

	fsop.size = size;
	
	if (size <= 0)
		{
		fclose (fs);
		return NULL;
		}
		
	// Return to beginning....
	fseek( fs, 0, SEEK_SET);
	
	buff = malloc (block);
	if (buff == NULL) 
		{
		fclose (fs);
		return NULL;
		}
	
	bytes = 0;
	do
		{
		rb = fread(buff, 1, block, fs );
		fwrite(buff, 1, rb, ft );
		bytes += rb;
		fsop.bytes = bytes;
		if (vc) vc();
		}
	while (bytes < size);

	fclose (fs);
	free (buff);

	return buff;
	}

/*
Semplified folder make
*/
int fsop_MakeFolder (char *path)
	{
	if (mkdir(path, S_IREAD | S_IWRITE) == 0) return TRUE;
	
	return FALSE;
	}

/*
Recursive copyfolder
*/
bool fsop_CopyFolder (char *source, char *target, fsopCallback vc)
	{
	DIR *pdir;
	struct dirent *pent;
	char newSource[300], newTarget[300];
	
	// If target folder doesn't exist, create it !
	if (!fsop_DirExist (target))
		{
		fsop_MakeFolder (target);
		}

	pdir=opendir(source);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;
			
		sprintf (newSource, "%s/%s", source, pent->d_name);
		sprintf (newTarget, "%s/%s", target, pent->d_name);
		
		// If it is a folder... recurse...
		if (fsop_DirExist (newSource))
			{
			fsop_CopyFolder (newSource, newTarget, vc);
			}
		else	// It is a file !
			{
			strcpy (fsop.op, pent->d_name);
			fsop_CopyFile (newSource, newTarget, vc);
			}
		}
	
	closedir(pdir);

	return TRUE;
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
			}
		}
	
	closedir(pdir);
	
	unlink (source);
	
	return TRUE;
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
	//Debug ("fsop_CreateFolderTree (%s, %d, %d)", path, start, len);

	for (i = start; i <= len; i++)
		{
		if (path[i] == '/' || i == len)
			{
			strcpy (buff, path);
			buff[i] = 0;

			//Debug ("fsop_CreateFolderTree: %s", buff);
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

