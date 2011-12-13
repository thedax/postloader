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

int find_string_in_buffer (u8 *buff, int buffsize, char *string)
	{
	int i, l;
	
	l = strlen(string);
	for (i = 0; i < buffsize-i; i++)
		{
		if (memcmp(&buff[i], string, l) == 0)
			return i;
		}
	return -1;
	}
	
// to identify title we should lock of 0x00 0x00 0x00 0x00 0x00 [Printable char]
char *find_title_name (u8 *buff, int buffsize)
	{
	char printable[] = "1234567890QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm ";
	char pattern[] = {'\0', '\0', '\0', '\0', '\0', '\0'};
	
	int printableLen = sizeof(printable);
	int patternLen = sizeof(pattern);
	int pid = patternLen-1;

	char *outbuff;
	char tempbuff[256];
	
	char *p, *eob;
	
	int start, j;
	
	start = find_string_in_buffer (buff, buffsize, "IMET");
	if (start < 0) return NULL;
	
	p = (char*)buff+start;
	eob = (char *) ((buff+buffsize) - (patternLen+2));

	while (p < eob)
		{
		for (j = 0; j < printableLen; j++)
			{
			if (printable[j])
				pattern[pid] = printable[j];
		
			if (memcmp(p, pattern, patternLen) == 0)
				{
				int pos, err;
				
				// We have found something valid
				memset (tempbuff, 0, sizeof(tempbuff));
				
				p += patternLen-1;
				pos = 0;
				err = -10;
				
				while (p < eob)
					{
					if (*p == '\0' && *(p+1) == '\0')
						{
						err = 0;
						break;
						}
					if (*p < 32)
						{
						err = -1;
						break;
						}
					if (*(p+1) != '\0')
						{
						err = -2;
						break;
						}
					if (pos >= 255)
						{
						err = -3;
						break;
						}
					tempbuff[pos] = *p;
					p += 2;
					pos++;
					}
				/*
				if (err != 0)
					{
					sprintf (tempbuff, "%s: %d, %d, %d, %d, %d, %d", tempbuff, pattern[pid], pid, p-(char*)buff, start, pos, err);
					pos = strlen(tempbuff);
					err = 0;
					}
				*/
				if (pos > 3 && err == 0) 
					{
					outbuff = malloc (pos + 16);
					
					if (outbuff)
						strcpy (outbuff, tempbuff);
					
					return outbuff;
					}
				
				//return NULL;
				}
			}
		p++;
		}
	
	return NULL;
	/*
	outbuff = malloc (256);
	strcpy (outbuff, "notfound");
	return outbuff;
	*/
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
		return NULL;
		}
   
	sprintf(path, "/title/%08x/%08x/content", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
	
    ret = getdir(path, &list, &num);
    if (ret < 0)
		{
		//grlib_dosm ("read_name_from_banner_app (getdir)=%d", ret);
		free(buffer);
		return NULL;
		}
	
	for (cnt=0; cnt < num; cnt++)
		{        
        if (strstr(list[cnt].name, ".app") != NULL || strstr(list[cnt].name, ".APP") != NULL) 
			{
            sprintf(path, "/title/%08x/%08x/content/%s", TITLE_UPPER(titleid), TITLE_LOWER(titleid), list[cnt].name);
  
            ret = read_file_from_nand(path, buffer, 368);
	        if (ret < 0)
				{
				//grlib_dosm ("read_name_from_banner_app (rffn)=%d\n%s (%d)", ret,path, strlen(path));
		        // Error is printed in read_file_from_nand already
				continue;
				}
			
			char * out = find_title_name (buffer, 368);
			if (out)
				{
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
		return NULL;
	}
   
	sprintf(path, "/title/%08x/%08x/data/banner.bin", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
  
	ret = read_file_from_nand(path, buffer, 160);
    if (ret < 0)
    {
		//grlib_dosm ("read_file_from_nand=%d", ret);
		
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
				strcat (temp," (NTSC-U)");
				break;
			case 'P':
				strcat (temp," (PAL)");
				break;
			case 'J':
				strcat (temp," (NTSC-J)");
				break;	
			case 'L':
				strcat (temp," (PAL)");
				break;	
			case 'N':
				strcat (temp," (NTSC-U)");
				break;		
			case 'M':
				strcat (temp," (PAL)");
				break;
			case 'K':
				strcat (temp," (NTSC)");
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
