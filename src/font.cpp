/*
    Formido - a shooting game (see README for additional information)
    Copyright (C) 2002, 2003 Mika Halttunen

    You can contact me by email: lsoft@mbnet.fi
    -----------------------------------------------------------------

    This file is part of Formido.

    Formido is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Formido is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Formido; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "SDL.h"
#include "SDL_image.h"
#include "init.h"
#include "font.h"

// Font characters (for each font)
SDL_Surface *fnt_char[FONTS][128];

// Font sizes
#ifdef SF3500
int font_size[] = { 32, 16, 16, 32 };		// the two small fonts are made larger below (19 pixels)
SDL_Surface *fnt_orig[2][128];				// the small fonts at their original size (the big menu font is made from them)
static SDL_Surface *scale_glyph(SDL_Surface *src, float scale);
#else
const int font_size[] = { 32, 16, 16, 32 };
#endif

// Font spacing for each font
#ifdef SF3500
int font_spacing[] = { 20, 10, 10, 20 };
#else
const int font_spacing[] = { 20, 10, 10, 20 };
#endif


// Write a string using a specifig font
void font_write(SDL_Surface *bmp, int font, int x, int y, char *str) {

	// Loop through each character
	for(unsigned int c=0; c<strlen(str); c++) {
		// Blit character
		SDL_Rect r;
		r.x = x;
		r.y = y;
		r.w = font_size[font];
		r.h = font_size[font];
		SDL_BlitSurface(fnt_char[font][str[c]], NULL, bmp, &r);

		x += font_spacing[font];
	}
}


// Return string length using a specifig font
int font_length(int font, char *str) {

	return (strlen(str) * font_spacing[font]);
}


// Load fonts
void load_fonts() {

	// Choose the color key
	Uint32 color_key = SDL_MapRGB(screen->format, 255,0,255);

	for(int i=0; i<FONTS; i++) {

		// Load the font graphic to a single surface, then grab the
		// invidual characters out of it.
		SDL_Surface *tmp, *tmp2;
		char file[100] = "";
		sprintf(file, "font%d.dat", i+1);
		tmp2 = IMG_Load(DAT(file));
		if(tmp2 == NULL)
			error_msg("Unable to load fonts!\n%s", IMG_GetError());

		// Convert it to display format
		tmp = SDL_DisplayFormat(tmp2);
		if(tmp == NULL)
			error_msg("Unable to load fonts!\n%s", IMG_GetError());

		SDL_FreeSurface(tmp2);

		int x = 0, y = 0;
		for(int f=0; f<128; f++) {
			// Create surface and set the color keying
			fnt_char[i][f] = SDL_CreateRGBSurface(SDL_HWSURFACE, font_size[i],font_size[i],
				screen->format->BitsPerPixel,
				screen->format->Rmask, screen->format->Gmask, screen->format->Bmask, 0);
			
			if(fnt_char[i][f] == NULL)
				error_msg("Unable to create font surfaces!\n%s", SDL_GetError());
			
			if(SDL_SetColorKey(fnt_char[i][f], SDL_SRCCOLORKEY|SDL_RLEACCEL, color_key))
				error_msg("Unable to set font color keys!\n%s", SDL_GetError());

			// Grab the font from tmp
			SDL_Rect r;
			r.x = x;
			r.y = y;
			r.w = font_size[i];
			r.h = font_size[i];
			SDL_BlitSurface(tmp, &r, fnt_char[i][f], NULL);

			x += font_size[i];
			if(x >= font_size[i]*16) {
				x = 0;
				y += font_size[i];
			}
		}

		SDL_FreeSurface(tmp);

#ifdef SF3500
		// "A bit larger": the two small handwriting fonts at 1.2 times the size (the advance follows: 12)
		if(i == 1 || i == 2) {
			for(int f=0; f<128; f++) {
				fnt_orig[i-1][f] = fnt_char[i][f];
				SDL_Surface *big = scale_glyph(fnt_orig[i-1][f], 1.2f);
				if(big)
					fnt_char[i][f] = big;
			}
		}
#endif
	}
#ifdef SF3500
	font_size[1] = font_size[2] = 19;
	font_spacing[1] = font_spacing[2] = 12;
#endif

}


// Unload fonts
void unload_fonts() {

	for(int i=0; i<FONTS; i++)
		for(int f=0; f<128; f++)
			if(fnt_char[i][f])
				SDL_FreeSurface(fnt_char[i][f]);
}

#ifdef SF3500
// The big font: the normal one scaled up with bilinear filtering
#define BIG_SCALE	1.65f			// the main menu
#define BIG_SPACING	15
#define MED_SCALE	1.4f			// the settings screen
#define MED_SPACING	13
SDL_Surface *fnt_big[2][128];
SDL_Surface *fnt_med[2][128];
SDL_Surface *fnt_score[128];			// the score counter: the bold font a little smaller
#define SCORE_SCALE	0.875f
#define SCORE_SPACING	17

static SDL_Surface *scale_glyph(SDL_Surface *src, float scale) {
	int nw = (int)(src->w * scale + 0.5f), nh = (int)(src->h * scale + 0.5f);
	SDL_Surface *dst = SDL_CreateRGBSurface(SDL_SWSURFACE, nw, nh, src->format->BitsPerPixel,
		src->format->Rmask, src->format->Gmask, src->format->Bmask, 0);
	if(!dst || src->format->BitsPerPixel != 16)
		return NULL;
	Uint32 key = SDL_MapRGB(dst->format, 255, 0, 255);
	Uint32 skey = (src->flags & SDL_SRCCOLORKEY) ? src->format->colorkey : key;
	SDL_FillRect(dst, NULL, key);
	if(SDL_MUSTLOCK(src))
		SDL_LockSurface(src);
	for(int y=0; y<nh; y++) {
		float fy = (y + 0.5f) / scale - 0.5f;
		int y0 = (int)floorf(fy);
		float wy = fy - y0;
		for(int x=0; x<nw; x++) {
			float fx = (x + 0.5f) / scale - 0.5f;
			int x0 = (int)floorf(fx);
			float wx = fx - x0;
			float ar = 0, ag = 0, ab = 0, aa = 0;
			for(int k=0; k<4; k++) {
				int xx = x0 + (k & 1), yy = y0 + (k >> 1);
				float w = ((k & 1) ? wx : 1 - wx) * ((k >> 1) ? wy : 1 - wy);
				if(xx < 0 || yy < 0 || xx >= src->w || yy >= src->h)
					continue;
				Uint32 v = *(Uint16 *)((Uint8 *)src->pixels + yy * src->pitch + xx * 2);
				if(v == skey)
					continue;
				Uint8 r,g,b;
				SDL_GetRGB(v, src->format, &r, &g, &b);
				ar += w * r; ag += w * g; ab += w * b; aa += w;
			}
			if(aa < 0.45f)
				continue;
			*(Uint16 *)((Uint8 *)dst->pixels + y * dst->pitch + x * 2) =
				(Uint16)SDL_MapRGB(dst->format, (int)(ar/aa), (int)(ag/aa), (int)(ab/aa));
		}
	}
	if(SDL_MUSTLOCK(src))
		SDL_UnlockSurface(src);
	SDL_SetColorKey(dst, SDL_SRCCOLORKEY|SDL_RLEACCEL, key);
	return dst;
}

void load_big_font() {
	static bool done = false;
	if(done)
		return;
	done = true;
	for(int c=0; c<128; c++) {
		fnt_big[0][c] = scale_glyph(fnt_orig[0][c], BIG_SCALE);
		fnt_big[1][c] = scale_glyph(fnt_orig[1][c], BIG_SCALE);
		if(fnt_char[FONT_BOLD][c])
			fnt_score[c] = scale_glyph(fnt_char[FONT_BOLD][c], SCORE_SCALE);
		fnt_med[0][c] = scale_glyph(fnt_orig[0][c], MED_SCALE);
		fnt_med[1][c] = scale_glyph(fnt_orig[1][c], MED_SCALE);
	}
}

void font_write_big(SDL_Surface *bmp, int sel, int x, int y, char *str) {
	for(unsigned int c=0; c<strlen(str); c++) {
		SDL_Surface *g = fnt_big[sel][(int)str[c]];
		if(g) {
			SDL_Rect r;
			r.x = x;
			r.y = y;
			r.w = g->w;
			r.h = g->h;
			SDL_BlitSurface(g, NULL, bmp, &r);
		}
		x += BIG_SPACING;
	}
}

void font_write_med(SDL_Surface *bmp, int sel, int x, int y, char *str) {
	for(unsigned int c=0; c<strlen(str); c++) {
		SDL_Surface *g = fnt_med[sel][(int)str[c]];
		if(g) {
			SDL_Rect r;
			r.x = x;
			r.y = y;
			r.w = g->w;
			r.h = g->h;
			SDL_BlitSurface(g, NULL, bmp, &r);
		}
		x += MED_SPACING;
	}
}

int font_length_score(char *str) {
	return (int)strlen(str) * SCORE_SPACING;
}

void font_write_score(SDL_Surface *bmp, int x, int y, char *str) {
	for(unsigned int c=0; c<strlen(str); c++) {
		SDL_Surface *g = fnt_score[(int)str[c]];
		if(g) {
			SDL_Rect r;
			r.x = x;
			r.y = y;
			r.w = g->w;
			r.h = g->h;
			SDL_BlitSurface(g, NULL, bmp, &r);
		}
		x += SCORE_SPACING;
	}
}

int font_length_med(char *str) {
	return (int)strlen(str) * MED_SPACING;
}
#endif
