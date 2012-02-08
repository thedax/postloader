#ifndef __GRLIB_H__
#define __GRLIB_H__

#include "grrlib.h"

#define GRLIBVER "1.3"

#define GRLIB_ALIGNLEFT 0
#define GRLIB_ALIGNCENTER 1
#define GRLIB_ALIGNRIGHT 2

#define GRLIB_FONTMODE_BMF 0
#define GRLIB_FONTMODE_TTF 1

#define BTNSTATE_NORM 0
#define BTNSTATE_SEL 1
#define BTNSTATE_DIM 2

#define MNUBTN_B -1	// cancel
#define MNUBTN_PLUS -2
#define MNUBTN_MINUS -3
#define MNUBTN_TOUT -4	// Menu has gone in timeout

// THEMED FLAGS
#define DSTF_NONE 0x0
#define DSTF_BKFILLBORDER 0x1		// Nella grlib_DrawSquareThemed
#define DSTF_BKFILLNOBORDER 0x2

GRR_EXTERN GXRModeObj *rmode;
GRR_EXTERN void      *xfb[2];
GRR_EXTERN u32       fb;

// This is used to pass your private redraw function. It must not contain any GX _Render() 
// so that grlib function can redraw background if need (for example when a menu' is displayed
typedef void (*GRLIB_RedrawCallback)(void); 

typedef struct
	{
	// these settings are used also for non themed version
	int buttonsTextOffsetY;
	
	// these settings are for theming
	bool enabled;
	GRRLIB_texImg * texWindow; 		// 3x3 tile, suggested size 24x24pix
	GRRLIB_texImg * texWindowBk; 	// full size image, used for window background
	GRRLIB_texImg * texButton; 		// 3x3 tile, suggested size 24x24pix
	GRRLIB_texImg * texButtonSel; 	// 3x3 tile, suggested size 24x24pix
	
	int windowMagX, windowMagY;
	int buttonMagX, buttonMagY;
	} 
s_grlibTheme;

// grlib is extension to grrlib
typedef struct
	{
	GRLIB_RedrawCallback RedrawCallback;	// grlib support two callbacks... they are called in sequence
	GRLIB_RedrawCallback OverlayCallback;	// overlay, for function that support dimming (like menu) is NOT drawn dimmed
	
	GRRLIB_texImg *pointer[4];
	GRRLIB_bytemapFont *fontBMF;				// Current bmf font to use
	GRRLIB_bytemapFont *fontNormBMF;			// Used by menu
	GRRLIB_bytemapFont *fontSmallBMF;			// Used by menu
	int fontBMF_reverse;						// If true font color is reversed
	u8 fontMode;
	u32 color_window;
	u32 color_windowBg;
	
	bool wiiswitch_reset;		// true if user press reset
	bool wiiswitch_poweroff;	// true if user use wiimote or button on the console
	
	bool doNotCall_GRRLIB_Exit;	// true if you doesn't want that 
	u32 autoCloseMenu;			// 0 disabled, otherwise num of second after whitch it will be closed
	
	u32 buttonActivity;			// Every time a button is pressed this is incremented, usefull to track activity
	u32 cursorActivity;			// Track if the user has moved by sticks
	
	// Theme managment
	s_grlibTheme theme;
	} 
s_grlibSettings;

typedef struct
	{
	f32 x1, y1, x2, y2;
	f32 xm, ym;
	u32 color,bcolor;
	char text[64];
	} 
s_grlibobj;

typedef struct
	{
	f32 x1, y1, x2, y2;
	} 
s_grlibrect;

typedef struct 
	{
	int x, y, valid;
	}
s_ir;

typedef struct 
	{
	bool sel;	// If sel
	bool noIcon;	// If true, no icon will be draw (also iconFake)
	u8 transparency;
	
	f32 x, y, w, h, xoff, yoff;	// Icon position (centered)
	
	GRRLIB_texImg *icon;
	GRRLIB_texImg *alticon;   // if icon is NULL draw this one
	GRRLIB_texImg *iconOverlay[4];	// topleft, topright,bottomright, bottomleft overlay icons.
	
	char title[64];	// Icon text (font must be selected before)
	
	f32 rx1, rx2, ry1, ry2; // After icon has been drawn, this is the bounding rect
	}
s_grlib_icon;

typedef struct 
	{
	bool themed;		// true if theming must be used

	f32 border;
	f32 magX, magY;					// Standard icon magnify
	f32 magXSel, magYSel;			// Selected icon magnify

	GRRLIB_texImg *iconFake; 		// if icon and alternateIcon are null, draw this !
	
	// Themed
	GRRLIB_texImg *bkgTex; 		// Background texture
	GRRLIB_texImg *fgrSelTex; 	// foreground texture when selected flag is on
	GRRLIB_texImg *fgrTex; 		// foreground texture 
	
	// Unthemed
	u32 bkgColor; 			// Background texture
	u32 borderSelColor; 	// Border color
	u32 borderColor; 		// Border color
	
	u32 fontReverse; 		// Border color
	}
s_grlib_iconSetting;

extern s_ir grlib_irPos;
extern s_grlibSettings grlibSettings;

void grlib_SetRedrawCallback (GRLIB_RedrawCallback cbRedraw, GRLIB_RedrawCallback cbOverlay);

void grlib_Controllers (bool enable);
void grlib_Init (void);
void grlib_Exit (void);
void grlib_SetWiimotePointer (GRRLIB_texImg *pointer, int num); // Up to 4 pointer, only one supported now
int grlib_GetScreenW (void);
int grlib_GetScreenH (void);
void grlib_PushScreen (void);
void grlib_PopScreen (void);
GRRLIB_texImg * grlib_CreateTexFromTex (GRRLIB_texImg *source);
void grlib_DrawScreenTex (GRRLIB_texImg * tex);
void grlib_Redraw (void);
void grlib_Render (void);
void grlib_SetFontBMF (GRRLIB_bytemapFont *bmf);
int grlib_GetFontMetrics (const char *text, int *width, int *heigh);
void grlib_Text (f32 xpos, f32 ypos, u8 align, u32 color, char *text);
void grlib_printf (const f32 xpos, const f32 ypos, const u8 align, const u32 color, const char *text, ...);

void grlib_DrawSquare ( s_grlibobj *b );
void grlib_DrawEmptySquare ( s_grlibobj *b );
void grlib_DrawBoldEmptySquare ( s_grlibobj *b );

void grlib_MagObject ( s_grlibobj *bt, s_grlibobj *bs, f32 magx, f32 magy);

void grlib_DrawImgCenter (int x, int y, int w, int h, GRRLIB_texImg * tex, f32 angle, u32 color);
void grlib_DrawImg (int x, int y, int w, int h, GRRLIB_texImg * tex, f32 angle, u32 color);
void grlib_DrawTile (int x, int y, int w, int h, GRRLIB_texImg * tex, f32 angle, u32 color, int frame);
void grlib_DrawPart (int x, int y, int w, int h, int tx, int ty, int tw, int th, GRRLIB_texImg * tex, f32 angle, u32 color);

int grlib_DrawCenteredWindow (char * title, int w, int h, bool grayoutBackground, s_grlibobj *go);
void grlib_DrawButton ( s_grlibobj *b, int state);
void grlib_DrawIRCursor (void);

int grlib_dosm (const char *text, ...);

int grlib_menuAddItem (char *menu, int id, const char *itemsstring, ...);
int grlib_menuAddSeparator (char *menu);
int grlib_menuAddColumn (char *menu);
int grlib_menuAddCheckItem (char *menu, int id, bool check, const char *itemsstring, ...);
int grlib_menu (char *title, const char *itemsstring, ...); // item1|item2|item3... etc,

void grlib_Message (const char *text, ...);

int grlib_GetUserInput (void);

void grlib_DrawSquareThemed ( s_grlibobj *b, GRRLIB_texImg * tex, GRRLIB_texImg * texbk, int magx, int magy, u32 flag);

void grlib_IconSettingInit (s_grlib_iconSetting *grlib_iconSetting);
void grlib_IconInit (s_grlib_icon *icon, s_grlib_icon *parentIcon);
void grlib_IconDraw (s_grlib_iconSetting *is, s_grlib_icon *icon);

/*****************************************************************************************************************************
HISTORY

1.3
* Icons routines

1.2
* Created grlib_Text for unformatted output.
* Renamed grlib_Print to grlib_printf
* Corrected a typo on function name grlib_menuAddSeparator

1.1
* Added overlay callback.

1.0 - First stable version
*****************************************************************************************************************************/
#endif