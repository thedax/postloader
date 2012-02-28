#include <gccore.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "mystring.h"

/* not case sensible functions */

void ms_strtoupper(char *str1)
	{
	if (!str1) return;
	
	Debug ("ms_strtoupper (in): '%s'", str1);
		
	int i;
	for (i = 0; i < strlen(str1); i++) str1[i] = (char)toupper((int)str1[i]);
	
	Debug ("ms_strtoupper(out): '%s'", str1);
	}

char *ms_strstr(char *str1, char *str2)
	{
	if (!str1 || !str2 || !*str1 || !*str2)
		return NULL;
		
	int l1 = strlen(str1);
	int l2 = strlen(str2);
	
	char *s1 = malloc (l1+1);
	char *s2 = malloc (l2+1);

	int i;
	for (i = 0; i <= l1; i++) s1[i] = toupper((int)str1[i]);
	for (i = 0; i <= l2; i++) s2[i] = toupper((int)str2[i]);
	
	char *p;
	p = strstr (s1, s2);
	
	if (p != NULL)
		p = str1 + (p - s1);
	
	free (s1);
	free (s2);
	
	return p;
	}

int ms_strcmp(char *str1, char *str2)
	{
	if (!str1 || !str2 || !*str1 || !*str2)
		return 0;
		
	int l1 = strlen(str1);
	int l2 = strlen(str2);

	char *s1 = malloc (l1+1);
	char *s2 = malloc (l2+1);

	int i;
	for (i = 0; i <= l1; i++) s1[i] = toupper((int)str1[i]);
	for (i = 0; i <= l2; i++) s2[i] = toupper((int)str2[i]);
	
	int ret;
	ret = strcmp (s1, s2);
	
	free (s1);
	free (s2);
	
	return ret;
	}

// Very simple wrapper for some utf8
char *ms_utf8_to_ascii (char *string)
	{
	char *buff = calloc (1, strlen(string));
	
	int i;
	int j = 0;
	
	for (i = 0; i < strlen(string); i++)
		{
		if (string[i] != 0xC3)
			{
			buff[j] = string[i];
			}
		else
			{
			i++;
			char c = '?';
			
			if (string[i] >= 0xA8 && string[i] <= 0xAB) c = 'e';
			if (string[i] >= 0xA0 && string[i] <= 0xA6) c = 'a';
			if (string[i] >= 0xAC && string[i] <= 0xAF) c = 'i';
			if (string[i] >= 0xB2 && string[i] <= 0xB6) c = 'o';

			buff[j] = c;
			}
		j++;
		}
 
	return buff;
	}
	
u8 *ms_FindStringInBuffer (u8 *buffer, size_t size, char *string)
	{
	int stringLen = strlen (string);
	
	if (size < stringLen) return NULL;
	
	int i, j;
	for (i = 0; i < size-stringLen; i++)
		{
		for (j = 0; j < stringLen; j++)
			{
			if (buffer[i + j] != string[j]) break;
			}
		if (j == stringLen) return buffer + i;
		}
	
	return NULL;
	}