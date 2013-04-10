#include <gccore.h>
#include "globals.h"
#include "mystring.h"
#include "devices.h"
#include "fsop/fsop.h"

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
	
	Debug ("ConfigWrite()");
	
	if (vars.defMount[0] == '\0') return false;	
	
	if (vars.neek == NEEK_NONE)
		sprintf (path, "%s://ploader/ploader.cfg", vars.defMount);
	else
		sprintf (path, "%s://ploader/pldneek.cfg", vars.defMount);
	
	Debug ("ConfigWrite: %s", path);

	f = fopen (path, "wb");
	if (!f) return false;
	
	// Let's write version
	sprintf (ver, "%s", CFGVER);
	fwrite (ver, sizeof(ver), 1, f);
	
	// Write configuration data
	fwrite (&config, sizeof(config), 1, f);
	fclose (f);
	
	return true;
	}
	
static void InitConfigCats (void)
	{
	strcpy (config.appCats[0], "Games");
	strcpy (config.appCats[1], "Utility");
	strcpy (config.appCats[2], "Emulators");
	strcpy (config.appCats[3], "Backup loaders");
	strcpy (config.appCats[4], "Hacking tools");
	strcpy (config.appCats[5], "n/a");
	strcpy (config.appCats[6], "n/a");
	strcpy (config.appCats[7], "n/a");
	}

bool ConfigRead (void)
	{
	char path[PATHMAX];
	char ver[32];
	FILE *f;
	
	memset (&config, 0, sizeof(config));
	
	if (vars.defMount[0] == '\0') return false;
	
	if (vars.neek == NEEK_NONE)
		sprintf (path, "%s://ploader/ploader.cfg", vars.defMount);
	else
		sprintf (path, "%s://ploader/pldneek.cfg", vars.defMount);
		
	size_t config_size;
	if (fsop_GetFileSizeBytes (path, &config_size) && config_size != (sizeof(config) + sizeof(ver)))
		{
		gprintf ("ConfigRead: %s, filesize mismatch, resetting settins\n", path);
		
		InitConfigCats ();
		return true;
		}
	
	gprintf ("ConfigRead: %s\n", path);

	f = fopen (path, "rb");
	if (!f) 
		{
		gprintf ("unable to open configuration file\n", path);
		InitConfigCats ();
		return false;
		}
	
	// read configuration data
	fread (ver, sizeof(ver), 1, f);
	ver[31] = 0; // make sure to terminate string
	
	if (strcmp (ver, CFGVER) == 0)
		fread (&config, sizeof(config), 1, f);

	fclose (f);
	
	return true;
	}

//////////////////////////////////////////////////

bool ExtConfigWrite (void)
	{
	char path[PATHMAX];
	char idStart[16];
	char idStop[16];
	size_t size;
	u8 *bootdol;
	u8 *cfg;
	
	if (!vars.saveExtendedConf) return true;
	vars.saveExtendedConf = 0;
	
	memcpy (idStart, EXTCONFIG, EXTCONFIGOFFS);
	idStart[EXTCONFIGOFFS] = '\0';
	
	memcpy (idStop, &EXTCONFIG[EXTCONFIGSIZE+EXTCONFIGOFFS], EXTCONFIGOFFS);
	idStop[EXTCONFIGOFFS] = '\0';
	
	int dev;
	for (dev = 0; dev <= 1; dev++)
		if (devices_Get (dev))
			{
			sprintf (path, "%s:/%s/boot.dol", devices_Get (dev), HBAPPFOLDER);
			
			bootdol = fsop_ReadFile (path, 0, &size);
			if (bootdol)
				{
				gprintf ("ExtConfigWrite: searching signature\n");
				cfg = ms_FindStringInBuffer (bootdol, size, idStart);
				if (cfg)
					{
					gprintf ("ExtConfigWrite: signature found\n");
					
					cfg += EXTCONFIGOFFS;
					memcpy (cfg, &extConfig, sizeof (s_extConfig));

					gprintf ("extConfig.use249 = %d\n", extConfig.use249);
					gprintf ("extConfig.disableUSB = %d\n", extConfig.disableUSB);
					gprintf ("extConfig.disableUSBneek = %d\n", extConfig.disableUSBneek);

					gprintf ("ExtConfigWrite: fsop_WriteFile %s %d\n", path, size);
					fsop_WriteFile (path, bootdol, size);
					free (bootdol);
					}
				}
			}
		
	return true;
	}
	
bool ExtConfigRead (void)
	{
	char buff[17];
	strcpy (buff, "0123456789ABCDEF");

	int ret = memcmp (&EXTCONFIG[EXTCONFIGOFFS], buff, 16);
	gprintf ("ExtConfigRead: memcmp ret = %d\n", ret);
	
	// Check if it is intialized....
	if (ret == 0)
		{
		gprintf ("ExtConfigRead: valid data not found\n");
		memset (&extConfig, 0, sizeof (s_extConfig));
		}
	else
		{
		gprintf ("ExtConfigRead: found valid data\n");
		memcpy (&extConfig, &EXTCONFIG[EXTCONFIGOFFS], sizeof (s_extConfig));
		}
		
	gprintf ("extConfig.use249 = %d\n", extConfig.use249);
	gprintf ("extConfig.disableUSB = %d\n", extConfig.disableUSB);
	gprintf ("extConfig.disableUSBneek = %d\n", extConfig.disableUSBneek);
		
	return true;
	}
