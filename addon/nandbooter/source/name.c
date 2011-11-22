#include <gccore.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>

#include "tools.h"
#include "isfs.h"


bool check_text(char *s) 
{
    int i = 0;
    for(i=0; i < strlen(s); i++)
    {
        if (s[i] < 32 || s[i] > 165)
		{
			s[i] = '?';
			//return false;
		}
	}  

	return true;
}

char *get_name_from_banner_buffer(u8 *buffer)
{
	char *out;
	u32 length;
	u32 i = 0;
	while (buffer[0x21 + i*2] != 0x00)
	{
		i++;
	}
	length = i;
	out = malloc(length+12);
	if(out == NULL)
	{
		Print("ERR: (get_name_from_banner_buffer) Allocating mem. failed\n");
		return NULL;
	}
	memset(out, 0x00, length+12);
	
	i = 0;
	while (buffer[0x21 + i*2] != 0x00)
	{
		out[i] = (char) buffer[0x21 + i*2];
		i++;
	}
	return out;
}

char *read_name_from_banner_app(u64 titleid)
{
    s32 ret;
	u32 num;
	dirent_t *list = NULL;
    char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
    u32 cnt = 0;
	u8 *buffer = allocate_memory(368);
	if (buffer == NULL)
	{
		Print("ERR: (read_name_from_banner_app) Allocating mem. failed\n");
		return NULL;
	}
   
	sprintf(path, "/title/%08x/%08x/content", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
	
    ret = getdir(path, &list, &num);
    if (ret < 0)
	{
		Print("ERR: (read_name_from_banner_app) Reading folder of the title failed\n");
		free(buffer);
		return NULL;
	}
	
	u8 imet[4] = {0x49, 0x4D, 0x45, 0x54};
	for (cnt=0; cnt < num; cnt++)
    {        
        if (strstr(list[cnt].name, ".app") != NULL || strstr(list[cnt].name, ".APP") != NULL) 
        {
            sprintf(path, "/title/%08x/%08x/content/%s", TITLE_UPPER(titleid), TITLE_LOWER(titleid), list[cnt].name);
  
            ret = read_file_from_nand(path, buffer, 368);
	        if (ret < 0)
	        {
		        // Error is printed in read_file_from_nand already
				continue;
	        }

			if (memcmp((buffer+0x80), imet, 4) == 0)
			{
				char *out = get_name_from_banner_buffer((void *)((u32)buffer+208));
				if (out == NULL)
				{
					free(buffer);
					free(list);
					return NULL;
				}
				
				free(buffer);
				free(list);
				
				return out;
			}   
        }
    }
	
	free(buffer);
	free(list);
	
	return NULL;
}

char *read_name_from_banner_bin(u64 titleid)
{
    s32 ret;
    char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	u8 *buffer = allocate_memory(160);
	if (buffer == NULL)
	{
		Print("ERR: (read_name_from_banner_bin) Allocating memory for buffer failed\n");
		return NULL;
	}
   
	sprintf(path, "/title/%08x/%08x/data/banner.bin", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
  
	ret = read_file_from_nand(path, buffer, 160);
    if (ret < 0)
    {
        // Error is printed in read_file_from_nand already
		free(buffer);
		return NULL;
	}

	char *out = get_name_from_banner_buffer(buffer);
	if (out == NULL)
	{
		free(buffer);
		return NULL;
	}
	
	free(buffer);

	return out;		
}

char *get_name(u64 titleid)
{
	char *temp;
	u32 low;
	low = TITLE_LOWER(titleid);

	temp = read_name_from_banner_bin(titleid);
	if (temp == NULL)
	{
		temp = read_name_from_banner_app(titleid);
	}
	
	if (temp != NULL)
	{
		check_text(temp);

		if (*(char *)&low == 'W')
		{
			return temp;
		}
		switch(low & 0xFF)
		{
			case 'E':
				memcpy(temp+strlen(temp), " (NTSC-U)", 9);
				break;
			case 'P':
				memcpy(temp+strlen(temp), " (PAL)", 6);
				break;
			case 'J':
				memcpy(temp+strlen(temp), " (NTSC-J)", 9);
				break;	
			case 'L':
				memcpy(temp+strlen(temp), " (PAL)", 6);
				break;	
			case 'N':
				memcpy(temp+strlen(temp), " (NTSC-U)", 9);
				break;		
			case 'M':
				memcpy(temp+strlen(temp), " (PAL)", 6);
				break;
			case 'K':
				memcpy(temp+strlen(temp), " (NTSC)", 7);
				break;
			default:
				break;				
		}
	}
	
	if (temp == NULL)
	{
		temp = malloc(6);
		memset(temp, 0, 6);
		memcpy(temp, (char *)(&low), 4);
	}
	
	return temp;
}
