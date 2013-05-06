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

//#define Debug printf
#define MAX_INIT_STRING 4096

union uTypes
{
    char            Char;
    unsigned char   UChar;

	int             Int;
	unsigned int    UInt;
	
    short           Short;
    unsigned short  UShort;

    long      		Long;
	unsigned long   ULong;
	
	u32				U32;
	u16				U16;
	u8 				U8;
	s8 				S8;
	bool			Bool;

	float           Float;
    double          Double;
};

static char sect[300];
static char sep[8] = {'|', 0};

/*
void Debug (char *string, ...)
	{
    char tmp[1024];

    va_list argp;
    va_start(argp, text);
    vsprintf(tmp, text, argp);
    va_end(argp);
	}
*/

char *cfg_EncodeString (char *source) // basically remove/add \n\r, source buffer must contain enought space
	{
	int i;
	int inlen = strlen(source);
	char *buff;
	char *p;
	
	if (inlen < 2) return source;
	
	buff = malloc ((inlen * 2)+1);
	if (!buff) return source;
	
	i = 0;
	p = source;
	do
		{
		if (*p == '\r' && *(p+1) == '\n') 
			{
			buff[i++] = '\\';
			buff[i++] = 'n';
			p += 2;
			}
		else if (*p == '\n')
			{
			buff[i++] = '\\';
			buff[i++] = 'n';
			p ++;
			}
		else
			{
			buff[i++] = *p;
			p ++;
			}
		}
	while (*p);
	
	buff[i++] = 0;
	
	strcpy (source, buff);
	free (buff);
	
	return source;		
	}

char *cfg_DecodeString (char *source) // reverse
	{
	int i;
	int inlen = strlen(source);
	char *buff;
	char *p = source;
	
	if (inlen < 2) return source;
	
	buff = malloc (inlen+1);
	if (!buff) return source;

	i = 0;
	p = source;
	do
		{
		if (*p == '\\' && *(p+1) == 'r') 
			{
			buff[i++] = '\r';
			p += 2;
			}
		else if (*p == '\\' && *(p+1) == 'n') 
			{
			buff[i++] = '\n';
			p += 2;
			}
		else
			{
			buff[i++] = *p;
			p++;
			}
		}
	while (*p);
	
	buff[i++] = 0;
	
	strcpy (source, buff);
	free (buff);
	
	return source;		
	}

char *cfg_FindInBuffer (char *buff, char *tag)
	{
	static char item[1024];
	char t[256];
	char *p = buff;
	int i = 0;
	
	if (!buff) return NULL;
	if (!strlen(tag)) return NULL;

	do
		{
		sprintf (t, "%s=", tag);
		p = strstr (buff, t);
		
		if (p == NULL)
			{
			sprintf (t, "%s =", tag);
			p = strstr (buff, t);
			}

		if (p != NULL)
			{
			p += strlen(t);
			if (*p != 0 && *p == ' ') p++;
			if (*p == 0) return NULL;
			
			//copy item
			do
				{
				item[i++] = *(p++);
				}
			while (*p != 0 && *p != 10 && *p != 13);
			item[i++] = 0;
			
			return item;
			}
		}
	while (p != NULL && *p != 0);
	return NULL;
	}
	
void cfg_TrimFPString (char *numstring)
	{
	char *p;
	int l = strlen (numstring);
	
	if (l < 1) return;
	
	p = strstr (numstring, ".");
	if (!p) return;
	
	p = &numstring[l-1];
	do
		{
		if (*p == '0') 
			{
			*p = '\0';
			p--;
			}
		else
			return;
		}
	while (p > numstring);
	}

int cfg_Section (char *section)
	{
	if (section == NULL) 
		*sect = '\0';
	else
		strcpy (sect, section);
	
	return 1;
	}

void cfg_Free (s_cfg *c) // Free allocated config. stuct
	{
	int i;
	
	for (i = 0; i < c->count; i++)
		{
		if (c->items[i]) free (c->items[i]);
		if (c->tags[i]) free (c->tags[i]);
		}
	
	free (c->items);
	free (c->tags);
	
	free (c);
	}

void cfg_Empty (s_cfg *c) // Free allocated config. stuct
	{
	int i;
	
	for (i = 0; i < c->count; i++)
		{
		if (c->items[i]) free (c->items[i]);
		if (c->tags[i]) free (c->tags[i]);
		
		c->items[i] = NULL;
		c->tags[i] = NULL;
		}
		
	c->count = 0;	
	}


bool cfg_Store (s_cfg *c, char *fn)
	{
	int i;
	FILE *f;
	char *fbuff;
	char *p;
	int filesize = 0;
	
	for (i = 0; i < c->count; i++)
		{
		filesize += strlen(c->tags[i]) + strlen (c->items[i]) + 2;
		}
	
	if (!filesize) 
		{
		return false;
		}
	
	fbuff = malloc (filesize + 1);
	
	*fbuff = '\0';
	p = fbuff;
	for (i = 0; i < c->count; i++)
		{
		strcpy (p, c->tags[i]);
		p += strlen (c->tags[i]);
		
		strcpy (p, "=");
		p ++;
		
		strcpy (p, c->items[i]);
		p += strlen (c->items[i]);

		strcpy (p, "\n");
		p ++;
		}
	
	f = fopen (fn, "wb");
	if (!f) 
		{
		free (fbuff);
		return false;
		}
	fwrite (fbuff, 1, strlen(fbuff), f);
	fclose (f);
	
	free (fbuff);
	
	return true;
	}

// If fn is null, it return an empty structure, otherwise... if maxcount is 0, one day, it will allocate the right number of lines, skipinvalid=1 will read only valid tags (skips comments, blabla)
s_cfg *cfg_Alloc (char *fn, int maxcount, int linebuffsize, int skipinvalid)
	{
	FILE *f;
	int filesize;
	char *fbuff;

	s_cfg *cfg;
	char *line;
	char *p, *pp;
	int i, linelen;
	
	cfg = calloc (1, sizeof (s_cfg));
	if (maxcount == 0)
		cfg->maxcount = 1024;
	else
		cfg->maxcount = maxcount;
	
	cfg->items = calloc (cfg->maxcount, sizeof(char*));
	cfg->tags = calloc (cfg->maxcount, sizeof(char*));
	
	if (fn == NULL) return cfg;
	
	// read the file in memory
	f = fopen (fn, "rb");
	if (!f) return cfg;
	
	fseek (f, 0, SEEK_END);
	filesize = ftell (f);
	fseek (f, 0, SEEK_SET);
	fbuff = malloc (filesize+1);
	fread (fbuff, 1, filesize, f);
	fbuff[filesize] = 0;
	fclose (f);
	
	// scan and count how many lines we have
	for (i = 0; i < filesize; i++)
		if (fbuff[i] == '\n')
			fbuff[i] = 0;
	
	i = 0;
	line = fbuff;
	while (*line != 0)
		{
		linelen = strlen(line);
		
		p = line + linelen - 1;
		while (p > line && (*p == '\32' || *p == 13 || *p == 10)) 
			{
			*p = '\0';
			p--; // remove spaces/cr/lf
			}
		
		p = strstr (line, "=");
		if (!skipinvalid && (line[0] == '#' || p == NULL)) // It is a comment or badly formatted line, just use the tag....
			{
			cfg->tags[i] = calloc (1, strlen (line) + 1);
			strcpy (cfg->tags[i], line);
			
			cfg->items[i] = NULL;
			
			i++;
			}
		else if (p != NULL && line[0] != '#')
			{
			pp = p;
			
			while (p > line && *(p-1) == ' ') p--; // remove spaces
			*p = '\0';
			cfg->tags[i] = calloc (1, strlen (line) + 1);
			strcpy (cfg->tags[i], line);
			
			p = pp + 1;
			while (*p == ' ') p++; // remove spaces
			cfg->items[i] = calloc (1, strlen (p) + 1);
			strcpy (cfg->items[i], p);
			
			i++;
			//Debug ("Adding %s=%s", c->tag, c->value);
			}

		line += (linelen+1);
		}
	
	cfg->count = i;
	
	free (fbuff);
	
	//Debug ("cfg_Alloc: completed");
	
	return cfg;
	}

int cfg_FindTag (s_cfg *c, char *tag) // return the index to an item
	{
	int i;
	
	for (i = 0; i < c->count; i++)
		{
		if (c->tags[i] && strcmp (c->tags[i], tag) == 0) return i;
		}
	
	return -1;
	}

bool cfg_RemoveTag (s_cfg *cfg, char *tag)
	{
	
	return true;
	}
	

int cfg_SetString (s_cfg *cfg, char *tag, char *string)
	{
	int i;
	
	i = cfg_FindTag (cfg, tag);
	
	if (i < 0) // add new item
		{
		i = cfg->count;
		cfg->count++;

		cfg->tags[i] = calloc (1, strlen (tag) + 1);
		strcpy (cfg->tags[i], tag);
		}

	if (cfg->items[i]) free (cfg->items[i]);
	cfg->items[i] = calloc (1, strlen (string) + 1);
	strcpy (cfg->items[i], string);
	
	return i;
	}

int cfg_GetString (s_cfg *cfg, char *tag, char *string)
	{
	int i;
	
	*string = '\0';
	
	i = cfg_FindTag (cfg, tag);
	
	if (i < 0) return i;

	strcpy (string, cfg->items[i]);
	return i;
	}


int cfg_Value (s_cfg *cfg, int mode, int type, char *item, void *data, int maxbytes)
	{
	int ret = 0;
	union uTypes ut;
	static char string[MAX_INIT_STRING];
	static char tag[256];
	
	if (data == NULL) return 0;
	
	if (strlen(sect))
		sprintf (tag, "%s.%s", sect, item);
	else
		sprintf (tag, "%s", item);
	
	if (mode == CFG_WRITE)
		{
		if (type == CFG_BOOL)
			{
			memcpy (&ut.Bool, data, sizeof(bool));
			sprintf (string, "%d", ut.Bool);
			}
		if (type == CFG_CHAR)
			{
			memcpy (&ut.Char, data, sizeof(char));
			sprintf (string, "%d", ut.Char);
			}
		if (type == CFG_UCHAR)
			{
			memcpy (&ut.UChar, data, sizeof(unsigned char));
			sprintf(string, "%u", ut.UChar);
			}
		if (type == CFG_INT)
			{
			memcpy (&ut.Int, data, sizeof(int));
			sprintf(string, "%d", ut.Int);
			}
		if (type == CFG_SHORT)
			{
			memcpy (&ut.Short, data, sizeof(short));
			sprintf(string, "%d", ut.Short);
			}
		if (type == CFG_UINT)
			{
			memcpy (&ut.UInt, data, sizeof(unsigned int));
			sprintf(string, "%u", ut.UInt);
			}
		if (type == CFG_U32)
			{
			memcpy (&ut.U32, data, sizeof(u32));
			sprintf(string, "%u", ut.U32);
			}
		if (type == CFG_U16)
			{
			memcpy (&ut.U16, data, sizeof(u16));
			sprintf(string, "%u", ut.U16);
			}
		if (type == CFG_U8)
			{
			memcpy (&ut.U8, data, sizeof(u8));
			sprintf(string, "%u", ut.U8);
			}
		if (type == CFG_S8)
			{
			memcpy (&ut.S8, data, sizeof(s8));
			sprintf(string, "%d", ut.S8);
			}
		if (type == CFG_LONG)
			{
			memcpy (&ut.Long, data, sizeof(long));
			sprintf(string, "%ld", ut.Long);
			}
		if (type == CFG_DOUBLE)
			{
			memcpy (&ut.Double, data, sizeof(double));
			sprintf(string, "%.10f", ut.Double);
			cfg_TrimFPString (string);
			}
		if (type == CFG_FLOAT)
			{
			memcpy (&ut.Float, data, sizeof(float));
			sprintf(string, "%f", ut.Float);
			cfg_TrimFPString (string);
			}
		if (type == CFG_STRING)
			{
			strcpy (string, (char*) data);
			}
		if (type == CFG_ENCSTRING)
			{
			strcpy (string, (char*) data);
			cfg_EncodeString (string);
			}
		
		return cfg_SetString (cfg, tag, string); 
		}

	if (mode == CFG_READ)
		{
		ret = cfg_GetString (cfg, tag, string);

		if (type == CFG_BOOL)
			{
			ut.Bool = (char)atoi(string);
			memcpy (data, &ut.Bool, sizeof(bool));
			return ret;
			}
		if (type == CFG_CHAR)
			{
			ut.Char = (char)atoi(string);
			memcpy (data, &ut.Char, sizeof(char));
			return ret;
			}
		if (type == CFG_UCHAR)
			{
			ut.UChar = (unsigned char)atoi(string);
			memcpy (data, &ut.UChar, sizeof(unsigned char));
			return ret;
			}
		if (type == CFG_INT)
			{
			ut.Int = atoi(string);
			memcpy (data, &ut.Int, sizeof(int));
			return ret;
			}
		if (type == CFG_SHORT)
			{
			ut.Short = atoi(string);
			memcpy (data, &ut.Short, sizeof(short));
			return ret;
			}
		if (type == CFG_UINT)
			{
			ut.UInt = atoi(string);
			memcpy (data, &ut.UInt, sizeof(int));
			return ret;
			}
		if (type == CFG_U32)
			{
			ut.U32 = (u32)atoi(string);
			memcpy (data, &ut.U32, sizeof(u32));
			return ret;
			}
		if (type == CFG_U16)
			{
			ut.U16 = (u16)atoi(string);
			memcpy (data, &ut.U16, sizeof(u16));
			return ret;
			}
		if (type == CFG_U8)
			{
			ut.U8 = (u8)atoi(string);
			memcpy (data, &ut.U8, sizeof(u8));
			return ret;
			}
		if (type == CFG_S8)
			{
			ut.S8 = (s8)atoi(string);
			memcpy (data, &ut.S8, sizeof(s8));
			return ret;
			}
		if (type == CFG_LONG)
			{
			ut.Long = atol(string);
			memcpy (data, &ut.Long, sizeof(long));
			return ret;
			}
		if (type == CFG_DOUBLE)
			{
			ut.Double = atof(string);
			memcpy (data, &ut.Double, sizeof(double));
			return ret;
			}
		if (type == CFG_FLOAT)
			{
			ut.Float = atof(string);
			memcpy (data, &ut.Float, sizeof(float));
			return ret;
			}
		if (type == CFG_STRING)
			{
			if (maxbytes <= 0)
				strcpy (data, string);
			else
				memcpy (data, string, maxbytes);
			return ret;
			}
		if (type == CFG_ENCSTRING)
			{
			cfg_DecodeString (string);
		
			if (maxbytes <= 0)
				strcpy (data, string);
			else
				{
				memcpy (data, string, maxbytes);
				char*p = data;
				p[maxbytes-1] = 0;
				}
				
			return ret;
			}
		}

	return ret;
	}

// helper function
void cfg_CatFmtString (char *buff, int type, void *data)
	{
	union uTypes ut;
	char string[MAX_INIT_STRING];
	
	if (buff == NULL || data == NULL) 
		{
		strcat (buff, sep);
		return;
		}

	if (type == CFG_CHAR)
		{
		memcpy (&ut.Char, data, sizeof(char));
		sprintf (string, "%d", ut.Char);
		}
	if (type == CFG_UCHAR)
		{
		memcpy (&ut.UChar, data, sizeof(unsigned char));
		sprintf(string, "%u", ut.UChar);
		}
	if (type == CFG_INT)
		{
		memcpy (&ut.Int, data, sizeof(int));
		sprintf(string, "%d", ut.Int);
		}
	if (type == CFG_SHORT)
		{
		memcpy (&ut.Short, data, sizeof(short));
		sprintf(string, "%d", ut.Short);
		}
	if (type == CFG_UINT)
		{
		memcpy (&ut.UInt, data, sizeof(unsigned int));
		sprintf(string, "%u", ut.UInt);
		}
	if (type == CFG_U32)
		{
		memcpy (&ut.U32, data, sizeof(u32));
		sprintf(string, "%u", ut.U32);
		}
	if (type == CFG_U16)
		{
		memcpy (&ut.U16, data, sizeof(u16));
		sprintf(string, "%u", ut.U16);
		}
	if (type == CFG_U8)
		{
		memcpy (&ut.U8, data, sizeof(u8));
		sprintf(string, "%u", ut.U8);
		}
	if (type == CFG_S8)
		{
		memcpy (&ut.S8, data, sizeof(s8));
		sprintf(string, "%d", ut.S8);
		}
	if (type == CFG_LONG)
		{
		memcpy (&ut.Long, data, sizeof(long));
		sprintf(string, "%ld", ut.Long);
		}
	if (type == CFG_DOUBLE)
		{
		memcpy (&ut.Double, data, sizeof(double));
		sprintf(string, "%.10f", ut.Double);
		cfg_TrimFPString (string);
		}
	if (type == CFG_FLOAT)
		{
		memcpy (&ut.Float, data, sizeof(float));
		sprintf(string, "%f", ut.Float);
		cfg_TrimFPString (string);
		}
	if (type == CFG_STRING)
		{
		strcpy (string, (char*) data);
		}
	if (type == CFG_ENCSTRING)
		{
		strcpy (string, (char*) data);
		cfg_EncodeString (string);
		}

	strcat (buff, string);
	strcat (buff, sep);
	}

int cfg_CountSepString (char *buff)
	{
	char *p = buff;
	
	int i = 0;
	while (true)
		{
		p = strstr (p, sep);
		if (!p) return i;
		p += strlen (sep);
		i ++;
		}
	
	return 0;
	}

bool cfg_GetFmtString (char *buff, int type, void *data, int index)
	{
	union uTypes ut;
	char string[MAX_INIT_STRING];

	int ret = true;
	
	int i;
	char *ns = NULL; // nexsep
	char *p = buff;
	
	for (i = 0; i < index; i++)
		{
		p = strstr (p, sep);
		if (!p) return false;
		p += strlen (sep);
		}
		
	i = 0;
	if (*p != sep[0])
		{
		ns = strstr (p, sep);
		do
			{
			string[i++] = *(p++);
			}
		while (*p != 0 && (ns != NULL && p < ns));
		}
	string[i++] = 0;
	
	if (type == CFG_CHAR)
		{
		ut.Char = (char)atoi(string);
		memcpy (data, &ut.Char, sizeof(char));
		return ret;
		}
	if (type == CFG_UCHAR)
		{
		ut.UChar = (unsigned char)atoi(string);
		memcpy (data, &ut.UChar, sizeof(unsigned char));
		return ret;
		}
	if (type == CFG_INT)
		{
		ut.Int = atoi(string);
		memcpy (data, &ut.Int, sizeof(int));
		return ret;
		}
	if (type == CFG_SHORT)
		{
		ut.Short = atoi(string);
		memcpy (data, &ut.Short, sizeof(short));
		return ret;
		}
	if (type == CFG_UINT)
		{
		ut.Int = atoi(string);
		memcpy (data, &ut.Int, sizeof(int));
		return ret;
		}
	if (type == CFG_U32)
		{
		ut.U32 = (u32)atoi(string);
		memcpy (data, &ut.U32, sizeof(u32));
		return ret;
		}
	if (type == CFG_U16)
		{
		ut.U16 = (u16)atoi(string);
		memcpy (data, &ut.U16, sizeof(u16));
		return ret;
		}
	if (type == CFG_U8)
		{
		ut.U8 = (u8)atoi(string);
		memcpy (data, &ut.U8, sizeof(u8));
		return ret;
		}
	if (type == CFG_S8)
		{
		ut.S8 = (s8)atoi(string);
		memcpy (data, &ut.S8, sizeof(s8));
		return ret;
		}
	if (type == CFG_LONG)
		{
		ut.Long = atol(string);
		memcpy (data, &ut.Long, sizeof(long));
		return ret;
		}
	if (type == CFG_DOUBLE)
		{
		ut.Double = atof(string);
		memcpy (data, &ut.Double, sizeof(double));
		return ret;
		}
	if (type == CFG_FLOAT)
		{
		ut.Float = atof(string);
		memcpy (data, &ut.Float, sizeof(float));
		return ret;
		}
	if (type == CFG_STRING)
		{
		strcpy (data, string);
		return ret;
		}
	if (type == CFG_ENCSTRING)
		{
		cfg_EncodeString (string);
		strcpy (data, string);
		return ret;
		}
	
	return false;
	}


bool cfg_FmtString (char *buff, int mode, int type, void *data, int index)
	{
	if (mode == CFG_WRITE)
		{
		cfg_CatFmtString (buff, type, data);
		return true;
		}
	else
		{
		return cfg_GetFmtString (buff, type, data, index);
		}
	}


int cfg_ValueArray (s_cfg *cfg, int mode, int type, char *item, int idx, void *data, int maxbytes)
	{
	char buff[300];
	
	sprintf (buff, "%s[%d]", item, idx);
	return cfg_Value (cfg, mode, type, buff, data, maxbytes);
	}

char* cfg_TagFromIndex (s_cfg *cfg, int index, char *tag)
	{
	static char buff[256]; // should be enught
	
	if (index < 0 || index >= cfg->maxcount || !cfg->tags[index])
		{
		if (tag) *tag = '\0';
		return NULL;
		}
		
	if (tag) strcpy (tag, cfg->tags[index]);
	strcpy (buff, cfg->tags[index]);
	
	return buff;
	}
