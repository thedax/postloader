#include <stdarg.h>
#include "grlib.h"
#include <wiiuse/wpad.h>

extern s_grlibSettings grlibSettings;

//DrawOnScreenMessage
int grlib_dosm (const char *text, ...) // ab > 0 show and wait ab second, otherwhise, wait a or b press
	{
	int ret;
	static char mex[1024];
	char *p1,*p2;
	
	if (text != NULL)
		{
		va_list argp;
		va_start(argp, text);
		vsprintf(mex, text, argp);
		va_end(argp);
		}

	// wait for no-button pressed
	do {WPAD_ScanPads();} while (WPAD_ButtonsDown(0));
	
	s_grlibobj b;

	b.x1 = 70;
	b.y1 = 120;
	b.x2 = grlib_GetScreenW()-b.x1;
	b.y2 = grlib_GetScreenH()-b.y1;
	b.color = RGBA (255, 2552, 255, 255);
	b.bcolor = RGBA (0, 0, 0, 192);
	
	grlib_PopScreen ();
	grlib_DrawSquare (&b);
	grlib_DrawBoldEmptySquare (&b);
	
	b.x1 += 10;
	b.y1 += 10;
	
	p1 = mex;
	do
		{
		p2 = strstr (p1, "\n");
		if (p2 != NULL)
			{
			*p2 = 0;
			p2++;
			}
		grlib_printf ( b.x1, b.y1, GRLIB_ALIGNLEFT, 0, p1);
		b.y1 +=15;
		p1 = p2;
		}
	while (p2 != NULL);

	grlib_Render ();
	
	ret = 0;
	do 
		{
		WPAD_ScanPads();
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_A) ret = 1;
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_B) ret = 2;
		} 
	while (ret == 0);
	do {WPAD_ScanPads();} while (WPAD_ButtonsDown(0));
	
	return ret;
	}
	
// if text start with "^+" the string is aligned to left, and [*] is drawn
// if text start with "^-" the string is aligned to left, and [ ] is drawn
void grlib_DrawButton ( s_grlibobj *b, int state)
	{
	int w, h;
	char text[64];
	int toggle = -1;
	
	if (b->text[0] == '^')
		{
		if (b->text[1] == '+') toggle = 1;
		if (b->text[1] == '-') toggle = 0;
		
		if (toggle == 0) sprintf (text, "[\255*] %s", &b->text[2]);
		if (toggle == 1) sprintf (text, "[*] %s", &b->text[2]);
		}
	
	if (toggle == -1) strcpy(text, b->text);
	
	if (grlibSettings.theme.enabled)
		{
		if (state == BTNSTATE_SEL && grlibSettings.theme.texButtonSel)
			grlib_DrawSquareThemed ( b, grlibSettings.theme.texButtonSel, NULL, grlibSettings.theme.buttonMagX, grlibSettings.theme.buttonMagY, DSTF_NONE);
		else
			grlib_DrawSquareThemed ( b, grlibSettings.theme.texButton, NULL, grlibSettings.theme.buttonMagX, grlibSettings.theme.buttonMagY, DSTF_NONE);
		}
	else
		grlib_DrawSquare ( b );
	
	grlib_GetFontMetrics (text, &w, &h);
	if (toggle == -1)
		grlib_printf (
					b->x1 + (b->x2 - b->x1) / 2, 
					b->y1 + (b->y2 - b->y1) / 2 - (h / 2) + grlibSettings.theme.buttonsTextOffsetY, 
					GRLIB_ALIGNCENTER, 0, text
					);
	else
		{
		grlib_printf (
					b->x1 + 5, 
					b->y1 + (b->y2 - b->y1) / 2 - (h / 2) + grlibSettings.theme.buttonsTextOffsetY, 
					GRLIB_ALIGNLEFT, 0, text
					);
		}
	}
	
void grlib_Message (const char *text, ...) // ab > 0 show and wait ab second, otherwhise, wait a or b press
	{
	char * mex;
	
	mex = calloc (1, strlen(text) + 1024);
	if (!mex) return;
	
	mex[0] = '\0';
	
	if (text != NULL)
		{
		va_list argp;
		va_start(argp, text);
		vsprintf(mex, text, argp);
		va_end(argp);
		}
	
	grlib_menu	(mex, "OK");
	
	free (mex);
	}
