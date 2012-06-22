#ifndef _CFG_
	#define _CFG_
	typedef struct cfg
		{
		char *tag;	// if tag is null, value is probably a comment or empty line.
		char *value;
		
		bool comment;
		
		struct cfg *next;
		}
	s_cfg;

	s_cfg *cfg_Alloc (char *fn, int linebuffsize); // If fn is null, it return an empty structure, otherwise...
	bool cfg_Store (s_cfg *c, char *fn);
	s_cfg *cfg_NextItem (s_cfg *c);	// return next item in list
	void cfg_Free (s_cfg *c); // Free allocated config. stuct
	bool cfg_GetInt (s_cfg *cfg, char *tag, int *ival);
	bool cfg_GetString (s_cfg *cfg, char *tag, char *string);
	bool cfg_SetString (s_cfg *cfg, char *tag, char *string);
	s_cfg *cfg_GetItemFromIndex (s_cfg *c, int idx); // return the pointer to item #
	
	void cfg_Debug (s_cfg *c); // Debug an item tree
#endif