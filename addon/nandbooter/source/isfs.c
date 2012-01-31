#include <gccore.h>
#include <string.h>
#include <stdlib.h>

#include "tools.h"
#include "isfs.h"




s32 __FileCmp(const void *a, const void *b)
{
	dirent_t *hdr1 = (dirent_t *)a;
	dirent_t *hdr2 = (dirent_t *)b;
	
	if (hdr1->type == hdr2->type)
	{
		return strcmp(hdr1->name, hdr2->name);
	} else
	{
		return 0;
	}
}

s32 getdircount(char *path, u32 *cnt)
{
	if (cnt == NULL) return -2;
	u32 temp = 0;
	s32 res = ISFS_ReadDir(path, NULL, &temp);
	if (res != ISFS_OK)
	{
		debug("ERR: (getdircount) Could not get dir entry count(ret: %d) for:\n'%s'\n", res, path);
		return -1;
	}
	*cnt = temp;
	
	return 1;
}

s32 getdir(char *path, dirent_t **ent, u32 *cnt)
{
	if (ent == NULL) return -2;
	
	s32 res;
	u32 num = 0;

	int i, j, k;
	
	res = getdircount(path, &num);
	if (res < 0)
	{
		//debug("Error: could not get dir entry count! (result: %d)\n", res);
		return -1;
	}
	*cnt = num;
	
	if (num == 0) return 0;	

	char ebuf[ISFS_MAXPATH + 1];

	char *nbuf = (char *)allocate_memory(13 * num);
	if (nbuf == NULL)
	{
		debug("ERR: (getdir) could not allocate buffer for name list!\n");
		return -2;
	}

	res = ISFS_ReadDir(path, nbuf, &num);
	if (res != ISFS_OK)
	{
		debug("ERR: (getdir) could not get name list! (result: %d)\n", res);
		free(nbuf);
		return -3;
	}
	
	*ent = malloc(sizeof(dirent_t) * num);
	if (*ent == NULL)
	{
		debug("ERR: (getdir) could not allocate buffer\n");
		free(nbuf);
		return -4;
	}	

	for (i = 0, k = 0; i < num; i++)
	{	    
		for (j = 0; nbuf[k] != 0; j++, k++)
			ebuf[j] = nbuf[k];
		ebuf[j] = 0;
		k++;

		strcpy((*ent)[i].name, ebuf);
	}
	
	qsort(*ent, *cnt, sizeof(dirent_t), __FileCmp);
	
	free(nbuf);
	return 0;
}

s32 read_full_file_from_nand(char *filepath, u8 **buffer, u32 *filesize)
{
	s32 Fd;
	int ret;

	if (buffer == NULL)
	{
		debug("\n(read_full_file_from_nand) NULL Pointer\n");
		return -1;
	}

	Fd = ISFS_Open(filepath, ISFS_OPEN_READ);
	if (Fd < 0)
	{
		debug("\n(read_full_file_from_nand) ISFS_Open %s failed %d\n", filepath, Fd);
		return Fd;
	}

	fstats *status = allocate_memory(sizeof(fstats));
	if (status == NULL)
	{
		debug("\n(read_full_file_from_nand) Out of memory for status\n");
		ISFS_Close(Fd);
		return -1;
	}
	
	ret = ISFS_GetFileStats(Fd, status);
	if (ret < 0)
	{
		debug("\n(read_full_file_from_nand) ISFS_GetFileStats failed %d\n", ret);
		ISFS_Close(Fd);
		free(status);
		return -1;
	}
	
	*buffer = allocate_memory(status->file_length);
	if (*buffer == NULL)
	{
		debug("\n(read_full_file_from_nand) Out of memory for buffer\n");
		ISFS_Close(Fd);
		free(status);
		return -1;
	}
		
	ret = ISFS_Read(Fd, *buffer, status->file_length);
	if (ret < 0)
	{
		debug("\n(read_full_file_from_nand) ISFS_Read failed %d\n", ret);
		ISFS_Close(Fd);
		free(status);
		free(*buffer);
		return ret;
	}
	ISFS_Close(Fd);

	*filesize = status->file_length;
	free(status);

	return 0;
}

s32 read_file_from_nand(char *filepath, u8 *buffer, u32 filesize)
{
	s32 Fd;
	int ret;

	if (buffer == NULL)
	{
		debug("ERR: (read_file_from_nand) NULL Pointer\n");
		return -1;
	}

	Fd = ISFS_Open(filepath, ISFS_OPEN_READ);
	if (Fd < 0)
	{
		debug("ERR: (read_file_from_nand) ISFS_Open %s failed %d\n", filepath, Fd);
		return Fd;
	}

	fstats *status = allocate_memory(sizeof(fstats));
	if (status == NULL)
	{
		debug("ERR: (read_file_from_nand) Out of memory for status\n");
		ISFS_Close(Fd);
		return -1;
	}
	
	ret = ISFS_GetFileStats(Fd, status);
	if (ret < 0)
	{
		debug("ERR: (read_file_from_nand) ISFS_GetFileStats failed %d\n", ret);
		ISFS_Close(Fd);
		free(status);
		return -1;
	}
	
	if (filesize > status->file_length)
	{
		debug("ERR: (read_file_from_nand) Reading %u bytes failed, filesize: %u\n", filesize, status->file_length);
		ISFS_Close(Fd);
		free(status);
		return -1;
	}
	
	ret = ISFS_Read(Fd, buffer, filesize);
	if (ret < 0)
	{
		debug("ERR: (read_file_from_nand) ISFS_Read failed %d\n", ret);
		ISFS_Close(Fd);
		free(status);
		return ret;
	}
	ISFS_Close(Fd);

	free(status);

	return 0;
}
