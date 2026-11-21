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

#ifndef MENU_H
#define MENU_H

#include "SDL.h"
#include "SDL_mixer.h"

// Menu backgrounds
extern SDL_Surface *menu_bg[3];

// Menu arrow
extern SDL_Surface *menu_arrow;

// Sounds
extern Mix_Chunk *snd_menu[3];

// Menu items and IDs
#define MENU_ID_MAIN			0
#ifdef SF3500
	#define MENU_NEW_GAME		0	// classic
	#define MENU_NEW_ENHANCED	1
	#define MENU_HALL_OF_FAME	2
	#define MENU_GAME_SETTINGS	3
	#define MENU_EXIT			4
#else
	#define MENU_NEW_GAME		0
	#define MENU_HALL_OF_FAME	1
	#define MENU_GAME_SETTINGS	2
	#define MENU_EXIT			3
#endif

#define MENU_ID_SETTINGS		1
#ifdef SF3500
	// Datafrog SF3500: the key rows are replaced by controls, frame limit and credits rows
	#define MENU_OPT_CONTROLS	5
	#define MENU_OPT_FPS		6
	#define MENU_OPT_CREDITS	7
	#define MENU_MUS_VOLUME		8
	#define MENU_ACCEPT			9
	#define MENU_KEY_FORWARD	MENU_OPT_CONTROLS	// first row, for the up/down wrap-around
#else
	#define MENU_KEY_FORWARD	4
	#define MENU_KEY_BACKWARD	5
	#define MENU_KEY_LEFT		6
	#define MENU_KEY_RIGHT		7
	#define MENU_KEY_SHOOT		8
	#define MENU_KEY_SPECIAL	9
	#define MENU_MUS_VOLUME		10
	#define MENU_ACCEPT			11
#endif

#define MENU_ID_HALL_OF_FAME	2
	#define MENU_HOF_NULL		12

void load_menus();
void unload_menus();
int show_menu(int menu_id);

#endif

