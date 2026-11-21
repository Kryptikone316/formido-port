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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "SDL.h"
#include "SDL_image.h"
#include "menu.h"
#include "mymath.h"
#include "timer.h"
#include "game.h"
#include "init.h"
#include "blend.h"
#include "config.h"
#include "player.h"
#include "font.h"
#include "hof.h"
#include "effect.h"


// Menu backgrounds
SDL_Surface *menu_bg[3];

// Menu arrow
SDL_Surface *menu_arrow;

// Sounds
Mix_Chunk *snd_menu[3];

// Current menu item
int menu_item;

// Pointer to the key to set
int *key_to_set;

// Previous key value
int prev_key;

// Are we setting a key?
bool setting_key;


// Load menu graphics
void load_menus() {

	// Load the backgrounds
	SDL_Surface *tmp;
	if(!screen->format->palette)
#ifdef SF3500
		tmp = IMG_Load(DAT("menu_sf.dat"));		// the main menu without its painted entries
#else
		tmp = IMG_Load(DAT("menu.dat"));
#endif
	else
		tmp = IMG_Load(DAT("menu_8bit.dat"));
	if(tmp == NULL)
		error_msg("Unable to load menus!\n%s", IMG_GetError());
	menu_bg[0] = SDL_DisplayFormat(tmp);
	if(menu_bg[0] == NULL)
		error_msg("Unable to load menus!\n%s", IMG_GetError());

	SDL_FreeSurface(tmp);

	if(!screen->format->palette)
#ifdef SF3500
		tmp = IMG_Load(DAT("menu2_sf.dat"));	// the settings panel without its painted key labels
#else
		tmp = IMG_Load(DAT("menu2.dat"));
#endif
	else
		tmp = IMG_Load(DAT("menu2_8bit.dat"));
	if(tmp == NULL)
		error_msg("Unable to load menus!\n%s", IMG_GetError());
	menu_bg[1] = SDL_DisplayFormat(tmp);
	if(menu_bg[1] == NULL)
		error_msg("Unable to load menus!\n%s", IMG_GetError());

	SDL_FreeSurface(tmp);

	if(!screen->format->palette)
		tmp = IMG_Load(DAT("menu3.dat"));
	else
		tmp = IMG_Load(DAT("menu3_8bit.dat"));
	if(tmp == NULL)
		error_msg("Unable to load menus!\n%s", IMG_GetError());
	menu_bg[2] = SDL_DisplayFormat(tmp);
	if(menu_bg[2] == NULL)
		error_msg("Unable to load menus!\n%s", IMG_GetError());

	SDL_FreeSurface(tmp);

#ifdef SF3500
	load_big_font();
#endif

	// Load the menu arrow
	tmp = IMG_Load(DAT("menu_arrow.dat"));
	if(tmp == NULL)
		error_msg("Unable to load menus!\n%s", IMG_GetError());
	menu_arrow = SDL_DisplayFormat(tmp);
	if(menu_arrow == NULL)
		error_msg("Unable to load menus!\n%s", IMG_GetError());

	SDL_FreeSurface(tmp);
}


// Unload menu graphics
void unload_menus() {

	if(menu_bg[0])
		SDL_FreeSurface(menu_bg[0]);
	if(menu_bg[1])
		SDL_FreeSurface(menu_bg[1]);
	if(menu_bg[2])
		SDL_FreeSurface(menu_bg[2]);

	if(menu_arrow)
		SDL_FreeSurface(menu_arrow);
}


// Return a key name in upper case
char *key_name(int key) {

	if(key != -1) {
		// Get the key name from SDL
		// (patch by Jarmo Hekkanen)
		static char buf[32];
#ifdef WIN32
		_snprintf(buf, 32, "%s", SDL_GetKeyName((SDLKey)key));
#else
		snprintf(buf, 32, "%s", SDL_GetKeyName((SDLKey)key));
#endif
		// Convert it to upper case
		for(unsigned int c=0; c<strlen(buf); c++) {
			buf[c] = toupper(buf[c]);
		}

		return buf;
	}
	else
		return "PRESS A KEY";	// This is displayed when we're setting
								// a key.
}


// Show a specifig menu
int show_menu(int menu_id) {

	// Play some music
	Mix_Music *music = NULL;
	if(config.sound) {
		//music = Mix_LoadMUS("data/music_menu.ogg");
		music = Mix_LoadMUS(DAT("music_menu.ogg"));
		//Mix_PlayMusic(music, -1);
		Mix_FadeInMusic(music, -1, 1000);
	}

	// Initialize
	HISCORE_LIST list;
	list.load(HISCORE("hiscore.dat"));
#ifdef SF3500
	HISCORE_LIST list_e;
	list_e.load(HISCORE("hiscore_enh.dat"));
	int hof_page = 0;		// 0 classic, 1 enhanced
#endif

	int action = 0;
	menu_item = 0;
	setting_key = false;
	prev_key = 0;
	key_to_set = NULL;
	memset(key, 0, sizeof(key));
	float arrow_anim = RANDF(0,359);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	// Menu loop
	bool menu_loop = true;
	timer_count = 0;
	while(menu_loop) {
		// Handle events
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_QUIT:		// Quit
					menu_loop = false;
					action = MENU_EXIT;
					break;

				case SDL_KEYDOWN:
					if(setting_key == false) {
						// Update the 'key' array
						key[event.key.keysym.sym] = 1;
					}
					else if(setting_key == true && event.key.keysym.sym != SDLK_ESCAPE && event.key.keysym.sym != SDLK_F12 && event.key.keysym.sym != SDLK_p) {
						// Set the key if it's not ESC or F12
						(*key_to_set) = event.key.keysym.sym;
						setting_key = false;
					}
					else if(setting_key == true && event.key.keysym.sym == SDLK_ESCAPE) {
						// ESC pressed -> cancel the key setting
						(*key_to_set) = prev_key;
						setting_key = false;
					}
					break;

				case SDL_KEYUP:
					// Update the 'key' array
					key[event.key.keysym.sym] = 0;
					break;
			}
		}

		while(timer_count > 0) {

			// Move arrow
			if(menu_id != MENU_ID_HALL_OF_FAME) {
				if(key[SDLK_UP]) {
					if(config.sound)
						Mix_PlayChannel(-1, snd_menu[0], 0);

					menu_item--;
					if(menu_id == MENU_ID_MAIN) {
						if(menu_item < 0)
							menu_item = MENU_EXIT;
					}
					else if(menu_id == MENU_ID_SETTINGS) {
						if(menu_item < MENU_KEY_FORWARD)
							menu_item = MENU_ACCEPT;
					}
				}
				if(key[SDLK_DOWN]) {
					if(config.sound)
						Mix_PlayChannel(-1, snd_menu[0], 0);

					menu_item++;
					if(menu_id == MENU_ID_MAIN) {
						if(menu_item > MENU_EXIT)
							menu_item = MENU_NEW_GAME;
					}
					else if(menu_id == MENU_ID_SETTINGS) {
						if(menu_item > MENU_ACCEPT)
							menu_item = MENU_KEY_FORWARD;
					}
				}
			}
			
			if(key[SDLK_ESCAPE]) {
				// In the main menu: exit, otherwise, return to main
				if(menu_id == MENU_ID_MAIN) {
					action = MENU_EXIT;
					menu_loop = false;
				}
				else if(menu_id == MENU_ID_SETTINGS) {
					menu_id = MENU_ID_MAIN;
					menu_item = MENU_GAME_SETTINGS;
					// Load back the initial settings from config
					load_config(CONFIG("formido.cfg"), &config);
					Mix_VolumeMusic(config.music_vol);
				}
				else if(menu_id == MENU_ID_HALL_OF_FAME) {
					menu_id = MENU_ID_MAIN;
					menu_item = MENU_HALL_OF_FAME;
				}
			}

			// Check for action
			if((key[SDLK_RETURN] || key[SDLK_SPACE])) {
				if(config.sound)
					Mix_PlayChannel(-1, snd_menu[1], 0);
				switch(menu_item) {
					// Main menu
					case MENU_NEW_GAME:
#ifdef SF3500
					case MENU_NEW_ENHANCED:
#endif
						action = menu_item;
						menu_loop = false;
						break;
					case MENU_HALL_OF_FAME:
						menu_item = MENU_HOF_NULL;
						menu_id = MENU_ID_HALL_OF_FAME;
						break;
					case MENU_GAME_SETTINGS:
						menu_item = MENU_KEY_FORWARD;
						menu_id = MENU_ID_SETTINGS;
						break;
					case MENU_EXIT:
						action = menu_item;
						menu_loop = false;
						break;

					// Hall of Fame
					case MENU_HOF_NULL:
						menu_item = MENU_HALL_OF_FAME;
						menu_id = MENU_ID_MAIN;
						break;

					// Game settings menu
					case MENU_ACCEPT:
						menu_item = MENU_GAME_SETTINGS;
						menu_id = MENU_ID_MAIN;
						// Save config
						save_config(CONFIG("formido.cfg"), &config);
						break;

					// Keys
#ifdef SF3500
					// Settings rows that are switched with the select button
					case MENU_OPT_CONTROLS:
						config.controls = !config.controls;
						break;
					case MENU_OPT_FPS:
						config.fps_cap = (config.fps_cap == 20) ? 30 : (config.fps_cap == 30) ? 60 : 20;
						sf3500_set_frame_cap(config.fps_cap);
						break;
					case MENU_OPT_CREDITS:
						config.credits = !config.credits;
						break;
#else
					case MENU_KEY_FORWARD:
						key_to_set = &(config.key_up);
						setting_key = true;
						prev_key = config.key_up;
						config.key_up = -1;
						break;
					case MENU_KEY_BACKWARD:
						key_to_set = &(config.key_down);
						setting_key = true;
						prev_key = config.key_down;
						config.key_down = -1;
						break;
					case MENU_KEY_LEFT:
						key_to_set = &(config.key_left);
						setting_key = true;
						prev_key = config.key_left;
						config.key_left = -1;
						break;
					case MENU_KEY_RIGHT:
						key_to_set = &(config.key_right);
						setting_key = true;
						prev_key = config.key_right;
						config.key_right = -1;
						break;
					case MENU_KEY_SHOOT:
						key_to_set = &(config.key_shoot);
						setting_key = true;
						prev_key = config.key_shoot;
						config.key_shoot = -1;
						break;
					case MENU_KEY_SPECIAL:
						key_to_set = &(config.key_special);
						setting_key = true;
						prev_key = config.key_special;
						config.key_special = -1;
						break;
#endif

				}
			}

#ifdef SF3500
			// Left/right change the option rows too (select cycles forward)
			if(menu_id == MENU_ID_SETTINGS && (key[SDLK_LEFT] || key[SDLK_RIGHT])) {
				int dir = key[SDLK_RIGHT] ? 1 : -1;
				if(menu_item == MENU_OPT_CONTROLS || menu_item == MENU_OPT_CREDITS) {
					if(menu_item == MENU_OPT_CONTROLS)
						config.controls = !config.controls;
					else
						config.credits = !config.credits;
					if(config.sound)
						Mix_PlayChannel(-1, snd_menu[0], 0);
				}
				else if(menu_item == MENU_OPT_FPS) {
					static const int caps[3] = { 20, 30, 60 };
					int i = (config.fps_cap == 20) ? 0 : (config.fps_cap == 30) ? 1 : 2;
					i = (i + dir + 3) % 3;
					config.fps_cap = caps[i];
					sf3500_set_frame_cap(config.fps_cap);
					if(config.sound)
						Mix_PlayChannel(-1, snd_menu[0], 0);
				}
			}
#endif

#ifdef SF3500
			// The Hall of Fame has a list for each game: left/right switch
			if(menu_id == MENU_ID_HALL_OF_FAME && (key[SDLK_LEFT] || key[SDLK_RIGHT])) {
				hof_page = !hof_page;
				if(config.sound)
					Mix_PlayChannel(-1, snd_menu[0], 0);
			}
#endif

			// Check for slider
			if(menu_item == MENU_MUS_VOLUME) {
				if(key[SDLK_LEFT]) {
					config.music_vol -= 5;
					if(config.music_vol < 0)
						config.music_vol = 0;
					Mix_VolumeMusic(config.music_vol);
				}
				if(key[SDLK_RIGHT]) {
					config.music_vol += 5;
					if(config.music_vol > 128)
						config.music_vol = 128;
					Mix_VolumeMusic(config.music_vol);
				}
			}

			// Clear the key array
			memset(key, 0, sizeof(key));

			// Animate arrow
			arrow_anim = add_angle(arrow_anim, 10);

			timer_count--;
		}

		// Draw the menu
		SDL_BlitSurface(menu_bg[menu_id], NULL, screen, NULL);

		// Determine the arrow position
		int ax,ay;
		if(menu_id == MENU_ID_MAIN) {
			ax = 3;
			ay = 243 + menu_item * 37;
		}
		else if(menu_id == MENU_ID_SETTINGS) {
			ax = 288;
#ifdef SF3500
			static const int row_y[5] = { 190, 238, 286, 346, 424 };	// controls, frame limit, credits, volume, accept
			ay = row_y[menu_item - MENU_OPT_CONTROLS] - 4;
#else
			ay = 170 + (menu_item - MENU_KEY_FORWARD) * 35;
			if(menu_item == MENU_MUS_VOLUME)
				ay = 401;
			else if(menu_item == MENU_ACCEPT)
				ay = 429;
#endif
		}

		// Draw the menu arrow
		if(menu_id != MENU_ID_HALL_OF_FAME) {
			if(SDL_MUSTLOCK(screen))
				SDL_LockSurface(screen);
			if(!screen->format->palette)
				add_blit(menu_arrow, screen, ax, ay, (0.5+SIN(arrow_anim)*0.3)*255);
			else
				add_blit8(menu_arrow, screen, ax, ay, (0.5+SIN(arrow_anim)*0.3)*255);

			if(SDL_MUSTLOCK(screen))
				SDL_UnlockSurface(screen);
		}

#ifdef SF3500
		if(menu_id == MENU_ID_MAIN) {
			static char *labels[5] = { (char *)"play classic game", (char *)"play enhanced game", (char *)"hall of fame",
				(char *)"game settings", (char *)"enough already" };
			for(int i=0; i<5; i++)
				font_write_big(screen, (i == menu_item) ? 1 : 0, 56, 240 + i * 37, labels[i]);
		}
#endif

		if(menu_id == MENU_ID_SETTINGS) {
#ifdef SF3500
			// Rows drawn with the game's own handwriting fonts on the cleaned panel
			static const int row_y[5] = { 190, 238, 286, 346, 424 };
			font_write(screen, FONT_NORMAL2, 322, 150, "game settings");

			font_write(screen, (menu_item == MENU_OPT_CONTROLS) ? FONT_NORMAL2 : FONT_NORMAL, 322, row_y[0], "controls");
			font_write(screen, (menu_item == MENU_OPT_CONTROLS) ? FONT_NORMAL2 : FONT_NORMAL, 490, row_y[0], (char *)(config.controls ? "twin stick" : "classic"));
			font_write(screen, FONT_NORMAL, 322, row_y[0]+19, (char *)(config.controls ? "L1 + A B X Y aim and fire" : "left/right turn, A fires"));

			char buf[32];
			sprintf(buf, "%d fps", config.fps_cap);
			font_write(screen, (menu_item == MENU_OPT_FPS) ? FONT_NORMAL2 : FONT_NORMAL, 322, row_y[1], "frame limit");
			font_write(screen, (menu_item == MENU_OPT_FPS) ? FONT_NORMAL2 : FONT_NORMAL, 490, row_y[1], buf);
			font_write(screen, FONT_NORMAL, 322, row_y[1]+19, "20 for slower devices");

			font_write(screen, (menu_item == MENU_OPT_CREDITS) ? FONT_NORMAL2 : FONT_NORMAL, 322, row_y[2], "credits on exit");
			font_write(screen, (menu_item == MENU_OPT_CREDITS) ? FONT_NORMAL2 : FONT_NORMAL, 490, row_y[2], (char *)(config.credits ? "on" : "off"));
			font_write(screen, FONT_NORMAL, 322, row_y[2]+19, "select+start leaves now");

			font_write(screen, (menu_item == MENU_MUS_VOLUME) ? FONT_NORMAL2 : FONT_NORMAL, 322, row_y[3]-18, "music volume");
			font_write(screen, (menu_item == MENU_ACCEPT) ? FONT_NORMAL2 : FONT_NORMAL, 322, row_y[4], "accept settings");
			{
				// The volume slider
				float vol = config.music_vol / 128.0;
				int bw = energy_bar[0]->w;
				for(int i=0; i<15; i++) {
					SDL_Rect r;
					r.x = 322+i*bw;
					r.y = row_y[3]+4;
					r.w = bw;
					r.h = energy_bar[0]->h;
					SDL_BlitSurface(((float)i/15.0 >= vol) ? energy_bar[1] : energy_bar[0], NULL, screen, &r);
				}
			}
#else
			// Draw the music volume slider
			float vol = config.music_vol / 128.0;
			for(int i=0; i<15; i++) {
				SDL_Rect r;
				r.x = 350+i*energy_bar[0]->w;
				r.y = 401;
				r.w = energy_bar[0]->w;
				r.h = energy_bar[0]->h;
				SDL_BlitSurface(((float)i/15.0 >= vol) ? energy_bar[1] : energy_bar[0], NULL, screen, &r);
			}

			// Draw the key names
			// Forward
			font_write(screen, (menu_item == MENU_KEY_FORWARD) ? FONT_NORMAL2 : FONT_NORMAL, 455, 173, key_name(config.key_up));
			// Backward
			font_write(screen, (menu_item == MENU_KEY_BACKWARD) ? FONT_NORMAL2 : FONT_NORMAL, 462, 209, key_name(config.key_down));
			// Turn left
			font_write(screen, (menu_item == MENU_KEY_LEFT) ? FONT_NORMAL2 : FONT_NORMAL, 463, 243, key_name(config.key_left));
			// Turn right
			font_write(screen, (menu_item == MENU_KEY_RIGHT) ? FONT_NORMAL2 : FONT_NORMAL, 476, 279, key_name(config.key_right));
			// Shoot
			font_write(screen, (menu_item == MENU_KEY_SHOOT) ? FONT_NORMAL2 : FONT_NORMAL, 425, 315, key_name(config.key_shoot));
			// Special
			font_write(screen, (menu_item == MENU_KEY_SPECIAL) ? FONT_NORMAL2 : FONT_NORMAL, 426, 348, key_name(config.key_special));
#endif
		}

		// Draw the hall of fame
		if(menu_id == MENU_ID_HALL_OF_FAME) {
#ifdef SF3500
			(hof_page ? list_e : list).draw(screen, 10, 10, -1);
			char *t = (char *)(hof_page ? "< enhanced game >" : "< classic game >");
			font_write(screen, FONT_NORMAL, 360, 192, (char *)"left or right changes the list");
			font_write(screen, FONT_NORMAL2, 360, 170, t);
#else
			list.draw(screen, 10, 10, -1);
#endif
		}

		// Flip to screen
		SDL_Flip(screen);
	}

	// Wipe the screen
	wipe_screen();

	// Unload music
	if(music) {
		Mix_HaltMusic();
		Mix_FreeMusic(music);
		music = NULL;
	}

	SDL_EnableKeyRepeat(0,0);

	return action;
}


