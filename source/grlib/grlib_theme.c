#include <stdarg.h>
#include "grlib.h"

static void CheckForTile (GRRLIB_texImg * tex)
	{
	if (!tex->tiledtex)
		{
		int w = tex->w / 3;
		int h = tex->h / 3;
		
		GRRLIB_InitTileSet (tex, w, h, 0);
		}
	}
	
/* TILES LAYOUT

|---|---|---|
| 0 | 1 | 2 |
|---|---|---|
| 3 | 4 | 5 |
|---|---|---|
| 6 | 7 | 8 |
|---|---|---|

*/

// magpx/y allow to expand image...
// flag is for future use
// tex is the tiled texture
// texbk is the background texture... can be NULL, otherwise it will be used to fill the square...

void grlib_DrawSquareThemed ( s_grlibobj *b, GRRLIB_texImg * tex, GRRLIB_texImg * texbk, f32 magx, f32 magy, u32 flag, u8 transparency)
	{
	if (tex == NULL) return;
	
	f32 tw, th;
	s_grlibobj gb;
	
	CheckForTile (tex);
	
	grlib_MagObject (&gb, b, magx, magy);
	
	tw = tex->tilew;
	th = tex->tileh;
	
	// Upper left
	grlib_DrawTile (gb.x1, gb.y1, tw, th, tex, 0, RGBA (255,255,255,transparency), 0);
	// Upper right
	grlib_DrawTile (gb.x2-tw, gb.y1, tw, th, tex, 0, RGBA (255,255,255,transparency), 2);
	// Upper center
	grlib_DrawTile (gb.x1+tw, gb.y1, gb.x2-gb.x1 - (tw*2.0), th, tex, 0, RGBA (255,255,255,transparency), 1);
	
	// Lower left
	grlib_DrawTile (gb.x1, gb.y2-th, tw, th, tex, 0, RGBA (255,255,255,transparency), 6);
	// Lower right
	grlib_DrawTile (gb.x2-tw, gb.y2-th, tw, th, tex, 0, RGBA (255,255,255,transparency), 8);
	// Lower center
	grlib_DrawTile (gb.x1+tw, gb.y2-th, gb.x2-gb.x1 - (tw*2.0), th, tex, 0, RGBA (255,255,255,transparency), 7);
	
	// center left
	grlib_DrawTile (gb.x1, gb.y1+th, tw, gb.y2-gb.y1 - (th*2.0), tex, 0, RGBA (255,255,255,transparency), 3);
	// center right
	grlib_DrawTile (gb.x2-tw, gb.y1+th, tw, gb.y2-gb.y1 - (th*2.0), tex, 0, RGBA (255,255,255,transparency), 5);

	// center/center
	if (texbk == NULL)
		grlib_DrawTile (gb.x1+tw, gb.y1+th, gb.x2-gb.x1 - (tw*2.0), gb.y2-gb.y1 - (th*2.0), tex, 0, RGBA (255,255,255,transparency), 4);
	else
		{
		// Applichiamo la texture come sfondo

		f32 tbw, tbh;
		f32 bx, by, bw, bh;
		
		if (flag == DSTF_BKFILLBORDER)
			{
			//gprintf ("#1");
			bx = gb.x1+1;
			by = gb.y1+th;
			bw = gb.x2 - gb.x1 - 2;
			bh = gb.y2-gb.y1 - (th*2.0)+1;
			}
		else
			{
			//Ugprintf ("#2");
			bx = gb.x1+tw;
			by = gb.y1+th;
			bw = gb.x2-gb.x1 - (tw*2.0);
			bh = gb.y2-gb.y1 - (th*2.0);
			}
		
		if (texbk->w > bw) 
			tbw = bw;
		else
			tbw = texbk->w;
		
		if (texbk->h > bh) 
			tbh = bh;
		else
			tbh = texbk->h;
			
		grlib_DrawPart (bx, by, bw, bh, 0, 0, tbw, tbh, texbk, 0, RGBA (255,255,255,transparency));
		}
		
		
	}

