# Formido on the Datafrog SF3500

Formido is a top-down shooter against alien insects, written by Mika Halttunen
(code, game design, artwork) with music by Josh Schroeder. Version 1.0.1, released
under the GNU General Public License, version 2 or (at your option) any later
version: `GPL_license.txt`, and the game's own `README`.

Original game: https://mhgames.org/formido/

## What was changed for this port

- The video, gamepad and sound calls go through `sf3500/sf3500_glue.cpp`: SDL runs on its
  dummy video driver and every frame is handed to the handheld's display driver; the gamepad
  becomes key presses; the few SDL_mixer calls the game makes are mixed in software and played
  through the handheld's audio driver; images (PCX/PNG/JPEG/BMP) are decoded without SDL_image.
- Twin-stick controls (D-pad walks, L1 + face buttons aim and fire) as the default; the original tank controls stay available with `controls = 0` in `formido.cfg`.
- High-score names are entered with the D-pad (the original reads a keyboard).
- The key-binding rows of the settings menu are inert: the gamepad has one fixed layout.
- Source changes are marked `#ifdef SF3500`; everything else is the upstream 1.0.1 source.

## Libraries and credits

- `stb_image.h`, `stb_vorbis.c` (Sean Barrett): public domain / MIT.
- The display, audio and gamepad glue (`hwdisp`, `hwaudio`, `joykey`, `sf3500_mixer`) is derived from
  TreeFrogUI's pico286 SF3000 patch and is shared by the other SF3500 ports.
- Box art: floppy-disk illustration made for this collection.
