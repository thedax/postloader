#include <gccore.h>
#include "globals.h"

/*
ioConfig manage the configuration file

TODO: use text configuration files... maybe
*/

// This function will init the structures used to keep configuration files
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
	
	memset (&config, 0, sizeof(config));
	
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
	
	if (strcmp (ver, CFGVER) == 0)
		fread (&config, sizeof(config), 1, f);
		
	fclose (f);
	
	return TRUE;
	}
