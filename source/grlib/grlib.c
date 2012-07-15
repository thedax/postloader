#define GRLIB

#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>
#include "grlib.h"

void Debug (const char *text, ...);

GRRLIB_texImg *popPushTex = NULL;
GRRLIB_texImg *redrawTex = NULL;

s_grlibSettings grlibSettings;

s_ir grlib_irPos;

// WPADShutdownCallback 
static void wmpower_cb( s32 chan )
	{	
	grlibSettings.wiiswitch_poweroff = true;
	return;
	}

static void power_cb() 
	{
	grlibSettings.wiiswitch_poweroff = true;
	}

static void reset_cb() 
	{
	grlibSettings.wiiswitch_reset = true;
	}
	
void grlib_Controllers (bool enable)
	{
	if (enable)
		{
		PAD_Init(); 
		WPAD_Init();
		WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
		WPAD_SetVRes(0, 640, 480);
		WPAD_SetPowerButtonCallback( wmpower_cb );
		}
	else
		{
		u8 i;		

		for (i = 0; i < WPAD_MAX_WIIMOTES; i++) 
			{
			if(WPAD_Probe(i,0) < 0) continue;
			
			WPAD_Flush(i);
			WPAD_Disconnect(i);
			}

		WPAD_Shutdown();
		PAD_Reset(0xf0000000);
		}
	}

void grlib_Init (void)
	{
	memset (&grlibSettings, 0, sizeof (s_grlibSettings));
	grlibSettings.wiiswitch_poweroff = false;
	grlibSettings.wiiswitch_reset = false;
	
    // Initialise the Wiimotes & Gamecube
	PAD_Init(); 
    WPAD_Init();
	WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);

	VIDEO_Init();
	VIDEO_SetBlack(true);  // Disable video output during initialisation
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();

    GRRLIB_Init(1,0);

	WPAD_SetVRes(0, 640, 480);
	WPAD_SetPowerButtonCallback( wmpower_cb );
	
	SYS_SetPowerCallback (power_cb);
    SYS_SetResetCallback (reset_cb);
	
	redrawTex = GRRLIB_CreateEmptyTexture (rmode->fbWidth, rmode->efbHeight);
	popPushTex = GRRLIB_CreateEmptyTexture (rmode->fbWidth, rmode->efbHeight);
	
	grlibSettings.RedrawCallback = NULL;
	
	grlibSettings.pointer[0] = NULL;
	grlibSettings.pointer[1] = NULL;
	grlibSettings.pointer[2] = NULL;
	grlibSettings.pointer[3] = NULL;
	}

void grlib_Exit (void)
	{
	GRRLIB_FreeTexture (redrawTex);
	GRRLIB_FreeTexture (popPushTex);
	if (!grlibSettings.doNotCall_GRRLIB_Exit) 
		GRRLIB_Exit ();
	else
		GRRLIB_ExitLight ();
	}
	
void grlib_SetRedrawCallback (GRLIB_RedrawCallback cbRedraw, GRLIB_RedrawCallback cbOverlay)
	{
	grlibSettings.RedrawCallback = cbRedraw;
	grlibSettings.OverlayCallback = cbOverlay;
	}

void grlib_SetWiimotePointer (GRRLIB_texImg *pointer, int num)
	{
	grlibSettings.pointer[num] = pointer;
	}
	
int grlib_GetScreenW (void)
	{
	return rmode->fbWidth;
	}

int grlib_GetScreenH (void)
	{
	return rmode->efbHeight;
	}

void grlib_PushScreen (void)
	{
	GRRLIB_Screen2Texture (0, 0, popPushTex, 0);
	}

void grlib_PopScreen (void)
	{
	bool al = GRRLIB_GetAntiAliasing ();
	GRRLIB_SetAntiAliasing (0);
	GRRLIB_DrawImg(0, 0, popPushTex, 0, 1, 1, RGBA(255, 255, 255, 255) );
	GRRLIB_SetAntiAliasing (al);
	}

GRRLIB_texImg * grlib_CreateTexFromTex (GRRLIB_texImg *source)
	{
	GRRLIB_texImg * tex;
	GRRLIB_SetAntiAliasing (FALSE);
	if (source == NULL) 
		return NULL;
		
	tex = GRRLIB_CreateEmptyTexture (source->w, source->h);
	if (tex == NULL) return NULL;
	
	memcpy (tex->data, source->data, source->w * source->h * 4);
	GRRLIB_FlushTex (tex);
	return tex;
	}

// Draw the latest buffered popPushTex... but before do that discard all previous texture
void grlib_DrawScreenTex (GRRLIB_texImg * tex)
	{
	bool al = GRRLIB_GetAntiAliasing ();
	GRRLIB_SetAntiAliasing (FALSE);
	GRRLIB_DrawImg (0, 0, tex , 0, 1, 1, RGBA(255, 255, 255, 255) );
	GRRLIB_SetAntiAliasing (al);
	}

// Call the current registered redraw function
void grlib_Redraw (void)
	{
	if (grlibSettings.RedrawCallback != NULL) grlibSettings.RedrawCallback();
	}

void inline grlib_Render (void) 
	{
	// Sometimes same fb are skipped, I do not know why, 
	// but copyng display to tex and redrawing it solves without big effort
	// and loss of performance.
	// In continuos update mode, doesn't heppen, so GRRLIB_Render can be called.
	
	// GRRLIB_Screen2Texture (0, 0, redrawTex, 1);
	// grlib_DrawScreenTex (redrawTex);
	
	GRRLIB_Render ();
	} 

void grlib_Text (f32 xpos, f32 ypos, u8 align, u32 color, char *text)
	{
    uint  i;
    u8    *pdata;
    u8    x, y;
	u8 	  r,g,b,a;
	u8 	  ghostChar = 0;

    f32   xoff = xpos;
    const GRRLIB_bytemapChar *pchar;
	
	ypos --;

	if (align == GRLIB_ALIGNCENTER)
	    xoff -= grlib_GetFontMetrics (text, NULL, NULL) / 2;

	if (align == GRLIB_ALIGNRIGHT)
	    xoff -= grlib_GetFontMetrics (text, NULL, NULL);
		
	if (grlibSettings.fontMode == GRLIB_FONTMODE_TTF) 
		{
		if (grlibSettings.fontDef.reverse)
			GRRLIB_PrintfTTF(xoff, ypos + grlibSettings.fontDef.offsetY, grlibSettings.font, text, grlibSettings.fontDef.size, 0x0 | 0xFF);
		else
			GRRLIB_PrintfTTF(xoff, ypos + grlibSettings.fontDef.offsetY, grlibSettings.font, text, grlibSettings.fontDef.size, 0xFFFFFFFF | 0xFF);
		return;
		}

	for (i=0; i < strlen (text); i++) 
		{
		if (text[i] < 32) continue;

		if (text[i] != '\255')
			{
			pchar = &grlibSettings.fontBMF->charDef[(u8)text[i]];
			ghostChar = 0;
			}
		else
			{
			i++;
			pchar = &grlibSettings.fontBMF->charDef[(u8)text[i]];
			ghostChar = 1;
			}

        pdata = pchar->data;
        for (y = 0; y < pchar->height; y++) 
			{
            for (x = 0; x < pchar->width; x++) 
				{
                if (*pdata && !ghostChar) 
					{
					if (color == 0)
						{
						u32 c = grlibSettings.fontBMF->palette[*pdata];
						
						if (!grlibSettings.fontDef.reverse)
							{
							r = R(c);	
							g = G(c);
							b = B(c);
							}
						else
							{
							r = 255-R(c);
							g = 255-G(c);
							b = 255-B(c);
							}
							
						a = A(c);
						c = RGBA (r,g,b,a);
						//if (R(grlibSettings.fontBMF->palette[*pdata]) > 32 && G(grlibSettings.fontBMF->palette[*pdata]) > 32 && B(grlibSettings.fontBMF->palette[*pdata]) > 32)
							GRRLIB_Plot(xoff + x + pchar->relx, ypos + y + pchar->rely, c);
						}
					else
						{
						u32 c;

						r = R(color);
						g = G(color);
						b = B(color);
						a = A(grlibSettings.fontBMF->palette[*pdata]);
						c = RGBA (r,g,b,a);
						GRRLIB_Plot(xoff + x + pchar->relx, ypos + y + pchar->rely, c);
						}
					}
                pdata++;
				}
			}
        
		xoff += pchar->kerning + grlibSettings.fontBMF->tracking;
		}
	}

void grlib_printf (const f32 xpos, const f32 ypos, const u8 align, const u32 color, const char *text, ...)
	{
    char  tmp[1024];

    va_list argp;
    va_start(argp, text);
    vsprintf(tmp, text, argp);
    va_end(argp);
	
	grlib_Text (xpos, ypos, align, color, tmp);
	}

// grlib_IsObjectVisible: just check if an object is visible on the screen. Using this may greatly speedup rendering 
// if there are a lot of offscreen obects 
bool grlib_IsObjectVisible ( s_grlibobj *b )
	{
	if ( (b->x1 < 0 && b->x2 < 0) ||
		 (b->x1 > 640 && b->x2 > 640) ||
		 (b->y1 < 0 && b->y2 < 0) ||
		 (b->y1 > 480 && b->x2 > 480))
		 return false;

	return true;
	}

void grlib_DrawSquare ( s_grlibobj *b )
	{
	GRRLIB_Rectangle ( b->x1, b->y1, b->x2-b->x1, b->y2-b->y1, b->bcolor, 1 );
	GRRLIB_Rectangle ( b->x1, b->y1, b->x2-b->x1, b->y2-b->y1, b->color, 0 );
	}

void grlib_DrawEmptySquare ( s_grlibobj *b )
	{
	GRRLIB_Rectangle ( b->x1, b->y1, b->x2-b->x1, b->y2-b->y1, b->color, 0 );
	}

void grlib_DrawBoldEmptySquare ( s_grlibobj *b )
	{
	GRRLIB_Rectangle ( b->x1, b->y1, b->x2-b->x1, b->y2-b->y1, b->color, 0 );
	GRRLIB_Rectangle ( b->x1-1, b->y1-1, b->x2-b->x1+2, b->y2-b->y1+2, b->color, 0 );
	GRRLIB_Rectangle ( b->x1-2, b->y1-2, b->x2-b->x1+4, b->y2-b->y1+4, b->color, 0 );
	}

// Set the font to be used for the next txt operation
void grlib_SetFontBMF2 (GRRLIB_bytemapFont *bmf) 
	{
	grlibSettings.fontBMF = bmf;
	//grlibSettings.fontMode = GRLIB_FONTMODE_BMF;
	}

// Set the font to be used for the next txt operation
void grlib_SetFontTTF (GRRLIB_ttfFont *ttf, int fontSize, int fontOffsetY, int fontSizeOffsetY) 
	{
	if (ttf) grlibSettings.font = ttf;
	
	if (fontSize < 10) fontSize = 10;

	grlibSettings.fontMode = GRLIB_FONTMODE_TTF;
	
	grlibSettings.fontDef.size = fontSize;
	grlibSettings.fontDef.offsetY = fontOffsetY;
	grlibSettings.fontDef.sizeOffsetY = fontSizeOffsetY;
	}

// Assign the font to be used in menu
void grlib_SetMenuFontTTF (GRRLIB_ttfFont *ttf, int fontSize, int fontOffsetY, int fontSizeOffsetY) 
	{
	if (ttf) grlibSettings.font = ttf;
	
	if (fontSize < 10) fontSize = 10;
	
	grlibSettings.fontMode = GRLIB_FONTMODE_TTF;
	
	grlibSettings.fontDefMenu.size = fontSize;
	grlibSettings.fontDefMenu.offsetY = fontOffsetY;
	grlibSettings.fontDefMenu.sizeOffsetY = fontSizeOffsetY;
	}

// return width and height of the font. height/with can be NULL... return is the widht
int grlib_GetFontMetrics (const char *text, int *width, int *height) 
	{
    uint  i;
	int   h;
	int   ch;
    int   xoff = 0;
    const GRRLIB_bytemapChar *pchar;
	
	if (grlibSettings.fontMode == GRLIB_FONTMODE_TTF)
		{
		xoff = GRRLIB_WidthTTF(grlibSettings.font, text, grlibSettings.fontDef.size);
		// gprintf ("GRRLIB_WidthTTF '%s' = %d\r\n", text, xoff);
		/*
		for (i = 0; i < strlen (text); i++) 
			xoff += wtable[grlibSettings.fontSize-10][(unsigned char)text[i]];
		*/
		if (height) *height = grlibSettings.fontDef.size + grlibSettings.fontDef.sizeOffsetY; // Should be measured
		if (width) *width = xoff;
		return xoff;
		}
	
	if (grlibSettings.fontMode == GRLIB_FONTMODE_BMF)
		{
		h = 0;
		for (i=0; i<strlen(text); i++) 
			{
			pchar = &grlibSettings.fontBMF->charDef[(u8)text[i]];
			xoff += pchar->kerning + grlibSettings.fontBMF->tracking;
			ch = pchar->height+pchar->relx;
			if (ch > h) h = ch;
			}

		if (height) *height = h;
		if (width) *width = xoff;
		return xoff;
		}
		
	return 0;
	} 

void grlib_ComputeObjectMeanPoint (s_grlibobj *go)
	{
	go->xm = go->x1 + (go->x2 - go->x1) / 2;
	go->ym = go->y1 + (go->y2 - go->y1) / 2;
	}


// Draw a window centered in the screen... return the y offset of the title.. go is the bounding rect
int grlib_DrawCenteredWindow (char * title, int w, int h, bool grayoutBackground, s_grlibobj *go)
	{
	s_grlibobj goWindow;
	int halfx, halfy;
	int th, tw;
	char *t;
	int l = 0;
	
	l = strlen (title);
	t = malloc (l+1);
	strcpy (t, title);
	
	if (l > 0)
		{
		l--;
		
		do
			{
			grlib_GetFontMetrics (t, &tw, &th);
			if (tw > w)
				{
				t[l] = 0;
				l--;
				}
			else
				break;
				
			if (l == 0) break;
			}
		while (TRUE);
		}
	
	// Grayout background
	if (grayoutBackground)
		{
		goWindow.x1 = -100;
		goWindow.y1 = -100;
		goWindow.x2 = grlib_GetScreenW()+100;
		goWindow.y2 = grlib_GetScreenH()+100;
		goWindow.color = RGBA(0,0,0,128);
		goWindow.bcolor = RGBA(0,0,0,128);
		grlib_DrawSquare (&goWindow);
		}

	halfx = rmode->fbWidth / 2;
	halfy = rmode->efbHeight / 2;
	
	goWindow.x1 = halfx - (w / 2);
	goWindow.x2 = halfx + (w / 2);
	
	goWindow.y1 = halfy - (h / 2);
	goWindow.y2 = halfy + (h / 2);
	
	goWindow.bcolor = RGBA (32, 32, 32, 192);
	goWindow.color = RGBA (192, 192, 192, 255);
	
	if (grlibSettings.theme.enabled)
		{
		grlib_DrawSquareThemed (&goWindow, grlibSettings.theme.texWindow, grlibSettings.theme.texWindowBk, grlibSettings.theme.windowMagX, grlibSettings.theme.windowMagY, DSTF_BKFILLBORDER);
		}
	else
		{
		grlib_DrawSquare (&goWindow);
		grlib_DrawBoldEmptySquare (&goWindow);
		}
		
	grlib_Text (halfx, goWindow.y1 + 10, GRLIB_ALIGNCENTER, 0, t);
	
	if (go)
		{
		go->x1 = goWindow.x1;
		go->x2 = goWindow.x2;
		go->y1 = goWindow.y1;
		go->y2 = goWindow.y2;
		
		grlib_ComputeObjectMeanPoint(go);
		}

	return 0;
	}

void grlib_DrawImgCenter (f32 x, f32 y, f32 w, f32 h, GRRLIB_texImg * tex, f32 angle, u32 color)
	{
	f32 zx, zy;
	
	if (!tex) return;
	
	zx = (f32)w / tex->w;
	zy = (f32)h / tex->h;
	
	GRRLIB_SetHandle (tex, tex->w / 2, tex->h / 2);
	//GRRLIB_SetHandle (tex, 0,0);
	
	x -= tex->w/2;
	y -= tex->w/2;
	
	GRRLIB_DrawImg (x, y, tex, angle, zx, zy, color ); 
	}
	
void grlib_DrawImg (f32 x, f32 y, f32 w, f32 h, GRRLIB_texImg * tex, f32 angle, u32 color)
	{
	f32 zx, zy;
	
	if (!tex) return;
	
	zx = (f32)w / tex->w;
	zy = (f32)h / tex->h;
	
	GRRLIB_DrawImg (x, y, tex, angle, zx, zy, color ); 
	}

void grlib_DrawTile (f32 x, f32 y, f32 w, f32 h, GRRLIB_texImg * tex, f32 angle, u32 color, int frame)
	{
	f32 zx, zy;
	
	if (!tex) return;
	
	zx = (f32)w / tex->tilew;
	zy = (f32)h / tex->tileh;
	
	GRRLIB_DrawTile (x, y, tex, angle, zx, zy, color, frame); 
	}
	
void grlib_DrawPart (f32 x, f32 y, f32 w, f32 h, f32 tx, f32 ty, f32 tw, f32 th, GRRLIB_texImg * tex, f32 angle, u32 color)
	{
	f32 zx, zy;
	
	if (!tex) return;
	
	zx = (f32)w / (f32)tw;
	zy = (f32)h / (f32)th;
	
	GRRLIB_DrawPart (x, y, tx, ty, tw, th, tex, angle, zx, zy, color);
	}
	
	
void grlib_DrawIRCursor (void)
	{
	ir_t ir;
	static u8 alphadec = 255;
	static s16 alpha = 0; // Start with cursor hidden
	static u32 cursorActivity = 0;
	
	static u32 startms = 0;
	u32 ms = ticks_to_millisecs(gettime());
	
	WPAD_ScanPads();  // Scan the Wiimotes
	WPAD_IR (0, &ir);
	
	if (ms > startms)
		{
		if (cursorActivity == grlibSettings.cursorActivity)
			{
			alpha -= alphadec;
			if (alpha < 0) alpha = 0;
			}
		else
			{
			alpha = 255;
			alphadec = 5;
			cursorActivity = grlibSettings.cursorActivity;
			}
		
		startms = ms+100;
		}
	
	if (ir.valid)
		{
		grlib_irPos.x = ir.x;
		grlib_irPos.y = ir.y;
		grlib_irPos.valid = 1;
		alpha = 255;
		}
	else
		{
		grlib_irPos.valid = 0;
		}

	GRRLIB_DrawImg( grlib_irPos.x, 
					grlib_irPos.y, 
					grlibSettings.pointer[0], 0, 1, 1, RGBA(255, 255, 255, alpha) ); 
	}
	
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This function scan for controllers and return the right common value
// it also support analog sticks for move cursor on the screen

#define MAXG 6
int grlib_GetUserInput (void)
	{
	u32  wbtn, gcbtn, cbtn;
	s8  gcX, gcY;
	int nX, nY;
	int cX, cY;
	
	static float g[MAXG];
	static int gidx = -1;
	static u32 gestureDisable = 0;
	static u64 repeatms = 0;
	
	u64 ms = ticks_to_millisecs(gettime());
		
	struct expansion_t e; //nunchuk

	WPAD_ScanPads();  // Scan the Wiimotes
	
	wbtn = WPAD_ButtonsDown(0);
	if (!wbtn)
		wbtn = WPAD_ButtonsHeld(0);
		
	if (!wbtn)
		repeatms = 0;
		
	//gprintf ("%u ", repeatms);
	if (gidx == -1)
		{
		memset (&g, 0, sizeof(g));
		gidx = 0;
		}
	
	if (grlibSettings.usesGestures && ms > gestureDisable)
		{
		WPADData *wp = WPAD_Data (0);
		g[gidx] = wp->gforce.x;

		int i;
		float mean = 0;
		for (i = 0; i < MAXG; i++)
			mean+=g[i];

		mean /= (float) MAXG;

		if (mean < -1.5)
			{
			memset (&g, 0, sizeof(g));
			gestureDisable = ms+1000;
			return WPAD_BUTTON_MINUS;
			}
		if (mean > 1.5)
			{
			memset (&g, 0, sizeof(g));
			gestureDisable = ms+1000;
			return WPAD_BUTTON_PLUS;
			}
		
		if (++gidx >= MAXG) gidx = 0;
		}
	

	WPAD_Expansion( 0, &e );
	
	if (e.type != WPAD_EXP_NUNCHUK)
		{
		nX = 0;
		nY = 0;
		}
	else
		{
		nX = e.nunchuk.js.pos.x - e.nunchuk.js.center.x;
		nY = e.nunchuk.js.pos.y - e.nunchuk.js.center.y;
		}

	if (e.type != WPAD_EXP_CLASSIC)
		{
		cX = 0;
		cY = 0;
		cbtn = 0;
		}
	else
		{
		cX = e.classic.ljs.pos.x - e.classic.ljs.center.x;
		cY = e.classic.ljs.pos.y - e.classic.ljs.center.y;
		cbtn = e.classic.btns;
		}
	
	PAD_ScanPads();
	gcX = PAD_StickX(0);
	gcY = PAD_StickY(0);
	gcbtn = PAD_ButtonsDown(0);
	
	// sticks
	if (abs (nX) > 10) {grlib_irPos.x += (nX / 16); grlibSettings.cursorActivity++;}
	if (abs (nY) > 10) {grlib_irPos.y -= (nY / 16); grlibSettings.cursorActivity++;}
	
	if (abs (gcX) > 10) {grlib_irPos.x += (gcX / 16); grlibSettings.cursorActivity++;}
	if (abs (gcY) > 10) {grlib_irPos.y -= (gcY / 16); grlibSettings.cursorActivity++;}
	
	if (abs (cX) > 10) {grlib_irPos.x += (cX / 4); grlibSettings.cursorActivity++;}
	if (abs (cY) > 10) {grlib_irPos.y -= (cY / 4); grlibSettings.cursorActivity++;}
	
	// Check limits
	if (grlib_irPos.x < 0) grlib_irPos.x = 0;
	if (grlib_irPos.x > 640) grlib_irPos.x = 640;
	
	if (grlib_irPos.y < 0) grlib_irPos.y = 0;
	if (grlib_irPos.y > 480) grlib_irPos.y = 480;
	
	// As usual wiimotes will have priority
	if (wbtn && !cbtn)
		{
		grlibSettings.buttonActivity ++;
		
		if (repeatms == 0)
			{
			repeatms = ticks_to_millisecs(gettime()) + 1000;
			}
		else
			{
			// Wait until button is released
			if (ticks_to_millisecs(gettime()) < repeatms)
				return 0;
			}
		
		return wbtn;
		}

	// Then gc
	if (gcbtn)
		{
		grlibSettings.buttonActivity ++;
		
		if (repeatms == 0)
			{
			repeatms = ticks_to_millisecs(gettime()) + 1000;
			}
		else
			{
			// Wait until button is released
			if (ticks_to_millisecs(gettime()) < repeatms)
				return 0;
			}
		
		// Convert to wiimote values
		if (gcbtn & PAD_TRIGGER_R) return WPAD_BUTTON_PLUS;
		if (gcbtn & PAD_TRIGGER_L) return WPAD_BUTTON_MINUS;

		if (gcbtn & PAD_BUTTON_A) return WPAD_BUTTON_A;
		if (gcbtn & PAD_BUTTON_B) return WPAD_BUTTON_B;
		if (gcbtn & PAD_BUTTON_X) return WPAD_BUTTON_1;
		if (gcbtn & PAD_BUTTON_Y) return WPAD_BUTTON_2;
		if (gcbtn & PAD_BUTTON_MENU) return WPAD_BUTTON_HOME;
		if (gcbtn & PAD_BUTTON_UP) return WPAD_BUTTON_UP;
		if (gcbtn & PAD_BUTTON_LEFT) return WPAD_BUTTON_LEFT;
		if (gcbtn & PAD_BUTTON_DOWN) return WPAD_BUTTON_DOWN;
		if (gcbtn & PAD_BUTTON_RIGHT) return WPAD_BUTTON_RIGHT;
		}
		
	// Classic
	if (cbtn)
		{
		grlibSettings.buttonActivity ++;
		
		if (repeatms == 0)
			{
			repeatms = ticks_to_millisecs(gettime()) + 1000;
			}
		else
			{
			// Wait until button is released
			if (ticks_to_millisecs(gettime()) < repeatms)
				return 0;
			}
		
		// Convert to wiimote values
		if (cbtn & CLASSIC_CTRL_BUTTON_ZR) return WPAD_BUTTON_PLUS;
		if (cbtn & CLASSIC_CTRL_BUTTON_ZL) return WPAD_BUTTON_MINUS;
		
		if (cbtn & CLASSIC_CTRL_BUTTON_PLUS) return WPAD_BUTTON_PLUS;
		if (cbtn & CLASSIC_CTRL_BUTTON_MINUS) return WPAD_BUTTON_MINUS;

		if (cbtn & CLASSIC_CTRL_BUTTON_A) return WPAD_BUTTON_A;
		if (cbtn & CLASSIC_CTRL_BUTTON_B) return WPAD_BUTTON_B;
		if (cbtn & CLASSIC_CTRL_BUTTON_X) return WPAD_BUTTON_1;
		if (cbtn & CLASSIC_CTRL_BUTTON_Y) return WPAD_BUTTON_2;
		if (cbtn & CLASSIC_CTRL_BUTTON_HOME) return WPAD_BUTTON_HOME;

        if (cbtn & CLASSIC_CTRL_BUTTON_UP) return WPAD_BUTTON_UP;
        if (cbtn & CLASSIC_CTRL_BUTTON_DOWN) return WPAD_BUTTON_DOWN;
        if (cbtn & CLASSIC_CTRL_BUTTON_LEFT) return WPAD_BUTTON_LEFT;
        if (cbtn & CLASSIC_CTRL_BUTTON_RIGHT) return WPAD_BUTTON_RIGHT;
		}
	
	
	return 0;
	}


void grlib_MagObject ( s_grlibobj *bt, s_grlibobj *bs, f32 magx, f32 magy)
	{
	memmove (bt, bs, sizeof(s_grlibobj));

	bt->x1 -= magx;
	bt->y1 -= magy;
	bt->x2 += magx;
	bt->y2 += magy;
	}

