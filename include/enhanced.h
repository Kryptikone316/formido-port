/*
    Formido - Datafrog SF3500 port: the enhanced game.

    The classic game is one 640x480 screen with one kind of bug. The enhanced game adds
      - a cross-shaped map: the original screen in the middle and a barren wasteland screen to the
        north, east, south and west of it, with extra power-ups to find out there;
      - bug ranks with different stats (runner, striker, brute) and a hive that adapts to how the
        player plays;
      - a light health regeneration on the stone platform.
    Everything here is compiled only for the SF3500 build and only used when `enhanced` is set;
    the classic game is untouched.

    World coordinates: all positions in the game stay "current screen" coordinates (0..640 x 0..480
    is what is on the display). When the player walks into the next screen every position is shifted
    by one screen, so bugs that are still in the screen we left keep chasing and walk in from the edge.
*/

#ifndef ENHANCED_H
#define ENHANCED_H

#ifdef SF3500

struct SDL_Surface;
class CRITTER;

// Playing the enhanced game (set by the menu before start_game())
extern bool enhanced;

// Bug ranks
#define CT_DRONE	0	// the classic bug
#define CT_RUNNER	1	// small and fast, comes in packs
#define CT_STRIKER	2	// tougher, hits harder
#define CT_BRUTE	3	// huge and slow
#define NUM_CTYPES	4

struct CRITTER_STATS {
	int hp;				// health
	float accel;		// walking acceleration (the classic bug: 0.25)
	int dmg;			// damage to the player per tick of contact
	int hs;				// half size of the body for hits (pixels)
	float mass;			// how much bullets and blasts push it
	int score;			// what a kill is worth
};
extern const CRITTER_STATS ctype_stats[NUM_CTYPES];

// Sprites of the ranks: [rank][frame][angle]; rank 0 uses the classic critter_gfx
extern SDL_Surface *ctype_gfx[NUM_CTYPES][3][35];

// Match start / end (both game modes: the end restores the classic screen)
void enhanced_begin();
void enhanced_end();

// One game tick, after the player has moved
void enhanced_update();

// One spawn event of the hive (replaces the single bug of create_critters())
void enhanced_spawn_event();

// Most bugs alive at once in the enhanced game
#define ENHANCED_MAX_CRITTERS	90

// HUD: map, adaptation message
void enhanced_draw(SDL_Surface *bmp);

// Is the display showing the centre screen (the stone platform)?
bool world_in_centre();

// Collision lookup over all five screens (x,y in current-screen coordinates)
bool world_blocked(int x, int y, bool bullet);

// Where a bug at (x,y) should walk to: the player, or the doorway to the centre screen
void world_target(float x, float y, float *tx, float *ty);

// Guardians of the power cells: is the arm's guard alarmed, and may this guardian rest at its post?
bool enhanced_guard_alert(int arm);
bool enhanced_guard_resting(CRITTER *c);

// Special ammunition (dropped in the arms): damage factor of the player's current rounds against a bug rank,
// to be called when a round is fired and when it hits
float enhanced_ammo_mult(int rank);
void enhanced_ammo_fired();
void enhanced_ammo_hit(float x, float y);

// How fast the player walks (slower while carrying a power cell) and how fast a bug at (x,y) may walk
// (slower inside the west pylon's field)
float enhanced_player_speed();
float enhanced_bug_speed(float x, float y);

// The player threw a bomb or a mine (feeds the hive's adaptation)
void enhanced_note_throw();

// Shift everything by a whole screen (used when the player changes screen)
void shift_critters(float dx, float dy);
void shift_gibs(float dx, float dy);
void shift_bombs(float dx, float dy);
void shift_power_ups(float dx, float dy);
void shift_particles(float dx, float dy);
void shift_explosions(float dx, float dy);

// Power-up placed by the world (arm loot)
void create_power_up_at(int x, int y, int type, int life);

// Make a critter a given rank
void critter_set_type(CRITTER *c, int type);

#endif
#endif
