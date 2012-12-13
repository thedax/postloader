/* 
language.c

simple language handling routine. This is the format.

{tag1}string1\n
{tag2}string2\n
etc...
*/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define MAXSTRINGS 4
#define MAXSTRINGLEN 512

static char *langBuff = 0;
static char string[MAXSTRINGS][MAXSTRINGLEN];
static int stringIdx = 0;

void SetLangFileBuffer (const char *buff, int size)
	{
	memset (string, 0, sizeof(string));
	stringIdx = 0;
	langBuff = (char*)buff;
	langBuff[size-1] = 0; // make sure it is null terminated
	}
	
char *GetLanguageString (char *tag)
	{
	int i;
	char fulltag[64];
	char *p;
	
	sprintf (fulltag, "{%s}", tag);
	p = strstr(langBuff, fulltag);
	
	stringIdx++;
	if (stringIdx >= MAXSTRINGS) stringIdx = 0;

	if (p == NULL) // not found... copy back the tag
		{
		strcpy (string[stringIdx], tag);
		}
	else
		{
		p += strlen(fulltag);
		i = 0;
		while (*p != '\r' && *p != '\n' && *p != 0 && i < (MAXSTRINGLEN-1))
			{
			if (*p == '\\' && *(p+1) == 'n')
				{
				string[stringIdx][i++] = '\n';
				p++;
				}
			else
				string[stringIdx][i++] = *p;
			p++;
			}
		string[stringIdx][i++] = 0;
		}
		
	return string[stringIdx];
	}