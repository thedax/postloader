#include <stdarg.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <ogc/lwp_watchdog.h>
#include "wiiload/wiiload.h"
#include "globals.h"
#include "bin2o.h"
#include "cfg.h"

GRRLIB_texImg *bkg[2];

void Video_Init (void)
	{
    // Initialise the Graphics & Video subsystem
	grlib_Init();
	grlibSettings.fontMode = GRLIB_FONTMODE_TTF;
	
	if (fatMountSimple("fat", &__io_wiisd))
		{
		bkg[0] = GRRLIB_LoadTextureFromFile ("fat://ploader.png");

		fatUnmount("fat:/");
		__io_wiisd.shutdown();
		}
		
	if (!bkg[0])
		bkg[0] = GRRLIB_LoadTexturePNG (background_png);
		
	bkg[1] = GRRLIB_LoadTexturePNG (bk_lines_png);

	vars.tex[TEX_CUR] = GRRLIB_LoadTexturePNG (wiicursor_png);

	vars.tex[TEX_STAR] = GRRLIB_LoadTexturePNG (star_png);
	vars.tex[TEX_NONE] = GRRLIB_LoadTexturePNG (noicon_png);
	vars.tex[TEX_CHECKED] = GRRLIB_LoadTexturePNG (checked_png);
	vars.tex[TEX_FOLDER] = GRRLIB_LoadTexturePNG (folder_png);
	vars.tex[TEX_FOLDERUP] = GRRLIB_LoadTexturePNG (folderup_png);
	vars.tex[TEX_GHOST] = GRRLIB_LoadTexturePNG (ghost_png);
	
	vars.tex[TEX_HDD] = GRRLIB_LoadTexturePNG (hdd_png);
	vars.tex[TEX_HGL] = GRRLIB_LoadTexturePNG (hourglass_png);
	vars.tex[TEX_RUN] = GRRLIB_LoadTexturePNG (run_png);
	vars.tex[TEX_CHIP] = GRRLIB_LoadTexturePNG (chip_png);
	vars.tex[TEX_EXCL] = GRRLIB_LoadTexturePNG (exclamation_png);
	vars.tex[TEX_WIFI] = GRRLIB_LoadTexturePNG (wifi_png);
	vars.tex[TEX_WIFIGK] = GRRLIB_LoadTexturePNG (wifigk_png);
	vars.tex[TEX_GK] = GRRLIB_LoadTexturePNG (gecko_png);
	vars.tex[TEX_DVD] = GRRLIB_LoadTexturePNG (dvd_png);
	vars.tex[TEX_USB] = GRRLIB_LoadTexturePNG (miniusb_png);
	vars.tex[TEX_SD] = GRRLIB_LoadTexturePNG (minisd_png);
	
	vars.tex[TEX_CAT_HB] = GRRLIB_LoadTexturePNG (cat_hb_png);
	vars.tex[TEX_CAT_GAMECUBE] = GRRLIB_LoadTexturePNG (cat_gamecube_png);
	vars.tex[TEX_CAT_WII] = GRRLIB_LoadTexturePNG (cat_wii_png);
	vars.tex[TEX_CAT_WIIWARE] = GRRLIB_LoadTexturePNG (cat_wiiware_png);
	vars.tex[TEX_CAT_EMUL] = GRRLIB_LoadTexturePNG (cat_emul_png);

	vars.tex[TEX_ICO_ABOUT] = GRRLIB_LoadTexturePNG (ico_about_png);
	vars.tex[TEX_ICO_CONFIG] = GRRLIB_LoadTexturePNG (ico_config_png);
	vars.tex[TEX_ICO_DVD] = GRRLIB_LoadTexturePNG (ico_dvd_png);
	vars.tex[TEX_ICO_SYSMENU] = GRRLIB_LoadTexturePNG (ico_sysmenu_png);
	vars.tex[TEX_ICO_NEEK] = GRRLIB_LoadTexturePNG (ico_neek_png);
	vars.tex[TEX_ICO_SE] = GRRLIB_LoadTexturePNG (ico_se_png);
	vars.tex[TEX_ICO_WM] = GRRLIB_LoadTexturePNG (ico_wm_png);

	grlibSettings.font = GRRLIB_LoadTTF (font_ttf, font_ttf_size);
	grlibSettings.fontSize = 14;
	
	fonts[FNTNORM] = GRRLIB_LoadBMF (fnorm_bmf);
	fonts[FNTSMALL] = GRRLIB_LoadBMF (fsmall_bmf);
	fonts[FNTBIG] = GRRLIB_LoadBMF (fbig_bmf);
	
	grlibSettings.fontNormBMF = fonts[FNTNORM];
	grlibSettings.fontSmallBMF = fonts[FNTSMALL];
	
	//GRRLIB_SetHandle (vars.tex[TEX_CUR], 10, 3);
	vars.tex[TEX_CUR]->offsetx = 10;
	vars.tex[TEX_CUR]->offsety = 3;
	
	GRRLIB_SetHandle (vars.tex[TEX_HDD], 18, 18);
	GRRLIB_SetHandle (vars.tex[TEX_HGL], 18, 18);
	
	grlib_SetWiimotePointer (vars.tex[TEX_CUR], 0);
	}
	
void Video_Deinit (void)
	{
	int i;
	
	GRRLIB_FreeTTF (grlibSettings.font);
	GRRLIB_FreeBMF (fonts[FNTNORM]);
	GRRLIB_FreeBMF (fonts[FNTSMALL]);
	GRRLIB_FreeBMF (fonts[FNTBIG]);
	
	GRRLIB_FreeTexture (bkg[0]);
	GRRLIB_FreeTexture (bkg[1]);
	
	for (i = 0; i < MAXTEX; i++)
		GRRLIB_FreeTexture (vars.tex[i]);
	
	grlib_Exit ();
	}

void Video_DrawBackgroud (int type)
	{
	if (type == 1 && theme.ok)
		{
		//GRRLIB_DrawImg( 0, 0, theme.bkg, 0, 1, 1, RGBA(255, 255, 255, 255) ); 
		grlib_DrawImg ( 0, 0, theme.bkg->w, theme.bkg->h, theme.bkg, 0, RGBA(255, 255, 255, 255) ); 
		}
	else
		GRRLIB_DrawImg( 0, 0, bkg[type], 0, 1, 1, RGBA(255, 255, 255, 255) ); 
	}

void Video_DrawIconZ (int icon, int x, int y, f32 zx, f32 zy)
	{
	static int hdd_angle = 0;
	static int hgl_angle = 0;

	int angle = 0;

	if (icon == TEX_HDD)
		{
		hdd_angle++; 
		if (hdd_angle >= 360) hdd_angle = 0;
		angle = hdd_angle;
		}
	if (icon == TEX_HGL)
		{
		hgl_angle+=2; 
		if (hgl_angle >= 360) hgl_angle = 0;
		angle = hgl_angle;
		}
		
	// Draw centered
	
	x -= ((f32)vars.tex[icon]->w * zx) / 2.0;
	y -= ((f32)vars.tex[icon]->h * zy) / 2.0;
		
	GRRLIB_DrawImg( x, y, vars.tex[icon], angle, zx, zy, RGBA(255, 255, 255, 255) ); 
	}
	
void Video_DrawIcon (int icon, int x, int y)
	{
	Video_DrawIconZ (icon, x, y, 1.0, 1.0);
	}	

/*********************************************************************
Draw an icon for wiiload modes
*********************************************************************/
void Video_DrawWIFI (void)
	{
	static u32 t = 0;
	static u32 lastt = 0;
	static u32 blinkt = 1000;
	static bool flag = 0;
	
	t = ticks_to_millisecs(gettime());
	
	if (t - lastt > blinkt)
		{
		lastt = t;
		
		flag = 1-flag;
		
		if (wiiload.status == WIILOAD_INITIALIZING)
			{
			blinkt = 1000;
			}
		else if (wiiload.status == WIILOAD_IDLE)
			{
			blinkt = 1000;
			flag = 1;		// Keep icon on
			}
		else if (wiiload.status == WIILOAD_RECEIVING)
			{
			blinkt = 250;
			}
		else
			{
			blinkt = 1000;
			flag = 0;		// Other states, icon off
			}
		}

	if (flag)
		{
		Video_DrawIconZ (TEX_WIFI, 600, 435, 0.4, 0.5);
		}
		
	if (wiiload.gecko) 
		Video_DrawIconZ (TEX_GK, 600 + 16, 435, 0.4, 0.5);
		
		
	if (wiiload.op[0] == '!')
		{
		//grlib_SetFontBMF(fonts[FNTSMALL]);
		Video_SetFont(TTFNORM);
		grlib_Text ( 590, 428, GRLIB_ALIGNRIGHT, 0, &wiiload.op[1]);
		}
	}

void Video_WaitPanel (int icon, const char *text, ...)
	{
	char mex[1024];
	char *p;

	if (text != NULL)
		{
		va_list argp;
		va_start(argp, text);
		vsprintf(mex, text, argp);
		va_end(argp);
		}
		
	p = strstr (mex, "|");
	if (p) *p = '\0';
	
	grlib_PopScreen() ;
	grlib_DrawCenteredWindow (mex, WAITPANWIDTH, 133, 0, NULL);
	
	if (p)
		{
		Video_DrawIcon (icon, 320, 240);
		p++;
		grlib_Text (320, 280, GRLIB_ALIGNCENTER, 0, p);
		}
	else
		{
		Video_DrawIcon (icon, 320, 240);
		}
	
	grlib_Render ();
	}
	
void Video_LoadTheme (int init)
	{
	Debug ("Video_LoadTheme(%d)", init);
	
	if (init)
		{
		char path[300];
	
		memset (&theme, 0, sizeof (s_theme));
		
		// Setting default values
		theme.line1Y = 410;
		theme.line2Y = 425;
		theme.line3Y = 450;
		grlibSettings.fontReverse = 0;
		
		sprintf (path, "%s://ploader/theme/bkg.png", vars.defMount);
		theme.bkg = GRRLIB_LoadTextureFromFile (path);
		
		sprintf (path, "%s://ploader/theme/frame.png", vars.defMount);
		theme.frame = GRRLIB_LoadTextureFromFile (path);
		
		sprintf (path, "%s://ploader/theme/frame_sel.png", vars.defMount);
		theme.frameSel = GRRLIB_LoadTextureFromFile (path);
		
		sprintf (path, "%s://ploader/theme/frame_back.png", vars.defMount);
		theme.frameBack = GRRLIB_LoadTextureFromFile (path);
		
		sprintf (path, "%s://ploader/theme/button.png", vars.defMount);
		grlibSettings.theme.texButton = GRRLIB_LoadTextureFromFile (path);
		
		sprintf (path, "%s://ploader/theme/button_sel.png", vars.defMount);
		grlibSettings.theme.texButtonSel = GRRLIB_LoadTextureFromFile (path);
		
		sprintf (path, "%s://ploader/theme/window.png", vars.defMount);
		grlibSettings.theme.texWindow = GRRLIB_LoadTextureFromFile (path);
		
		sprintf (path, "%s://ploader/theme/windowbk.png", vars.defMount);
		grlibSettings.theme.texWindowBk = GRRLIB_LoadTextureFromFile (path);

		Debug ("theme.bkg = 0x%X", theme.bkg);
		Debug ("theme.frame = 0x%X", theme.frame);
		Debug ("theme.frameSel = 0x%X", theme.frameSel);
		Debug ("theme.frameBack = 0x%X", theme.frameBack);
		Debug ("grlibSettings.theme.texButton = 0x%X", grlibSettings.theme.texButton);
		Debug ("grlibSettings.theme.texButtonSel = 0x%X", grlibSettings.theme.texButtonSel);
		Debug ("grlibSettings.theme.texWindow = 0x%X", grlibSettings.theme.texWindow);
		Debug ("grlibSettings.theme.texWindowBk = 0x%X", grlibSettings.theme.texWindowBk);

		if (
			theme.bkg &&
			theme.frame &&
			theme.frameSel &&
			theme.frameBack &&
			grlibSettings.theme.texButton &&
			grlibSettings.theme.texButtonSel &&
			grlibSettings.theme.texWindow /* &&
			grlibSettings.theme.texWindowBk*/
			)
			{
			theme.ok = 1;
			grlibSettings.theme.enabled = true;
			}
		
		sprintf (path, "%s://ploader/theme/theme.cfg", vars.defMount);
		s_cfg *cfg = cfg_Alloc(path, 0);
		if (cfg)
			{
			cfg_GetInt (cfg, "grlibSettings.theme.windowMagX", &grlibSettings.theme.windowMagX);
			cfg_GetInt (cfg, "grlibSettings.theme.windowMagY", &grlibSettings.theme.windowMagY);
			cfg_GetInt (cfg, "grlibSettings.theme.buttonMagX", &grlibSettings.theme.buttonMagX);
			cfg_GetInt (cfg, "grlibSettings.theme.buttonMagY", &grlibSettings.theme.buttonMagY);
			
			cfg_GetInt (cfg, "grlibSettings.theme.buttonsTextOffsetY", &grlibSettings.theme.buttonsTextOffsetY);
			cfg_GetInt (cfg, "grlibSettings.fontBMF_reverse", &grlibSettings.fontReverse);

			cfg_GetInt (cfg, "theme.line1Y", &theme.line1Y);
			cfg_GetInt (cfg, "theme.line3Y", &theme.line3Y);
			cfg_GetInt (cfg, "theme.line2Y", &theme.line2Y);

			Debug ("grlibSettings.theme.windowMagX = %d", grlibSettings.theme.windowMagX);
			Debug ("grlibSettings.theme.windowMagY = %d", grlibSettings.theme.windowMagY);
			Debug ("grlibSettings.theme.buttonMagX = %d", grlibSettings.theme.buttonMagX);
			Debug ("grlibSettings.theme.buttonMagY = %d", grlibSettings.theme.buttonMagY);
			Debug ("grlibSettings.theme.buttonsTextOffsetY = %d", grlibSettings.theme.buttonsTextOffsetY);
			Debug ("grlibSettings.fontReverse = %d", grlibSettings.fontReverse);

			Debug ("theme.line1Y = %d", theme.line1Y);
			Debug ("theme.line2Y = %d", theme.line2Y);
			Debug ("theme.line3Y = %d", theme.line3Y);
			
			cfg_Free (cfg);
			
			if (grlibSettings.fontReverse) // we likely need to revert textures (skipping cursor)
				{
				Debug ("inverting tex");
				GRRLIB_BMFX_Invert (vars.tex[TEX_HDD],vars.tex[TEX_HDD]);
				GRRLIB_BMFX_Invert (vars.tex[TEX_HGL],vars.tex[TEX_HGL]);
				GRRLIB_BMFX_Invert (vars.tex[TEX_RUN],vars.tex[TEX_RUN]);
				GRRLIB_BMFX_Invert (vars.tex[TEX_CHIP],vars.tex[TEX_CHIP]);
				GRRLIB_BMFX_Invert (vars.tex[TEX_EXCL],vars.tex[TEX_EXCL]);
				GRRLIB_BMFX_Invert (vars.tex[TEX_WIFI],vars.tex[TEX_WIFI]);
				GRRLIB_BMFX_Invert (vars.tex[TEX_WIFIGK],vars.tex[TEX_WIFIGK]);
				GRRLIB_BMFX_Invert (vars.tex[TEX_GK],vars.tex[TEX_GK]);
				}
			}
		
		#ifndef DOLPHINE
		if (rmode->viTVMode == VI_DEBUG_PAL)
			{
			theme.line1Y /= 1.01;
			theme.line2Y /= 1.01;
			theme.line3Y /= 1.01;
			}
		#endif
		}
	else if (theme.ok)
		{
		theme.ok = 0;
		grlibSettings.theme.enabled = false;
		
		if (grlibSettings.fontReverse) // we likely need to revert textures (again)  (skipping cursor)
			{
			GRRLIB_BMFX_Invert (vars.tex[TEX_HDD],vars.tex[TEX_HDD]);
			GRRLIB_BMFX_Invert (vars.tex[TEX_HGL],vars.tex[TEX_HGL]);
			GRRLIB_BMFX_Invert (vars.tex[TEX_RUN],vars.tex[TEX_RUN]);
			GRRLIB_BMFX_Invert (vars.tex[TEX_CHIP],vars.tex[TEX_CHIP]);
			GRRLIB_BMFX_Invert (vars.tex[TEX_EXCL],vars.tex[TEX_EXCL]);
			GRRLIB_BMFX_Invert (vars.tex[TEX_WIFI],vars.tex[TEX_WIFI]);
			GRRLIB_BMFX_Invert (vars.tex[TEX_WIFIGK],vars.tex[TEX_WIFIGK]);
			GRRLIB_BMFX_Invert (vars.tex[TEX_GK],vars.tex[TEX_GK]);
			}

		grlibSettings.fontReverse = 0;
		
		GRRLIB_FreeTexture (theme.bkg);
		GRRLIB_FreeTexture (theme.frame);
		GRRLIB_FreeTexture (theme.frameSel);
		GRRLIB_FreeTexture (theme.frameBack);
		GRRLIB_FreeTexture (grlibSettings.theme.texButton);
		GRRLIB_FreeTexture (grlibSettings.theme.texButtonSel);
		GRRLIB_FreeTexture (grlibSettings.theme.texWindow);
		GRRLIB_FreeTexture (grlibSettings.theme.texWindowBk);
		}
	}
	
void Video_SetFont (int size)
	{
	if (size == TTFNORM)
		{
		grlib_SetFontTTF (NULL, size, TTFNORM_fontOffsetY, TTFNORM_fontSizeOffsetY);
		}
	if (size == TTFSMALL)
		{
		grlib_SetFontTTF (NULL, size, TTFSMALL_fontOffsetY, TTFSMALL_fontSizeOffsetY);
		}
	}
	
#define VILEFT 25
#define VITOP 420
void Video_DrawVersionInfo (void)
	{
	Video_SetFont(TTFSMALL);
	grlib_Text ( VILEFT, VITOP, GRLIB_ALIGNLEFT, 0, "V."VER);
	
	if (vars.neek == NEEK_NONE)
		{
		if (vars.ahbprot)
			grlib_printf ( VILEFT, VITOP+15, GRLIB_ALIGNLEFT, 0, "IOS%d+", vars.ios);
		else
			grlib_printf ( VILEFT, VITOP+15, GRLIB_ALIGNLEFT, 0, "IOS%d", vars.ios);
		}
	else
		{
		char neek[32];

		if (vars.neek == NEEK_2o)
			strcpy (neek, "neek2o");
		else
			strcpy (neek, "neek");
		
		grlib_printf ( VILEFT, VITOP+15, GRLIB_ALIGNLEFT, 0, "(%s)", neek);
		
		if (strlen(vars.neekName))
			grlib_printf ( VILEFT, VITOP+30, GRLIB_ALIGNLEFT, 0, "%s", vars.neekName);
		}
	}
