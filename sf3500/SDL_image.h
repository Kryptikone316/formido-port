// SPDX-License-Identifier: GPL-2.0-or-later
/* Stand-in for SDL_image's header on the Datafrog SF3500.
 *
 * Formido only calls IMG_Load and IMG_GetError. Its .dat files are PCX, PNG, JPEG and
 * BMP in disguise; sf3500_glue.cpp decodes them itself (a small PCX reader plus
 * stb_image), so no SDL_image / libpng / libjpeg is needed on the device. */
#ifndef SF3500_SDL_IMAGE_H
#define SF3500_SDL_IMAGE_H

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

SDL_Surface *IMG_Load(const char *file);
const char  *IMG_GetError(void);

#ifdef __cplusplus
}
#endif

#endif
