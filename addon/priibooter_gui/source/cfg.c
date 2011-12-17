#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cfg.h"

/////////////////////////////////////////////////////////////////////
//
// cfg buffer managment routines
//
/////////////////////////////////////////////////////////////////////

char *cfg_FindTag (char *buff, char *tag)
	{
	char b[300];
	char *p;
	
	sprintf (b, "%s=", tag);
	p = strstr (buff, b);
	
	if (p == NULL)
		{
		sprintf (b, "%s =", tag);
		p = strstr (buff, b);
		}
	
	if (p == NULL) return NULL;
	
	// move on value
	p += strlen (b);
	
	// strip spaces
	while (*p == ' ' && *p != '\0') p++;
	if (*p == '\0') return NULL;
	
	// we should have a value
	return p;
	}
	
int cfg_GetString (char *buff, char *tag, char *string)
	{
	char *p;
	char *ps;
	
	p = cfg_FindTag (buff, tag);
	if (p == NULL) 
		{
		*string = '\0';
		return 0;
		}
		
	ps = string;
	
	while (*p != '\0' && *p != '\n')
		{
		*ps++ = *p++;
		}
	*ps = '\0';
	
	return 1;
	}
	
int cfg_GetInt (char *buff, char *tag, int *ival)
	{
	char *p;
	
	p = cfg_FindTag (buff, tag);
	if (p == NULL) 
		{
		*ival = 0;
		return 0;
		}
	*ival = atoi (p);
	
	return 1;
	}
