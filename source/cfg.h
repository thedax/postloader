#ifndef _CFG_
	#define _CFG_

	#ifndef bool
		typedef unsigned int bool;
	#endif

	#ifndef u32
		typedef unsigned int u32;
	#endif

	#ifndef u16
		typedef unsigned short u16;
	#endif

	#ifndef u8
		typedef unsigned char u8;
	#endif

/*
	#ifndef s8
		typedef char s8;
	#endif
*/
	#ifndef false
		#define false 0
	#endif
	
	#ifndef true
		#define true 1
	#endif
	
	#define CFG_READ 0
	#define CFG_WRITE 1

	enum 
		{
		CFG_INT=0,
		CFG_UINT,
		CFG_DOUBLE,
		CFG_FLOAT,
		CFG_STRING,
		CFG_LONG,
		CFG_CHAR,
		CFG_UCHAR,
		CFG_SHORT,
		CFG_ENCSTRING,
		CFG_U32,
		CFG_U16,
		CFG_U8,
		CFG_S8
		};

	typedef struct cfg
		{
		char **tags;	// it will contain the data
		char **items;	// it will contain the data
		int count;
		int maxcount;		// allocated number of items
		}
	s_cfg;

	char *cfg_FindInBuffer (char *buff, char *tag);
	int cfg_Section (char *section);
	s_cfg *cfg_Alloc (char *fn, int maxcount, int linebuffsize, int skipinvalid); // If fn is null, it return an empty structure, otherwise...
	
	bool cfg_Store (s_cfg *c, char *fn);
	void cfg_Free (s_cfg *c); // Free allocated config. stuct
	void cfg_Empty (s_cfg *c);
	
	int cfg_FindTag (s_cfg *c, char *tag); // return the pointer to an item
	int cfg_GetString (s_cfg *cfg, char *tag, char *string);
	int cfg_SetString (s_cfg *cfg, char *tag, char *string);
	int cfg_CountSepString (char *buff);
	
	void cfg_CatFmtString (char *buff, int type, void *data);
	bool cfg_GetFmtString (char *buff, int type, void *data, int index);
	bool cfg_FmtString (char *buff, int mode, int type, void *data, int index);
	
	int cfg_Value (s_cfg *cfg, int mode, int type, char *item, void *data, int maxbytes);
	int cfg_ValueArray (s_cfg *cfg, int mode, int type, char *item, int idx, void *data, int maxbytes);
	
	char* cfg_TagFromIndex (s_cfg *cfg, int index, char *tag);
#endif
