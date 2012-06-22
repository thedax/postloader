/////////////////////////////////////////////////////////////////////
// Functions to manage in-memory configuration files
/////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ogcsys.h>
#include "cfg.h"
#include "debug.h"

s_cfg *cfg_AddNode (s_cfg *c) // Add a new empty node
	{
	if (!c->tag && !c->value) return c;
	
	// Make sure to write to the last itme.	
	while (c->next)
		{
		c = c->next;
		}

	// Allocate space
	c->next = calloc (1, sizeof (s_cfg));

	return c->next;
	}

void cfg_EmptyNode (s_cfg *c) // Free allocated config. stuct
	{
	if (c->tag)	free (c->tag);
	if (c->value) free (c->value);
	
	c->tag = NULL;
	c->value = NULL;
	}
	
s_cfg *cfg_NextItem (s_cfg *c)	// return next item in list
	{
	return c->next;
	}

void cfg_Debug (s_cfg *c) // Debug an item
	{
	while (c)
		{
		if (c->tag)	  Debug ("  tag[0x%X] = '%s'", c, c->tag);
		if (c->value) Debug ("value[0x%X] = '%s'", c, c->value);
		if (c->comment)	
			          Debug ("  tag[0x%X] = COMMENT", c);
		else
					  Debug ("  tag[0x%X] = KEY", c);
			
		c = c->next;
		}
	}

void cfg_Free (s_cfg *c) // Free allocated config. stuct
	{
	if (!c) return;
	
	s_cfg *next;
	
	do
		{
		if (c->tag)	free (c->tag);
		if (c->value) free (c->value);
		next = c->next;
		
		free (c);
		
		c = next;
		}
	while (c);
	}

bool cfg_Store (s_cfg *c, char *fn)
	{
	FILE *f;
	
	//cfg_Debug (c);

	Debug ("cfg_Store: %s (0x%X)", fn, c);

	f = fopen (fn, "wb");
	
	Debug ("cfg_Store: fopen 0x%X", f);	
	
	if (!f) return false;
	
	do
		{
		if (!c->comment && c->value && c->tag)	
			fprintf (f, "%s = %s\n", c->tag, c->value);
		else if (c->value)
			fprintf (f, "%s\n", c->value);
			
		c = c->next;
		}
	while (c);
	
	fclose (f);
	
	return true;
	}

s_cfg *cfg_Alloc (char *fn, int linebuffsize) // If fn is null, it return an empty structure, otherwise...
	{
	s_cfg *c, *base;
	
	base = calloc (1, sizeof (s_cfg));
	
	if (fn == NULL) return base;
	
	Debug ("cfg_Alloc (%s, %d)", fn, linebuffsize);
	
	if (linebuffsize == 0) linebuffsize = 1024;

	FILE *f;
	char *line = calloc (1, linebuffsize);
	char *p, *pp;
	bool addnew = false;
	
	f = fopen (fn, "rb");
	if (!f) return base;
	
	Debug ("cfg_Alloc: open ok");
	
	c = base;
	while (fgets (line, linebuffsize, f))
		{
		//Debug ("cfg_Alloc: (rd) %s", line);
		
		p = line + strlen(line) - 1;
		while (p > line && (*(p-1) == '\32' || *(p-1) == 13 || *(p-1) == 10)) p--; // remove spaces/cr/lf
		*p = '\0';

		if (addnew)
			c = cfg_AddNode (c);
			
		p = strstr (line, "=");
		if (line[0] == '#' || p == NULL) // It is a comment or badly formatted line
			{
			c->comment = true;
			c->value = calloc (1, strlen (line) + 1);
			strcpy (c->value, line);
			}
		else
			{
			pp = p;
			
			while (p > line && *(p-1) == ' ') p--; // remove spaces
			*p = '\0';
			c->tag = calloc (1, strlen (line) + 1);
			strcpy (c->tag, line);
			
			p = pp + 1;
			while (*p == ' ') p++; // remove spaces
			c->value = calloc (1, strlen (p) + 1);
			strcpy (c->value, p);
			
			//Debug ("Adding %s=%s", c->tag, c->value);
			}
		
		addnew = true;
		}
	
	free (line);
	
	fclose (f);
	//cfg_Debug (base);
	
	Debug ("cfg_Alloc: completed");
	
	return base;
	}

s_cfg *cfg_FindTag (s_cfg *c, char *tag) // return the pointer to an item
	{
	do
		{
		if (c->tag && strcmp (c->tag, tag) == 0) break;
		c = c->next;
		}
	while (c);
	
	return c;
	}

bool cfg_RemoveTag (s_cfg *cfg, char *tag)
	{
	
	return true;
	}
	
bool cfg_SetString (s_cfg *cfg, char *tag, char *string)
	{
	s_cfg *c;
	
	c = cfg_FindTag (cfg, tag);
	
	if (c)
		{
		Debug ("cfg_SetString (found) %s = %s", tag, string);
		cfg_EmptyNode (c);
		}
	else
		{
		Debug ("cfg_SetString (newnode) %s = %s", tag, string);
		c = cfg_AddNode (cfg);
		}
		
	c->tag = calloc (1, strlen (tag) + 1);
	c->value = calloc (1, strlen (string) + 1);
	
	strcpy (c->tag, tag);
	strcpy (c->value, string);
	c->comment = false;
	
	return true;
	}

bool cfg_GetString (s_cfg *cfg, char *tag, char *string)
	{
	s_cfg *c;
	
	c = cfg_FindTag (cfg, tag);
	
	if (c)
		{
		strcpy (string, c->value);
		return true;
		}
	
	return false;
	}

bool cfg_GetInt (s_cfg *cfg, char *tag, int *ival)
	{
	s_cfg *c;
	
	c = cfg_FindTag (cfg, tag);
	
	if (c)
		{
		*ival = atoi (c->value);
		return true;
		}
	
	return false;
	}

s_cfg *cfg_GetItemFromIndex (s_cfg *c, int idx) // return the pointer to item #
	{
	int i = 0;
	
	do
		{
		//Debug ("cfg_GetItemFromIndex (%d:%s:%s)", i, c->tag, c->value);
		if (c->tag)
			{
			if (idx == i) return c;
			i ++;
			}
		c = c->next;
		}
	while (c);
	
	return NULL;
	}

