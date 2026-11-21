/* Stand-in for SDL_mixer's header on the Datafrog SF3500.
 *
 * The device has no SDL audio device (and no SDL_mixer library), so the few Mix_*
 * calls Formido makes are answered by sf3500_glue.cpp, which mixes in software and
 * hands the result to the device's own audio driver (hwaudio / sf3500_mixer).
 * Only the subset Formido uses is declared. */
#ifndef SF3500_SDL_MIXER_H
#define SF3500_SDL_MIXER_H

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mix_Chunk Mix_Chunk;
typedef struct Mix_Music Mix_Music;

int         Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void        Mix_CloseAudio(void);
const char *Mix_GetError(void);

Mix_Chunk  *Mix_LoadWAV_RW(SDL_RWops *src, int freesrc);
#define Mix_LoadWAV(file) Mix_LoadWAV_RW(SDL_RWFromFile(file, "rb"), 1)
void        Mix_FreeChunk(Mix_Chunk *chunk);

int         Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
int         Mix_SetPanning(int channel, Uint8 left, Uint8 right);
int         Mix_HaltChannel(int channel);

Mix_Music  *Mix_LoadMUS(const char *file);
void        Mix_FreeMusic(Mix_Music *music);
int         Mix_PlayMusic(Mix_Music *music, int loops);
int         Mix_FadeInMusic(Mix_Music *music, int loops, int ms);
int         Mix_FadeOutMusic(int ms);
int         Mix_HaltMusic(void);
int         Mix_VolumeMusic(int volume);

#ifdef __cplusplus
}
#endif

#endif
