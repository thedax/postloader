#include <gccore.h>
#include "globals.h"

/*
ioConfig manage the configuration file

TODO: use text configuration files... maybe
*/

bool ConfigWrite (void)
	{
	char path[PATHMAX];
	char ver[32];
	FILE *f;
	
	if (vars.defMount[0] == '\0') return FALSE;	
	
	if (vars.neek == NEEK_NONE)
		sprintf (path, "%s://ploader/ploader.cfg", vars.defMount);
	else
		sprintf (path, "%s://ploader/pldneek.cfg", vars.defMount);
	
	Debug ("ConfigWrite: %s", path);

	f = fopen (path, "wb");
	if (!f) return FALSE;
	
	// Let's write version
	sprintf (ver, "%s", CFGVER);
	fwrite (ver, sizeof(ver), 1, f);
	
	// Write configuration data
	fwrite (&config, sizeof(config), 1, f);
	fclose (f);
	
	return TRUE;
	}
	
bool ConfigRead (void)
	{
	char path[PATHMAX];
	char ver[32];
	FILE *f;
	
	if (vars.defMount[0] == '\0') return FALSE;
	
	if (vars.neek == NEEK_NONE)
		sprintf (path, "%s://ploader/ploader.cfg", vars.defMount);
	else
		sprintf (path, "%s://ploader/pldneek.cfg", vars.defMount);
	
	Debug ("ConfigRead: %s", path);

	f = fopen (path, "rb");
	if (!f) return FALSE;
	
	// read configuration data
	fread (ver, sizeof(ver), 1, f);
	ver[31] = 0; // make sure to terminate string
	
	if (strcmp (ver, CFGVER) != 0)
		memset (&config, 0, sizeof(config)); // configuration version has changed... clear data struct
	else
		fread (&config, sizeof(config), 1, f);
		
	fclose (f);
	
	return TRUE;
	}
	
int ManageTitleConfig (char *asciiId, int write, s_channelConfig *config)
	{
	int ret = 0;
	char cfg[256];
	FILE* f = NULL;
	
	sprintf (cfg, "%s://ploader/config/%s.cfg", vars.defMount, asciiId);
	
	if (write)
		{
		f = fopen(cfg, "wb");
		if (!f) return 0;
		
		ret = fwrite (config, 1, sizeof(s_channelConfig), f );
		fclose(f);
		}
	else
		{
		if (!fsop_FileExist (cfg)) // Create a default config
			{
			config->priority = 5;
			config->hidden = 0;
			
			config->ios = 0; 		// use current
			config->vmode = 0;
			config->language = -1;
			config->vpatch = 0;
			config->ocarina = 0;
			config->hook = 0;
			config->bootMode = 0;
			
			return -1;
			}
		
		f = fopen(cfg, "rb");
		if (!f) return 0;
		
		ret = fread (config, 1, sizeof(s_channelConfig), f );
		fclose(f);
		}
	
	return ret;
	}
	
int ManageGameConfig (char *asciiId, int write, s_gameConfig *config)
	{
	int ret = 0;
	char cfg[256];
	FILE* f = NULL;
	
	sprintf (cfg, "%s://ploader/config/%s.cfg", vars.defMount, asciiId);
	
	if (write)
		{
		f = fopen(cfg, "wb");
		if (!f) return 0;
		
		ret = fwrite (config, 1, sizeof(s_gameConfig), f );
		fclose(f);
		}
	else
		{
		if (!fsop_FileExist (cfg)) // Create a default config
			{
			config->priority = 5;
			config->hidden = 0;
			
			config->ios = 0; 		// use current
			config->vmode = 0;
			config->language = -1;
			config->vpatch = 0;
			config->ocarina = 0;
			config->hook = 0;
			
			return -1;
			}
		
		f = fopen(cfg, "rb");
		if (!f) return 0;
		
		ret = fread (config, 1, sizeof(s_gameConfig), f );
		fclose(f);
		}
	
	return ret;
	}
	
