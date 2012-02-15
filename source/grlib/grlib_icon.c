#include <stdarg.h>
#include "../globals.h"
#include "grlib.h"

void grlib_IconSettingInit (s_grlib_iconSetting *grlib_iconSetting)
	{
	memset (grlib_iconSetting, 0, sizeof(s_grlib_iconSetting));
	}
	
void grlib_IconInit (s_grlib_icon *icon, s_grlib_icon *parentIcon)
	{
	if (parentIcon)
		memcpy (icon, parentIcon, sizeof(s_grlib_icon));
	else
		{
		memset (icon, 0, sizeof(s_grlib_icon));
		icon->transparency = 255;
		}
	}
	
void grlib_IconDraw (s_grlib_iconSetting *is, s_grlib_icon *icon)
	{
	GRRLIB_texImg *tex;
	f32 w, h;
	s_grlibobj gro, grob;
	
	if (icon->sel)
		{
		w = (f32)icon->w * is->magXSel;
		h = (f32)icon->h * is->magYSel;
		}
	else
		{
		w = (f32)icon->w * is->magX;
		h = (f32)icon->h * is->magY;
		}
		
	// Calculate icon bounds

	gro.x1 = (icon->x - w / 2.0) - icon->xoff;
	gro.x2 = (icon->x + w / 2.0) - icon->xoff;
	gro.y1 = (icon->y - h / 2.0) - icon->yoff;
	gro.y2 = (icon->y + h / 2.0) - icon->yoff;
	
	grlib_MagObject (&grob, &gro, is->border, is->border);
	
	// Draw background
	
	if (is->themed)		{
		// Draw mask (if exist)
		if (!icon->noIcon)	grlib_DrawSquareThemed (&grob, is->bkgTex, NULL, 0, 0, DSTF_NONE);
		}
	else
		{
		grob.bcolor = is->bkgColor;
		grob.color = is->borderColor;
		
		grlib_DrawSquare (&grob);
		}
	
	// Draw Icon
	if (!icon->noIcon)
		{
		if (icon->icon)
			tex = icon->icon;
		else if (icon->alticon)
			tex = icon->alticon;
		else
			tex = is->iconFake;
		
		grlib_DrawImg (gro.x1, gro.y1, w, h, tex, 0, RGBA(255, 255, 255, icon->transparency));
		}
		
	// Draw text
	if (icon->title && strlen(icon->title))
		{
		char desc[64];
		
		strncpy (desc, icon->title, 63);
			
		bool cutted = FALSE;
		while (grlib_GetFontMetrics(desc, NULL, NULL) > w - 10 && strlen(desc) > 0)
			{
			desc[strlen(desc)-1] = 0;
			cutted = TRUE;
			}
		if (cutted) strcat (desc, "...");
		
		int ofr = grlibSettings.fontBMF_reverse;
		grlibSettings.fontBMF_reverse = is->fontReverse;
		grlib_printf (gro.x1 + w / 2, gro.y1 + h / 2 - 5, GRLIB_ALIGNCENTER, 0, desc); 
		grlibSettings.fontBMF_reverse = ofr;
		}
	
	// Draw overlay icons
	if (icon->iconOverlay[0])
		{
		grlib_DrawImg (gro.x1, gro.y1, icon->iconOverlay[0]->w, icon->iconOverlay[0]->h, icon->iconOverlay[0], 0, RGBA(255, 255, 255, 255));
		}
	
	if (icon->iconOverlay[1])
		{
		grlib_DrawImg (gro.x2 - icon->iconOverlay[1]->w, gro.y1, icon->iconOverlay[1]->w, icon->iconOverlay[1]->h, icon->iconOverlay[1], 0, RGBA(255, 255, 255, 255));
		}
	
	if (icon->iconOverlay[2])
		{
		grlib_DrawImg (gro.x2 - icon->iconOverlay[2]->w, gro.y2 - icon->iconOverlay[2]->h, icon->iconOverlay[2]->w, icon->iconOverlay[2]->h, icon->iconOverlay[2], 0, RGBA(255, 255, 255, 255));
		}
	
	// Draw outer frame
	if (icon->sel)
		{
		if (is->themed)
			{
			grlib_DrawSquareThemed (&grob, is->fgrTex, NULL, 0, 0, DSTF_NONE);
			grlib_DrawSquareThemed (&grob, is->fgrSelTex, NULL, 0, 0, DSTF_NONE);
			}
		else
			{
			grob.bcolor = is->bkgColor;
			grob.color = is->borderSelColor;
			
			grlib_DrawBoldEmptySquare (&grob);
			}
		}
	else
		{
		if (is->themed)
			{
			grlib_DrawSquareThemed (&grob, is->fgrTex, NULL, 0, 0, DSTF_NONE);
			}
		else
			{
			grob.bcolor = is->bkgColor;
			grob.color = is->borderColor;
			
			grlib_DrawEmptySquare (&grob);
			}
		}
		
	icon->rx1 = grob.x1;
	icon->rx2 = grob.x2;

	icon->ry1 = grob.y1;
	icon->ry2 = grob.y2;
		
	}