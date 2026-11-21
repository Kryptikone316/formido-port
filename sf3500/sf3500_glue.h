/* Datafrog SF3500 glue for Formido: display, gamepad, audio, image loading.
 * Everything the game needs from the device goes through the functions below;
 * init.h maps SDL_Flip / SDL_PollEvent onto sf3500_flip / sf3500_poll. */
#ifndef SF3500_GLUE_H
#define SF3500_GLUE_H

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Call first thing in init_sdl(): picks SDL's dummy video driver, opens the
 * display driver and the gamepad reader. */
void sf3500_init(void);

/* Registered with atexit by sf3500_init(); safe to call more than once. */
void sf3500_shutdown(void);

/* Replacement for SDL_Flip: presents the screen surface through the display driver. */
int sf3500_flip(SDL_Surface *s);

/* Replacement for SDL_PollEvent: first turns gamepad changes into SDL key events. */
int sf3500_poll(SDL_Event *e);

/* The match is running (1) or a menu/credits/high-score screen is (0): decides what
 * the B and Start buttons do. */
void sf3500_set_in_game(int in_game);

#ifdef __cplusplus
}
#endif

#endif
