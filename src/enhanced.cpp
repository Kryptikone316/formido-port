/*
    Formido - Datafrog SF3500 port: the enhanced game (see include/enhanced.h).

    Pieces: the cross-shaped world, bug ranks and their sprites, the hive's director,
    the platform's health regeneration, arm loot and the HUD.
*/

#ifdef SF3500

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "init.h"
#include "config.h"
#include "game.h"
#include "player.h"
#include "critter.h"
#include "powerup.h"
#include "particle.h"
#include "explo.h"
#include "mymath.h"
#include "font.h"
#include "enhanced.h"
#include "tuning.h"

bool enhanced = false;

#ifdef FORMIDO_TESTHOOKS
static bool bot_in_match = false;
static int bot_calls = 0;
static int bot_peak_bugs = 0;
static int bot_tyrants_seen = 0;
static long bot_last_tick = -1;
static long bot_healed = 0, bot_platform_ticks = 0;
static long bot_dmg_total = 0, bot_dmg_ticks = 0, bot_alive_ticks = 0, bot_first_hurt = -1, bot_hits[8];
static long bot_far_ticks = 0, bot_stall_ticks = 0;		// bug-ticks 150+ pixels from the player, and those spent hardly moving
static int bot_prev_hp = 200;
static void bot_minute_log();
#endif

static int stat_kills[NUM_CTYPES];
static int stat_cells = 0;
static int stat_tyrants = 0;

const CRITTER_STATS ctype_stats[NUM_CTYPES] = {
	//  hp   accel  dmg  hs  mass  score
	{ 100, 0.25f, 1, 16, 1.0f, 1 },	// drone (the classic bug)
	{  40, 0.58f, 1, 11, 0.6f, 1 },	// runner
	{ 220, 0.33f, 2, 18, 1.8f, 3 },	// striker
	{ 700, 0.17f, 2, 28, 4.5f, 8 },	// brute
	{ 240, 0.50f, 2, 17, 1.5f, 4 },	// burrower
	{ 4500, 0.14f, 3, 40, 12.0f, 100 },	// Hive Tyrant
	{ 60, 0.42f, 2, 16, 0.8f, 1 },		// phantom (hallucination)
	{ 240, 0.50f, 2, 17, 1.5f, 8 },	// stunning burrower
};

SDL_Surface *ctype_gfx[NUM_CTYPES][3][35];

#define CELL_W	SCREEN_W
#define CELL_H	SCREEN_H


///////////////////////////////////////////////////////////////////////////////
// Helpers

static int fdiv(int a, int b) {
	return (a >= 0) ? a / b : -((-a + b - 1) / b);
}

static void get_rgb(SDL_Surface *s, int x, int y, Uint8 *r, Uint8 *g, Uint8 *b) {
	int bpp = s->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)s->pixels + y * s->pitch + x * bpp;
	Uint32 v = 0;
	switch(bpp) {
		case 1: v = *p; break;
		case 2: v = *(Uint16 *)p; break;
		case 3: v = p[0] | (p[1] << 8) | (p[2] << 16); break;
		default: v = *(Uint32 *)p; break;
	}
	SDL_GetRGB(v, s->format, r, g, b);
}


///////////////////////////////////////////////////////////////////////////////
// The world: a cross of five screens

// cell index: 0 centre, 1 north, 2 east, 3 south, 4 west
static const int CELL_GX[5] = { 0, 0, 1, 0, -1 };
static const int CELL_GY[5] = { 0, -1, 0, 1, 0 };

static int cell_index(int cx, int cy) {
	for(int i=0; i<5; i++)
		if(CELL_GX[i] == cx && CELL_GY[i] == cy)
			return i;
	return -1;
}

struct WSCREEN {
	SDL_Surface *orig;		// clean picture
	SDL_Surface *cur;		// picture with the blood and burn marks of this match
	unsigned char *col;		// collision: 0 free, 1 solid, 2 solid for walking only
};
static WSCREEN scr[5];
static SDL_Surface *center_cur = NULL, *center_orig = NULL;
static bool world_loaded = false;
static int gx = 0, gy = 0;		// the cell the display shows

static unsigned char *build_col(SDL_Surface *img) {
	unsigned char *c = new unsigned char[CELL_W * CELL_H];
	for(int y=0; y<CELL_H; y++) {
		for(int x=0; x<CELL_W; x++) {
			Uint8 r,g,b;
			get_rgb(img, x, y, &r, &g, &b);
			unsigned char v = 0;
			if(r == 255 && g == 255 && b == 255)
				v = 1;
			else if(r == 0 && g == 0 && b == 255)
				v = 2;
			else if(r == 255 && g == 0 && b == 0)
				v = 3;			// thorns: walkable, they scratch
			c[y * CELL_W + x] = v;
		}
	}
	return c;
}

static SDL_Surface *load_display(const char *name) {
	SDL_Surface *tmp = IMG_Load(DAT((char *)name));
	if(tmp == NULL)
		error_msg("Unable to load %s!\n%s", name, IMG_GetError());
	SDL_Surface *s = SDL_DisplayFormat(tmp);
	if(s == NULL)
		error_msg("Unable to load %s!\n%s", name, IMG_GetError());
	SDL_FreeSurface(tmp);
	return s;
}

static void load_world() {
	static const char *pic[5] = { NULL, "waste_n.dat", "waste_e.dat", "waste_s.dat", "waste_w.dat" };
	static const char *colf[5] = { NULL, "waste_n_col.dat", "waste_e_col.dat", "waste_s_col.dat", "waste_w_col.dat" };

	// the classic centre screen is kept for the classic game; the enhanced game has its own (the same ground and
	// pylons, with rock blocks in the four corners where the arms' cliff walls meet it)
	center_orig = orig_bground;
	center_cur = cur_bground;
	scr[0].orig = load_display("ground_enh.dat");
	scr[0].cur = SDL_DisplayFormat(scr[0].orig);
	{
		SDL_Surface *c0 = IMG_Load(DAT((char *)"collisionmap_enh.dat"));
		if(c0 == NULL)
			error_msg("Unable to load collisionmap_enh.dat!\n%s", IMG_GetError());
		scr[0].col = build_col(c0);
		SDL_FreeSurface(c0);
	}

	for(int i=1; i<5; i++) {
		scr[i].orig = load_display(pic[i]);
		scr[i].cur = SDL_DisplayFormat(scr[i].orig);
		SDL_Surface *c = IMG_Load(DAT((char *)colf[i]));
		if(c == NULL)
			error_msg("Unable to load %s!\n%s", colf[i], IMG_GetError());
		scr[i].col = build_col(c);
		SDL_FreeSurface(c);
	}
	world_loaded = true;
}

bool world_in_centre() {
	return gx == 0 && gy == 0;
}

bool world_blocked(int x, int y, bool bullet) {
	int cx = gx + fdiv(x, CELL_W);
	int cy = gy + fdiv(y, CELL_H);
	int i = cell_index(cx, cy);
	if(i < 0)
		return true;			// outside the cross: solid rock
	int lx = x - (cx - gx) * CELL_W;
	int ly = y - (cy - gy) * CELL_H;
	unsigned char v = scr[i].col[ly * CELL_W + lx];
	return bullet ? (v == 1) : (v == 1 || v == 2);
}

static bool open_spot(int x, int y) {
	return !world_blocked(x, y, false) && !world_blocked(x + 5, y, false) && !world_blocked(x - 5, y, false) &&
		!world_blocked(x, y + 5, false) && !world_blocked(x, y - 5, false);
}

bool enhanced_unstick(float *px, float *py) {
	int x = (int)*px, y = (int)*py;
	if(!world_blocked(x, y, false))
		return false;
	for(int r=2; r<=100; r+=2) {
		for(int k=0; k<24; k++) {
			float a = k * 6.2831853f / 24;
			int nx = x + (int)(cosf(a) * r), ny = y + (int)(sinf(a) * r);
			if(open_spot(nx, ny)) {
				*px = (float)nx;
				*py = (float)ny;
				return true;
			}
		}
	}
	return false;
}

// Thorns (the red plants' patches): can be walked through
static bool world_thorns(int x, int y) {
	int cx = gx + fdiv(x, CELL_W), cy = gy + fdiv(y, CELL_H);
	int i = cell_index(cx, cy);
	if(i < 0)
		return false;
	return scr[i].col[(y - (cy - gy) * CELL_H) * CELL_W + (x - (cx - gx) * CELL_W)] == 3;
}

// Where a bug heads for. In sight of the player: straight at them. Otherwise it follows a flow field: a breadth-first
// distance to the player over a coarse grid of the whole cross (8-pixel cells, open ground only, with a little clearance
// from the walls), so bugs walk round the cliff steps and rock islands instead of pushing into them and piling up.
extern long enhanced_ticks;

#define NAV		8
#define NAV_W	(3 * CELL_W / NAV)
#define NAV_H	(3 * CELL_H / NAV)
#define NAV_FAR	32767

static unsigned char nav_open[NAV_W * NAV_H];
static short nav_dist[NAV_W * NAV_H];
static int nav_queue[NAV_W * NAV_H];
static bool nav_built = false;
static long nav_stamp = -1000;
static int nav_src = -1;
#ifdef FORMIDO_TESTHOOKS
static long nav_us = 0, nav_floods = 0;
#endif

static bool abs_solid(int ax, int ay) {		// absolute world pixel: the centre screen starts at (CELL_W, CELL_H)
	if(ax < 0 || ay < 0 || ax >= 3 * CELL_W || ay >= 3 * CELL_H)
		return true;
	int i = cell_index(ax / CELL_W - 1, ay / CELL_H - 1);
	if(i < 0)
		return true;
	unsigned char v = scr[i].col[(ay % CELL_H) * CELL_W + (ax % CELL_W)];
	return v == 1 || v == 2;
}

static void nav_build() {
	for(int cy=0; cy<NAV_H; cy++) {
		for(int cx=0; cx<NAV_W; cx++) {
			int px = cx * NAV + NAV / 2, py = cy * NAV + NAV / 2;
			nav_open[cy * NAV_W + cx] = !(abs_solid(px, py) || abs_solid(px + 7, py) || abs_solid(px - 7, py) ||
				abs_solid(px, py + 7) || abs_solid(px, py - 7));
		}
	}
	nav_built = true;
}

static void nav_flood(int src) {
	for(int i=0; i<NAV_W*NAV_H; i++)
		nav_dist[i] = NAV_FAR;
	int head = 0, tail = 0;
	nav_dist[src] = 0;
	nav_queue[tail++] = src;
	while(head < tail) {
		int c = nav_queue[head++];
		int cx = c % NAV_W, cy = c / NAV_W;
		short d = nav_dist[c] + 1;
		if(cx > 0 && nav_open[c-1] && nav_dist[c-1] > d) { nav_dist[c-1] = d; nav_queue[tail++] = c-1; }
		if(cx < NAV_W-1 && nav_open[c+1] && nav_dist[c+1] > d) { nav_dist[c+1] = d; nav_queue[tail++] = c+1; }
		if(cy > 0 && nav_open[c-NAV_W] && nav_dist[c-NAV_W] > d) { nav_dist[c-NAV_W] = d; nav_queue[tail++] = c-NAV_W; }
		if(cy < NAV_H-1 && nav_open[c+NAV_W] && nav_dist[c+NAV_W] > d) { nav_dist[c+NAV_W] = d; nav_queue[tail++] = c+NAV_W; }
	}
	nav_src = src;
}

// open ground all the way between two absolute points (the ends are allowed to sit close to a wall)
static bool nav_clear(int ax, int ay, int bx, int by) {
	float dx = (float)(bx - ax), dy = (float)(by - ay);
	float len = sqrtf(dx * dx + dy * dy);
	int steps = (int)(len / 6.0f);
	for(int i=1; i<steps; i++) {
		float t = (float)i / steps;
		float px = ax + dx * t, py = ay + dy * t;
		if(len * t < 14 || len * (1 - t) < 20)
			continue;
		int cx = (int)px / NAV, cy = (int)py / NAV;
		if(cx < 0 || cy < 0 || cx >= NAV_W || cy >= NAV_H || !nav_open[cy * NAV_W + cx])
			return false;
	}
	return true;
}

void world_target(float x, float y, float *tx, float *ty) {
	*tx = player.x;
	*ty = player.y;
	if(!world_loaded)
		return;
#ifdef FORMIDO_TESTHOOKS
	if(getenv("FORMIDO_NONAV")) {		// the old straight-line steering, for comparisons
		int bi = cell_index(gx + fdiv((int)x, CELL_W), gy + fdiv((int)y, CELL_H));
		int pi = cell_index(gx + fdiv((int)player.x, CELL_W), gy + fdiv((int)player.y, CELL_H));
		if(!(bi < 0 || pi < 0 || bi == pi || bi == 0 || pi == 0)) {
			*tx = (float)(CELL_W / 2 - gx * CELL_W);
			*ty = (float)(CELL_H / 2 - gy * CELL_H);
		}
		return;
	}
#endif
	if(!nav_built)
		nav_build();
	int offx = (gx + 1) * CELL_W, offy = (gy + 1) * CELL_H;			// current screen frame -> absolute
	int bax = (int)x + offx, bay = (int)y + offy;
	int pax = (int)player.x + offx, pay = (int)player.y + offy;
	if(bax < 0 || bay < 0 || pax < 0 || pay < 0 || bax >= 3 * CELL_W || bay >= 3 * CELL_H || pax >= 3 * CELL_W || pay >= 3 * CELL_H)
		return;
	float ddx = player.x - x, ddy = player.y - y;
	if(ddx * ddx + ddy * ddy < 320.0f * 320.0f && nav_clear(bax, bay, pax, pay))
		return;															// in plain sight

	int src = (pay / NAV) * NAV_W + (pax / NAV);
	if(nav_src < 0 || enhanced_ticks - nav_stamp >= 10 || enhanced_ticks < nav_stamp) {		// about six times a second: bugs only need the general way round
#ifdef FORMIDO_TESTHOOKS
		struct timespec t0, t1;
		clock_gettime(CLOCK_MONOTONIC, &t0);
#endif
		nav_flood(src);
		nav_stamp = enhanced_ticks;
#ifdef FORMIDO_TESTHOOKS
		clock_gettime(CLOCK_MONOTONIC, &t1);
		nav_us += (t1.tv_sec - t0.tv_sec) * 1000000L + (t1.tv_nsec - t0.tv_nsec) / 1000L;
		nav_floods++;
#endif
	}

	int cx = bax / NAV, cy = bay / NAV;
	int moved = 0;
	for(int step=0; step<3; step++) {
		short cur = nav_dist[cy * NAV_W + cx];		// NAV_FAR when the bug stands within the wall clearance: any reachable neighbour will do
		short bd = NAV_FAR;
		float bscore = 1e9f;
		int bx = -1, by = -1;
		for(int k=0; k<8; k++) {
			static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
			int nx = cx + DX[k], ny = cy + DY[k];
			if(nx < 0 || ny < 0 || nx >= NAV_W || ny >= NAV_H || !nav_open[ny * NAV_W + nx])
				continue;
			if(DX[k] && DY[k] && (!nav_open[cy * NAV_W + nx] || !nav_open[ny * NAV_W + cx]))
				continue;			// no cutting round a corner
			short d = nav_dist[ny * NAV_W + nx];
			if(d >= NAV_FAR || (cur < NAV_FAR && d >= cur))
				continue;
			float px = (float)(nx * NAV + NAV / 2 - pax), py = (float)(ny * NAV + NAV / 2 - pay);
			float score = px * px + py * py;
			if(d < bd || (d == bd && score < bscore)) {
				bd = d;
				bscore = score;
				bx = nx;
				by = ny;
			}
		}
		if(bx < 0)
			break;
		cx = bx;
		cy = by;
		moved++;
	}
	if(moved) {
		*tx = (float)(cx * NAV + NAV / 2 - offx);
		*ty = (float)(cy * NAV + NAV / 2 - offy);
	}
}


///////////////////////////////////////////////////////////////////////////////
// Bug ranks: sprites made from the classic bug (scaled and recoloured)

static bool sprites_made = false;

// dark, mid and light colour of the rank (the classic bug's brightness is mapped onto this ramp)
static const int RAMP[NUM_CTYPES][3][3] = {
	{ {0,0,0}, {0,0,0}, {0,0,0} },
	{ {70,46,14}, {196,156,58}, {252,230,140} },		// runner: pale desert yellow
	{ {16,13,14}, {92,74,22}, {236,192,44} },			// striker: black with yellow
	{ {14,18,30}, {62,76,104}, {156,170,196} },			// brute: dark steel blue
	{ {22,14,10}, {112,86,60}, {232,216,184} },			// burrower: dark earth and bone
	{ {44,6,28}, {150,28,62}, {255,206,96} },			// Hive Tyrant: dark crimson with gold
	{ {40,10,90}, {150,70,230}, {120,255,230} },		// phantom: violet with a cyan glare
	{ {14,18,60}, {64,112,226}, {255,238,120} },		// stunning burrower: electric blue with a yellow glare
};
static const float SCALE[NUM_CTYPES] = { 1.0f, 0.70f, 1.12f, 1.70f, 1.20f, 2.30f, 1.0f, 1.20f };

static SDL_Surface *make_variant(SDL_Surface *src, int type) {
	float scale = SCALE[type];
	int nw = (int)(src->w * scale + 0.5f), nh = (int)(src->h * scale + 0.5f);
	SDL_Surface *dst = SDL_CreateRGBSurface(SDL_SWSURFACE, nw, nh, src->format->BitsPerPixel,
		src->format->Rmask, src->format->Gmask, src->format->Bmask, 0);
	if(!dst)
		return NULL;
	Uint32 key = SDL_MapRGB(dst->format, 255, 0, 255);
	Uint32 skey = (src->flags & SDL_SRCCOLORKEY) ? src->format->colorkey : key;
	SDL_FillRect(dst, NULL, key);

	// a surface that has been drawn with RLE acceleration must be unpacked before its pixels are read
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
				Uint8 *p = (Uint8 *)src->pixels + yy * src->pitch + xx * 2;
				Uint32 v = *(Uint16 *)p;
				if(v == skey)
					continue;
				Uint8 r,g,b;
				SDL_GetRGB(v, src->format, &r, &g, &b);
				ar += w * r; ag += w * g; ab += w * b; aa += w;
			}
			if(aa < 0.5f)
				continue;
			float r = ar / aa, g = ag / aa, b = ab / aa;
			float lum = (r * 0.30f + g * 0.59f + b * 0.11f) / 190.0f;
			if(lum > 1) lum = 1;
			const int (*rp)[3] = RAMP[type];
			float t = lum * 2.0f;
			int lo = (t < 1.0f) ? 0 : 1;
			float u = (t < 1.0f) ? t : t - 1.0f;
			int rr = (int)(rp[lo][0] + (rp[lo+1][0] - rp[lo][0]) * u);
			int gg = (int)(rp[lo][1] + (rp[lo+1][1] - rp[lo][1]) * u);
			int bb = (int)(rp[lo][2] + (rp[lo+1][2] - rp[lo][2]) * u);
			Uint32 out = SDL_MapRGB(dst->format, rr, gg, bb);
			*(Uint16 *)((Uint8 *)dst->pixels + y * dst->pitch + x * 2) = (Uint16)out;
		}
	}
	if(SDL_MUSTLOCK(src))
		SDL_UnlockSurface(src);
	SDL_SetColorKey(dst, SDL_SRCCOLORKEY|SDL_RLEACCEL, key);
	return dst;
}

static void make_sprites() {
	if(sprites_made)
		return;
	sprites_made = true;
	for(int t=0; t<NUM_CTYPES; t++) {
		if(t == CT_TYRANT)
			continue;						// made a few at a time while the Tyrant is on its way
		for(int f=0; f<3; f++) {
			for(int a=0; a<35; a++) {
				SDL_Surface *src = critter_gfx[f][a];
				if(t == 0 || src == NULL || src->format->BitsPerPixel != 16)
					ctype_gfx[t][f][a] = src;
				else
					ctype_gfx[t][f][a] = make_variant(src, t);
			}
		}
	}
}

static int hive_level = 0;			// grows by one with every Tyrant slain: all bugs a little tougher and quicker
static long boon_until = 0;			// the Tyrant's spoils last until this tick

void critter_set_type(CRITTER *c, int type) {
	if(type < 0 || type >= NUM_CTYPES)
		type = 0;
	c->type = type;
	extern long enhanced_ticks;
	float mul = 1.0f;
	if(type > 0) {
		float t = (float)(enhanced_ticks / 60);
		if(t > 900) t = 900;
		mul += tune.time_toughening * t / 900.0f;		// the hive's bugs toughen slowly
	}
	// the hive grows stronger with every Tyrant it loses: +3 % health and +1.5 % speed each time
	int lvl = hive_level > 30 ? 30 : hive_level;
	if(type == CT_TYRANT)
		mul = 1.0f + tune.tyrant_health_growth * lvl;		// the bosses themselves grow faster
	else
		mul *= 1.0f + tune.hive_health_per_tyrant * lvl;
	// no two bugs are quite the same: health +-15 %, speed +-10 %
	if(type == CT_TYRANT)
		c->maxhp = (int)(tune.tyrant_health * mul);
	else
		c->maxhp = (int)(ctype_stats[type].hp * mul * tune.bug_health_mult * RANDF(0.85f, 1.15f));
	c->health = c->maxhp;
	c->hs = ctype_stats[type].hs;
	c->spd = RANDF(0.9f, 1.1f) * (type == CT_TYRANT ? 1.0f : (1.0f + tune.hive_speed_per_tyrant * lvl) * tune.bug_speed_mult);
}


///////////////////////////////////////////////////////////////////////////////
// The hive's director

long enhanced_ticks = 0;
static long last_damage_tick = 0;
static int thorn_out = 100;			// ticks since the player last stood in thorns
static long thorn_hit = 0;
static int prev_health = 200;
static int regen_acc = 0;

static float hist_x[8], hist_y[8];
static int hist_n = 0;
static float throws_recent = 0;
static int adapt_camp = 0, adapt_kite = 0, adapt_bombs = 0, adapt_low = 0;
static int dominant = 0;
static char msg[64] = "";
static int msg_timer = 0;

// special ammunition: type = the bug rank it is made against
#define AMMO_ROUNDS ((int)tune.ammo_rounds)
#define AMMO_SECONDS ((int)tune.ammo_seconds)
#define NUM_AMMO 4			// one kind of rounds for each of the first four ranks
static const char *AMMO_NAME[NUM_AMMO] = { "hollow-point", "flechette", "incendiary", "armour-piercing" };
static const char *RANK_NAME[NUM_AMMO] = { "drones", "runners", "strikers", "brutes" };
static const int AMMO_COL[NUM_AMMO][3] = { {214,96,60}, {250,226,120}, {236,176,40}, {130,164,224} };
static const float AMMO_BONUS[NUM_AMMO] = { 2.0f, 2.2f, 1.9f, 2.1f };
static int ammo_type = -1;					// the rounds the player has loaded, -1 for the standard ones
static int ammo_left = 0;
static long ammo_until = 0;					// tick at which the rounds run out whatever is left
static float ammo_recent[NUM_AMMO];		// how much of each kind was fired lately: the hive notices
static float ammo_counter[NUM_AMMO];		// 0..1: how hard the hive pulls that rank back
static bool ammo_counter_on[NUM_AMMO];

static long arm_last_loot[5];
static int arm_timer[5];

void enhanced_note_throw() {
	throws_recent += 1.0f;
}

static void set_message(const char *s) {
	snprintf(msg, sizeof(msg), "%s", s);
	msg_timer = 60 * 5;
}

static void director_sample() {
	float wx = player.x + gx * CELL_W, wy = player.y + gy * CELL_H;
	for(int i=0; i<7; i++) {
		hist_x[i] = hist_x[i+1];
		hist_y[i] = hist_y[i+1];
	}
	hist_x[7] = wx;
	hist_y[7] = wy;
	if(hist_n < 8)
		hist_n++;
	throws_recent *= 0.965f;		// forgets a throw in about half a minute

	adapt_camp = adapt_kite = adapt_bombs = adapt_low = 0;
	if(hist_n >= 8) {
		float mx = 0, my = 0, path = 0;
		for(int i=0; i<8; i++) { mx += hist_x[i]; my += hist_y[i]; }
		mx /= 8; my /= 8;
		float far_ = 0;
		for(int i=0; i<8; i++) {
			float d = distance(hist_x[i], hist_y[i], mx, my);
			if(d > far_) far_ = d;
			if(i > 0)
				path += distance(hist_x[i], hist_y[i], hist_x[i-1], hist_y[i-1]);
		}
		if(far_ < 90)
			adapt_camp = 1;
		else if(path / 7.0f > 170.0f)
			adapt_kite = 1;
	}
	if(throws_recent >= 3.0f)
		adapt_bombs = 1;
	if(player.ok && player.health < 70)
		adapt_low = 1;

	// the hive notices which rounds are being fired and starts to hold that rank back
	for(int r=0; r<NUM_AMMO; r++) {
		ammo_recent[r] *= 0.9f;
		float c = (ammo_recent[r] - 30.0f) / 120.0f;
		ammo_counter[r] = c < 0 ? 0 : (c > 1 ? 1 : c);
		if(ammo_counter[r] > 0.3f && !ammo_counter_on[r]) {
			ammo_counter_on[r] = true;
			char buf[64];
			snprintf(buf, sizeof(buf), "the hive adapts to %s rounds", AMMO_NAME[r]);
			set_message(buf);
		}
		else if(ammo_counter[r] < 0.1f)
			ammo_counter_on[r] = false;
	}

	int now = adapt_camp ? 1 : adapt_bombs ? 2 : adapt_low ? 3 : adapt_kite ? 4 : 0;
	if(now != dominant) {
		dominant = now;
		switch(now) {
			case 1: set_message("the hive adapts: heavy bugs"); break;
			case 2: set_message("the hive adapts: scattered swarms"); break;
			case 3: set_message("the hive smells blood"); break;
			case 4: set_message("the hive adapts: fast swarms"); break;
		}
	}
}

static int count_type(int type) {
	int n = 0;
	for(CRITTER *c = critter_first; c; c = c->next)
		if(c->type == type && !c->remove && !c->guard)
			n++;
	return n;
}

static int pick_type() {
	float t = (float)(enhanced_ticks / 60);
	float w[NUM_CTYPES];
	w[CT_DRONE] = 100.0f;
	w[CT_BURROWER] = 0;
	w[CT_TYRANT] = 0;
	w[CT_PHANTOM] = 0;
	w[CT_STUNNER] = 0;
	w[CT_RUNNER] = (t < 20) ? 0 : (t - 20) * 0.6f; if(w[CT_RUNNER] > 40) w[CT_RUNNER] = 40;
	w[CT_STRIKER] = (t < 50) ? 0 : (t - 50) * 0.5f; if(w[CT_STRIKER] > 30) w[CT_STRIKER] = 30;
	w[CT_BRUTE] = (t < 100) ? 0 : (t - 100) * 0.25f; if(w[CT_BRUTE] > 14) w[CT_BRUTE] = 14;
	if(count_type(CT_BRUTE) >= 1 + (int)(t / 240) + (enhanced_tyrant_active() ? 3 : 0))
		w[CT_BRUTE] = 0;

	if(enhanced_tyrant_active()) {			// the heavy bugs screen for the Tyrant; the weak ones will not come near it
		w[CT_DRONE] *= 0.15f;
		w[CT_RUNNER] *= 0.15f;
		w[CT_STRIKER] = (w[CT_STRIKER] < 12 ? 12 : w[CT_STRIKER]) * 2.5f;
		w[CT_BRUTE] = (w[CT_BRUTE] < 6 ? 6 : w[CT_BRUTE]) * 2.0f;
	}
	if(adapt_camp) { w[CT_BRUTE] *= 2.4f; w[CT_STRIKER] *= 1.6f; if(t >= 40 && w[CT_BRUTE] == 0 && count_type(CT_BRUTE) < 1) w[CT_BRUTE] = 6; }
	if(adapt_kite) { w[CT_RUNNER] *= 1.6f; if(t >= 10 && w[CT_RUNNER] < 10) w[CT_RUNNER] = 10; }
	if(adapt_bombs) { w[CT_RUNNER] *= 1.8f; w[CT_STRIKER] *= 1.3f; }
	if(adapt_low) { w[CT_RUNNER] *= 1.6f; }

	for(int i=0; i<NUM_AMMO; i++)
		w[i] *= 1.0f - 0.7f * ammo_counter[i];
	w[CT_RUNNER] *= tune.weight_runner;
	if(count_type(CT_RUNNER) >= (int)tune.runner_max_alive)
		w[CT_RUNNER] = 0;
	w[CT_STRIKER] *= tune.weight_striker;
	w[CT_BRUTE] *= tune.weight_brute;

	float sum = 0;
	for(int i=0; i<NUM_CTYPES; i++) sum += w[i];
	float r = RANDF(0, sum);
	for(int i=0; i<NUM_CTYPES; i++) {
		if(r < w[i])
			return i;
		r -= w[i];
	}
	return CT_DRONE;
}

// Is (lx,ly) of an arm screen close to one of its closed (rock) sides?
static bool near_closed_side(int ci, int lx, int ly) {
	const int B = 120;
	switch(ci) {
		case 1: return lx < B || lx > CELL_W - B || ly < B;				// north arm opens to the south
		case 3: return lx < B || lx > CELL_W - B || ly > CELL_H - B;	// south arm opens to the north
		case 2: return ly < B || ly > CELL_H - B || lx > CELL_W - B;	// east arm opens to the west
		case 4: return ly < B || ly > CELL_H - B || lx < B;				// west arm opens to the east
	}
	return false;
}

static bool spawn_point(float *ox, float *oy, bool *visible) {
	int here = cell_index(gx, gy);
	for(int tries=0; tries<60; tries++) {
		// the screens other than the one we are on weigh twice as much; a centre screen
		// has no closed sides to climb out of, so nothing spawns in it while we stand there
		int ci = (RAND(0,4));
		if(ci == here) {
			if(here == 0 || (RAND(0,2)) != 0)
				continue;
		}
		int lx = (RAND(70, CELL_W - 70)), ly = (RAND(70, CELL_H - 70));
		float x = (float)(lx + (CELL_GX[ci] - gx) * CELL_W);
		float y = (float)(ly + (CELL_GY[ci] - gy) * CELL_H);
		if(ci == here && !near_closed_side(ci, lx, ly))
			continue;
		if(distance(x, y, player.x, player.y) < 280)
			continue;
		if(world_blocked((int)x, (int)y, false))
			continue;
		*ox = x;
		*oy = y;
		*visible = (ci == here);
		return true;
	}
	return false;
}

void enhanced_spawn_event() {
	int roaming = 0;
	for(CRITTER *c = critter_first; c; c = c->next)
		if(!c->guard && !c->remove)
			roaming++;
	if(roaming >= (int)tune.max_roaming_bugs || num_critters >= (int)tune.max_roaming_bugs + 60)
		return;
	int type = pick_type();
	int n = 1;
	if(type == CT_RUNNER)
		n = (RAND((int)tune.runner_pack_min, (int)tune.runner_pack_max)) + (adapt_bombs ? 1 : 0);
	float x, y;
	bool visible;
	if(!spawn_point(&x, &y, &visible))
		return;
	for(int i=0; i<n; i++) {
		float px = x + ((i == 0) ? 0 : RANDF(-34, 34));
		float py = y + ((i == 0) ? 0 : RANDF(-34, 34));
		if(world_blocked((int)px, (int)py, false)) {
			px = x;
			py = y;
		}
		CRITTER *c = add_critter(px, py, 0);
		critter_set_type(c, type);
		if(visible) {
			for(int k=0; k<6; k++)
				add_particle(px, py, RANDF(-1.5, 1.5), RANDF(-2.0, 0.3), (RAND(120,170)), (RAND(80,110)), (RAND(40,60)), (RAND(10,25)));
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// Power cells and the pylons' mode
//
// Every arm holds one power cell. Carried to the stone platform it sets what the pylons offer:
//   north cell: the north pylon becomes an automatic turret
//   east cell:  the pylons drop bombs and mines on the platform
//   south cell: a medbay, the platform heals fast
//   west cell:  a slowing field around the platform
// One mode at a time: a new cell switches it, the same cell again adds time.

static const int ARM_COL[6][3] = { {0,0,0}, {90,160,255}, {255,165,60}, {100,230,120}, {205,120,255}, {120,235,120} };	// [5]: the healing plant
static const char *MODE_NAME[5] = { "", "turret", "supply", "medbay", "field" };
static const char *DIR_NAME[5] = { "", "north", "east", "south", "west" };
static const int PYL_X[5] = { 0, 320, 478, 320, 160 };		// pylon centres on the centre screen
static const int PYL_Y[5] = { 0, 78, 240, 400, 240 };

struct POWERCELL {
	float lx, ly;			// position on its arm's own screen
	bool present;
	long back_tick;			// when it is there again after being taken
};
static POWERCELL pcell[5];
static bool need_guards[5];		// the arm's guardians have yet to be placed (first visit, or after the cell was delivered)
static bool sealed[5];			// the cell cannot be taken: guardians alive or not yet placed
static bool alert[5];			// the arm's guardians are awake
static float garrison[5];		// how strong this time's guardians are: 0.7 (light) .. 1.4 (heavy)

static void roll_garrison(int i) {
	garrison[i] = RANDF(tune.garrison_min, tune.garrison_max);
}

// 1 light, 2 average, 3 heavy: shown on the map next to the arm
static int garrison_level(int i) {
	return garrison[i] < 0.9f ? 1 : (garrison[i] < 1.15f ? 2 : 3);
}
static int carrying = 0;			// arm of the cell the player carries, 0 for none
static int mode = 0;				// arm of the cell that set the pylons' mode, 0 for none
static long mode_until = 0;
static SDL_Surface *cell_spr[5];	// the cell
static SDL_Surface *glow_spr[6];
static SDL_Surface *glow_item[9];		// soft light under the pickups: [0-3] ammo, [4-7] traps, [8] bunker kit	// soft light in the arm's colour ([5]: the healing plant)

#define MODE_SECONDS	((int)tune.pylon_mode_seconds)
#define MODE_ADD_SECONDS 90
#define MODE_MAX_SECONDS 240

static bool mode_on() { return mode != 0 && enhanced_ticks < mode_until; }

float enhanced_player_speed() {
	return (carrying ? tune.carry_speed : 1.0f) * (enhanced_ticks < boon_until ? tune.boon_speed_mult : 1.0f);
}

float enhanced_bug_speed(float x, float y) {
	if(mode_on() && mode == 4) {
		float cx = (float)(CELL_W / 2 - gx * CELL_W), cy = (float)(CELL_H / 2 - gy * CELL_H);
		if(distance(x, y, cx, cy) < 210)
			return 0.5f;
	}
	return 1.0f;
}

static SDL_Surface *make_cell_sprite(int arm) {
	const int S = 28;
	SDL_Surface *sp = SDL_CreateRGBSurface(SDL_SWSURFACE, S, S, 16, 0xf800, 0x07e0, 0x001f, 0);
	Uint32 key = SDL_MapRGB(sp->format, 255, 0, 255);
	SDL_FillRect(sp, NULL, key);
	for(int y=0; y<S; y++) {
		for(int x=0; x<S; x++) {
			float dx = x - (S - 1) / 2.0f, dy = y - (S - 1) / 2.0f;
			float ax = fabsf(dx), ay = fabsf(dy);
			float hex = fmaxf(ay, ax * 0.866f + ay * 0.5f);		// 0 at the centre, 1 at the rim, for a hexagon
			hex /= 12.5f;
			if(hex > 1.0f)
				continue;
			float lit = 0.75f + 0.35f * (-dx * 0.5f - dy * 0.6f) / 12.0f;
			float r, g, b;
			if(hex > 0.86f) {				// dark rim
				r = 40; g = 40; b = 44;
			}
			else if(hex > 0.5f) {			// grey metal body
				r = 116 * lit; g = 112 * lit; b = 118 * lit;
			}
			else {							// glowing core in the arm's colour
				float k = 1.0f - hex * 0.9f;
				r = ARM_COL[arm][0] * (0.55f + 0.45f * k) + 80 * k * k;
				g = ARM_COL[arm][1] * (0.55f + 0.45f * k) + 80 * k * k;
				b = ARM_COL[arm][2] * (0.55f + 0.45f * k) + 80 * k * k;
			}
			*(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format,
				(int)fminf(r, 255), (int)fminf(g, 255), (int)fminf(b, 255));
		}
	}
	SDL_SetColorKey(sp, SDL_SRCCOLORKEY, key);
	return sp;
}

static SDL_Surface *make_glow_rgb(int cr, int cg, int cb, int S, float strength) {
	SDL_Surface *sp = SDL_CreateRGBSurface(SDL_SWSURFACE, S, S, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
	for(int y=0; y<S; y++) {
		for(int x=0; x<S; x++) {
			float d = sqrtf((x - S / 2.0f + 0.5f) * (x - S / 2.0f + 0.5f) + (y - S / 2.0f + 0.5f) * (y - S / 2.0f + 0.5f)) / (S / 2.0f);
			float a = d >= 1.0f ? 0.0f : (1.0f - d) * (1.0f - d);
			Uint8 *p = (Uint8 *)sp->pixels + y * sp->pitch + x * 4;
			p[0] = cr; p[1] = cg; p[2] = cb;
			p[3] = (Uint8)(a * strength);
		}
	}
	return sp;
}

static SDL_Surface *make_glow_sprite(int arm) {
	return make_glow_rgb(ARM_COL[arm][0], ARM_COL[arm][1], ARM_COL[arm][2], 72, 150.0f);
}

static void make_cell_sprites() {
	if(cell_spr[1])
		return;
	for(int i=1; i<5; i++) {
		cell_spr[i] = make_cell_sprite(i);
		glow_spr[i] = make_glow_sprite(i);
	}
	glow_spr[5] = make_glow_sprite(5);
	// each kind of pickup has its own colour of light under it: ammo in its rank's colour, traps in theirs, the kit white
	static const int IG[9][3] = { {214,96,60}, {250,226,120}, {236,176,40}, {130,164,224},
		{255,140,40}, {196,196,176}, {230,215,70}, {0,0,0}, {240,240,230} };
	for(int i=0; i<9; i++)
		if(i != 7)
			glow_item[i] = make_glow_rgb(IG[i][0], IG[i][1], IG[i][2], 56, 120.0f);
}

// somewhere deep in the arm, away from the open side, not inside rock, and not where it was last time
static void place_cell(int i) {
	static const int LX0[5] = { 0, 100, 400, 100, 90 }, LX1[5] = { 0, 540, 550, 540, 240 };
	static const int LY0[5] = { 0, 80, 90, 250, 90 }, LY1[5] = { 0, 230, 390, 400, 390 };
	float old_x = pcell[i].lx, old_y = pcell[i].ly;
	bool had_old = (old_x != 0 || old_y != 0);
	bool found = false;
	for(int tries=0; tries<120 && !found; tries++) {
		int lx = (RAND(LX0[i], LX1[i])), ly = (RAND(LY0[i], LY1[i]));
		if(had_old && tries < 100 && distance((float)lx, (float)ly, old_x, old_y) < 170)
			continue;
		int x = lx + (CELL_GX[i] - gx) * CELL_W, y = ly + (CELL_GY[i] - gy) * CELL_H;
		if(world_blocked(x, y, false) || world_blocked(x + 24, y, false) || world_blocked(x - 24, y, false) ||
		   world_blocked(x, y + 24, false) || world_blocked(x, y - 24, false))
			continue;
		pcell[i].lx = (float)lx;
		pcell[i].ly = (float)ly;
		found = true;
	}
	if(!found && !had_old) {
		pcell[i].lx = (float)((LX0[i] + LX1[i]) / 2);
		pcell[i].ly = (float)((LY0[i] + LY1[i]) / 2);
	}
}

static void place_cells() {
	for(int i=1; i<5; i++) {
		pcell[i].present = true;
		pcell[i].back_tick = 0;
		pcell[i].lx = pcell[i].ly = 0;
		place_cell(i);
	}
}

// What stands watch over each arm's cell: drones, runners, strikers, brutes (the first brute is the warden)
static const int GUARD_PLAN[5][4] = { {0,0,0,0}, {0,2,2,1}, {5,2,0,1}, {4,0,2,1}, {0,8,2,0} };

static void spawn_cell_guards(int arm) {
	need_guards[arm] = false;
	int mins = (int)(enhanced_ticks / 3600);
	int plan[4];
	for(int k=0; k<4; k++)
		plan[k] = GUARD_PLAN[arm][k];
	plan[CT_STRIKER] += (mins / 2 > 3) ? 3 : mins / 2;		// they grow with the match
	if(mins >= 8 && plan[CT_BRUTE] > 0)
		plan[CT_BRUTE]++;
	float f = garrison[arm];
	for(int k=0; k<4; k++) {
		if(plan[k] > 0) {
			plan[k] = (int)(plan[k] * f + 0.5f);
			if(plan[k] < 1)
				plan[k] = 1;
		}
	}
	float cx = pcell[arm].lx + (CELL_GX[arm] - gx) * CELL_W;
	float cy = pcell[arm].ly + (CELL_GY[arm] - gy) * CELL_H;
	bool warden = true;
	for(int type=CT_BRUTE; type>=0; type--) {
		for(int k=0; k<plan[type]; k++) {
			for(int tries=0; tries<60; tries++) {
				float rad = (type == CT_BRUTE && warden) ? RANDF(45, 80) : RANDF(70, 175);
				float ang = RANDF(0, 6.2831853f);
				float x = cx + cosf(ang) * rad, y = cy + sinf(ang) * rad * 0.85f;
				float lx = x - (CELL_GX[arm] - gx) * CELL_W, ly = y - (CELL_GY[arm] - gy) * CELL_H;
				if(lx < 50 || lx > CELL_W - 50 || ly < 50 || ly > CELL_H - 50)
					continue;
				if(world_blocked((int)x, (int)y, false) || world_blocked((int)x + 14, (int)y, false) || world_blocked((int)x - 14, (int)y, false) ||
				   world_blocked((int)x, (int)y + 14, false) || world_blocked((int)x, (int)y - 14, false))
					continue;
				bool crowded = false;
				for(CRITTER *o = critter_first; o; o = o->next)
					if(o->guard == arm && distance(o->x, o->y, x, y) < 26) { crowded = true; break; }
				if(crowded)
					continue;
				CRITTER *c = add_critter(x, y, RANDF(0, 359));
				critter_set_type(c, type);
				c->maxhp = (int)(c->maxhp * sqrtf(f));				// a heavy garrison is bigger, a bit tougher and quicker
				c->spd *= 0.9f + 0.1f * f;
				if(type == CT_BRUTE && warden) {
					c->maxhp = c->maxhp * 13 / 10;
					warden = false;
				}
				c->health = c->maxhp;
				c->guard = arm;
				c->hx = x;
				c->hy = y;
				break;
			}
		}
	}
}

bool enhanced_guard_alert(int arm) {
	return arm > 0 && arm < 5 && alert[arm];
}

bool enhanced_guard_resting(CRITTER *c) {
	if(alert[c->guard])
		return false;
	if(distance(c->x, c->y, c->hx, c->hy) >= 24)
		return false;
	c->dirx = c->diry = 0;
	if(c->health < c->maxhp)
		c->health = (c->health + 2 > c->maxhp) ? c->maxhp : c->health + 2;
	return true;
}

static void turret_fire(float px, float py) {
	CRITTER *best = NULL;
	float bd = 250;
	for(CRITTER *c = critter_first; c; c = c->next) {
		if(c->remove || c->health <= 0 || c->hidden)
			continue;
		float d = distance(c->x, c->y, px, py);
		if(d < bd) {
			bd = d;
			best = c;
		}
	}
	if(!best)
		return;
	float ang = compute_angle(best->x, best->y, px, py);
	best->health -= enhanced_damage(best, 25);
	best->blood((RAND(2,5)), ang);
	if(best->health <= 0)
		best->die(ang);
	for(int k=1; k<=6; k++)
		add_particle(px + (best->x - px) * k / 7.0f, py + (best->y - py) * k / 7.0f, 0, 0, 255, 235, 140, 4);
	if(config.sound && (RAND(0,2)) == 0) {
		int chan = Mix_PlayChannel(-1, snd_bullet[(RAND(0,3))], 0);
		int pan = (int)(px * 255 / SCREEN_W);
		if(pan < 0) pan = 0;
		if(pan > 255) pan = 255;
		Mix_SetPanning(chan, 255 - pan, pan);
	}
}

static bool carrying_arm_is(int arm) {
	return carrying == arm;
}

static void cells_update(int here) {
	// cells come back some time after they were taken (with fresh guardians)
	for(int i=1; i<5; i++)
		if(!pcell[i].present && enhanced_ticks >= pcell[i].back_tick && !carrying_arm_is(i)) {
			pcell[i].present = true;
			need_guards[i] = true;
			roll_garrison(i);				// a different garrison...
			place_cell(i);					// ...in a new spot every time
			if(i != here) {
				static const char *WORD[4] = { "", "light", "average", "heavy" };
				char buf[64];
				snprintf(buf, sizeof(buf), "a new %s cell has formed: %s guard", DIR_NAME[i], WORD[garrison_level(i)]);
				set_message(buf);
			}
		}

	// the guardians: alive, hurt, awake; the cell is sealed while any of them lives
	int alive[5] = { 0, 0, 0, 0, 0 };
	bool hurt[5] = { false, false, false, false, false };
	for(CRITTER *c = critter_first; c; c = c->next) {
		if(c->guard && !c->remove) {
			alive[c->guard]++;
			if(c->health < c->maxhp)
				hurt[c->guard] = true;
		}
	}
	for(int i=1; i<5; i++) {
		bool was_sealed = sealed[i];
		sealed[i] = need_guards[i] || alive[i] > 0;
		bool was_alert = alert[i];
		if(i == here && player.ok && alive[i] > 0) {
			float cx = pcell[i].lx + (CELL_GX[i] - gx) * CELL_W, cy = pcell[i].ly + (CELL_GY[i] - gy) * CELL_H;
			if(hurt[i] || distance(player.x, player.y, cx, cy) < 300)
				alert[i] = true;
		}
		else
			alert[i] = false;
		if(alert[i] && !was_alert)
			set_message("the guardians wake");
		if(was_sealed && !sealed[i] && pcell[i].present) {
			char buf[64];
			snprintf(buf, sizeof(buf), "the %s cell is free", DIR_NAME[i]);
			set_message(buf);
			kills += 5;
			float cx = pcell[i].lx + (CELL_GX[i] - gx) * CELL_W, cy = pcell[i].ly + (CELL_GY[i] - gy) * CELL_H;
			for(int f=0; f<30; f++)
				add_particle(cx, cy, RANDF(-2.5, 2.5), RANDF(-2.5, 2.5), ARM_COL[i][0], ARM_COL[i][1], ARM_COL[i][2], (RAND(20,50)));
		}
	}

	if(player.ok) {
		// pick one up
		if(here > 0 && !carrying && pcell[here].present && !sealed[here]) {
			float cx = pcell[here].lx + (CELL_GX[here] - gx) * CELL_W;
			float cy = pcell[here].ly + (CELL_GY[here] - gy) * CELL_H;
			if(distance(player.x, player.y, cx, cy) < 28) {
				carrying = here;
				pcell[here].present = false;
				pcell[here].back_tick = enhanced_ticks + 60 * 60 * 10;	// until it is delivered
				char buf[64];
				snprintf(buf, sizeof(buf), "%s cell taken: bring it to the platform", DIR_NAME[here]);
				set_message(buf);
				if(config.sound)
					Mix_PlayChannel(-1, snd_power_up, 0);
			}
		}
		// deliver it
		if(carrying && gx == 0 && gy == 0) {
			float dx = (player.x - CELL_W / 2) / 118.0f, dy = (player.y - CELL_H / 2) / 128.0f;
			if(dx * dx + dy * dy < 1.0f) {
				if(mode_on() && mode == carrying) {
					mode_until += 60 * MODE_ADD_SECONDS;
					if(mode_until > enhanced_ticks + 60 * MODE_MAX_SECONDS)
						mode_until = enhanced_ticks + 60 * MODE_MAX_SECONDS;
				}
				else {
					mode = carrying;
					mode_until = enhanced_ticks + 60 * MODE_SECONDS;
				}
				char buf[64];
				snprintf(buf, sizeof(buf), "%s cell: pylons are now a %s", DIR_NAME[carrying], MODE_NAME[carrying]);
				set_message(buf);
				pcell[carrying].back_tick = enhanced_ticks + (long)(60 * RANDF(tune.cell_respawn_min_seconds, tune.cell_respawn_max_seconds));	// a long wait, a little different each time
				pcell[carrying].present = false;
				carrying = 0;
				stat_cells++;
				kills += 5;
				if(config.sound)
					Mix_PlayChannel(-1, snd_mushroom, 0);
				for(int f=0; f<24; f++)
					add_particle(player.x, player.y, RANDF(-2.5, 2.5), RANDF(-2.5, 2.5), ARM_COL[mode][0], ARM_COL[mode][1], ARM_COL[mode][2], (RAND(20,50)));
			}
		}
	}

	// what the pylons do
	if(mode_on()) {
		float ox = (float)(-gx * CELL_W), oy = (float)(-gy * CELL_H);		// centre screen in current coordinates
		if(mode == 1 && enhanced_ticks % 10 == 0)
			turret_fire(PYL_X[1] + ox, PYL_Y[1] + oy);
		if(mode == 2 && enhanced_ticks % (60 * 12) == 0) {
			int free_items = 0;
			for(int i=0; i<MAX_POWER_UPS; i++)
				if(!power_up[i].ok)
					free_items++;
			if(free_items > 2)
				create_power_up_at((int)(CELL_W / 2 + RANDF(-70, 70) + ox), (int)(CELL_H / 2 + RANDF(-50, 70) + oy),
					(RAND(0,1)) ? POWER_UP_BOMB : POWER_UP_MINE, 60 * 14);
		}
		if((RAND(0,19)) == 0)
			add_particle(PYL_X[mode] + ox + RANDF(-6, 6), PYL_Y[mode] + oy - 6, RANDF(-0.2, 0.2), RANDF(-0.7, -0.3),
				ARM_COL[mode][0], ARM_COL[mode][1], ARM_COL[mode][2], (RAND(25,45)));
	}
}


///////////////////////////////////////////////////////////////////////////////
// Healing plants: a rare green plant in the arms. Standing near it mends a little; it shrivels as it is used
// (and as time passes) and then disappears.

#define HEAL_POOL		((int)tune.plant_pool)
#define HEAL_LIFE		(60 * 70)	// ticks until it withers on its own
#define HEAL_RADIUS		52
#define MAX_HEAL_PLANTS	3

struct HEAL_PLANT {
	float x, y;
	int pool;
	int life;
	bool ok;
};
static HEAL_PLANT hplant[MAX_HEAL_PLANTS];
static int hplant_timer = 0;
static bool hplant_ready = false;
static int hplant_messages = 0;
static SDL_Surface *hplant_img = NULL;		// four frames, 80 x 80 each

void spawn_heal_plant_at(float x, float y);
static bool spawn_heal_plant(int here) {
	int slot = -1;
	for(int i=0; i<MAX_HEAL_PLANTS; i++) {
		if(!hplant[i].ok) { slot = i; break; }
	}
	if(slot < 0)
		return false;
#ifdef FORMIDO_TESTHOOKS
	if(getenv("FORMIDO_HEAL") && atoi(getenv("FORMIDO_HEAL")) == 2) {		// test: right next to the player
		hplant[slot].x = player.x + 30;
		hplant[slot].y = player.y + 10;
		hplant[slot].pool = HEAL_POOL;
		hplant[slot].life = HEAL_LIFE;
		hplant[slot].ok = true;
		return true;
	}
#endif
	for(int tries=0; tries<60; tries++) {
		int lx = (RAND(100, CELL_W - 100)), ly = (RAND(100, CELL_H - 100));
		int x = lx + (CELL_GX[here] - gx) * CELL_W, y = ly + (CELL_GY[here] - gy) * CELL_H;
		if(world_blocked(x, y, false) || world_blocked(x + 16, y, false) || world_blocked(x - 16, y, false) ||
		   world_blocked(x, y + 16, false) || world_blocked(x, y - 16, false) || world_thorns(x, y))
			continue;
		if(distance((float)x, (float)y, player.x, player.y) < 110)
			continue;
		float cx = pcell[here].lx + (CELL_GX[here] - gx) * CELL_W, cy = pcell[here].ly + (CELL_GY[here] - gy) * CELL_H;
		if(distance((float)x, (float)y, cx, cy) < 90)
			continue;
		hplant[slot].x = (float)x;
		hplant[slot].y = (float)y;
		hplant[slot].pool = HEAL_POOL;
		hplant[slot].life = HEAL_LIFE;
		hplant[slot].ok = true;
		if(hplant_messages < 2) {
			hplant_messages++;
			set_message("a healing plant has bloomed nearby");
		}
		return true;
	}
	return false;
}

void spawn_heal_plant_at(float x, float y) {
	if(!hplant_img)
		return;
	int slot = -1;
	for(int i=0; i<MAX_HEAL_PLANTS; i++) {
		if(!hplant[i].ok) { slot = i; break; }
	}
	if(slot < 0 || world_blocked((int)x, (int)y, false))
		return;
	hplant[slot].x = x;
	hplant[slot].y = y;
	hplant[slot].pool = HEAL_POOL / 2;			// a smaller one: it grew out of a vision
	hplant[slot].life = HEAL_LIFE;
	hplant[slot].ok = true;
}

static void heal_plant_update(int here) {
	if(!hplant_img)
		return;
	// the wait between plants runs on its own; once it is over, the next plant grows in the first arm the player
	// is out in (a spot of open sand, away from rock, thorns and the power cell)
	if(!hplant_ready && --hplant_timer <= 0)
		hplant_ready = true;
	if(hplant_ready && here > 0 && player.ok) {
		bool already = false;
		for(int i=0; i<MAX_HEAL_PLANTS; i++)
			if(hplant[i].ok && cell_index(gx + fdiv((int)hplant[i].x, CELL_W), gy + fdiv((int)hplant[i].y, CELL_H)) == here)
				already = true;
		if(!already && spawn_heal_plant(here)) {
			hplant_ready = false;
			hplant_timer = (int)(60 * RANDF(tune.plant_min_seconds, tune.plant_max_seconds));		// a long wait before the next one: the pylon stays the reliable heal
		}
	}
	for(int i=0; i<MAX_HEAL_PLANTS; i++) {
		if(!hplant[i].ok)
			continue;
		if(--hplant[i].life <= 0 || hplant[i].pool <= 0) {
			hplant[i].ok = false;
			continue;
		}
		if(player.ok && player.health < 200 && distance(player.x, player.y, hplant[i].x, hplant[i].y) < HEAL_RADIUS) {
			if(enhanced_ticks % 12 == 0) {
				player.health++;
				prev_health = player.health;
				hplant[i].pool--;
			}
			if((RAND(0,7)) == 0)
				add_particle(player.x + RANDF(-10, 10), player.y + RANDF(-6, 8), RANDF(-0.2, 0.2), RANDF(-0.9, -0.3), 110, 240, 120, (RAND(18,36)));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Burrowers: hide in the sand of the arms (never the centre screen), creep towards the player under the surface,
// rumble when close, erupt, then fight like any bug. They dive again when the player leaves the arm.

static int burrow_timer = 0;
static bool dust_stun = false;				// a stunning burrower's dust is blue-white
static int player_stun = 0;					// ticks the player cannot walk
static long stun_immune_until = 0;			// no second stun straight after one
#define STUN_TICKS		((int)tune.stun_ticks)
#define STUN_IMMUNE		((int)(60 * tune.stun_immune_seconds))
static bool force_stunner = false;			// test hook
static bool test_stunnow = false;			// test hook
static int rumble_messages = 0;
static SDL_Surface *crater_spr = NULL;

static void make_crater_sprite() {
	if(crater_spr)
		return;
	const int S = 56;
	crater_spr = SDL_CreateRGBSurface(SDL_SWSURFACE, S, S, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
	for(int y=0; y<S; y++) {
		for(int x=0; x<S; x++) {
			float dx = (x - S / 2.0f + 0.5f) / (S / 2.0f), dy = (y - S / 2.0f + 0.5f) / (S / 2.0f) * 1.2f;
			float d = sqrtf(dx * dx + dy * dy);
			float a = d >= 1.0f ? 0.0f : (d < 0.55f ? 0.62f : 0.62f * (1.0f - d) / 0.45f);
			Uint8 *p = (Uint8 *)crater_spr->pixels + y * crater_spr->pitch + x * 4;
			p[0] = 20; p[1] = 12; p[2] = 6;
			p[3] = (Uint8)(a * 255);
		}
	}
}

static int cell_of_critter(CRITTER *c) {
	return cell_index(gx + fdiv((int)c->x, CELL_W), gy + fdiv((int)c->y, CELL_H));
}

static void dust_puff(float x, float y, int n, float spread) {
	for(int k=0; k<n; k++)
		if(dust_stun)
			add_particle(x + RANDF(-6, 6), y + RANDF(-4, 4), RANDF(-spread, spread), RANDF(-spread, spread * 0.3f), (RAND(150,200)), (RAND(190,230)), (RAND(240,255)), (RAND(10,26)));
		else
			add_particle(x + RANDF(-6, 6), y + RANDF(-4, 4), RANDF(-spread, spread), RANDF(-spread, spread * 0.3f), (RAND(150,190)), (RAND(105,135)), (RAND(60,80)), (RAND(10,26)));
}

static bool burrower_update_impl(CRITTER *c);
bool enhanced_burrower_update(CRITTER *c) {
	dust_stun = (c->type == CT_STUNNER);
	bool r = burrower_update_impl(c);
	dust_stun = false;
	return r;
}

bool enhanced_player_held() {
	return enhanced_in_bunker() || player_stun > 0;
}

static bool burrower_update_impl(CRITTER *c) {
	bool st = (c->type == CT_STUNNER);
	int cc = cell_of_critter(c);
	int pc = cell_index(gx + fdiv((int)player.x, CELL_W), gy + fdiv((int)player.y, CELL_H));
	bool with_player = player.ok && cc > 0 && cc == pc;

	switch(c->bstate) {
		case 0: {		// hiding, and slowly creeping towards the player
			c->hidden = true;
			if(!with_player)
				return true;
			float d = distance(c->x, c->y, player.x, player.y);
			if(d < 135 && c->btimer <= 0) {
				c->bstate = 1;
				c->btimer = 50;
				if(rumble_messages < 3) {
					rumble_messages++;
					set_message(st ? "the ground hums" : "the ground trembles");
				}
				return true;
			}
			if(c->btimer > 0)
				c->btimer--;
			// creep: about a third of the player's speed, only through free sand
			float a = compute_angle(player.x, player.y, c->x, c->y);
			float mx, my;
			move_xy(1.0f, a, &mx, &my);
			if(!world_blocked((int)(c->x + mx * 4), (int)(c->y + my * 4), false)) {
				c->x += mx;
				c->y += my;
			}
			// the faint trail of a thing moving under the surface
			if(enhanced_ticks % 22 == 0 && d < 330)
				dust_puff(c->x, c->y, 2, 0.3f);
			return true;
		}
		case 1: {		// rumbling: dust builds, it tracks the player quickly, then breaks out
			c->hidden = true;
			if(!with_player) {
				c->bstate = 0;
				c->btimer = 60 * (RAND(5, 9));
				return true;
			}
			float a = compute_angle(player.x, player.y, c->x, c->y);
			float mx, my;
			move_xy(2.2f, a, &mx, &my);
			if(!world_blocked((int)(c->x + mx * 3), (int)(c->y + my * 3), false)) {
				c->x += mx;
				c->y += my;
			}
			dust_puff(c->x, c->y, 1, 0.9f);
			if(--c->btimer <= 0) {
				// eruption
				c->hidden = false;
				c->bstate = 2;
				c->btimer = 0;
				dust_puff(c->x, c->y, 22, 3.0f);
				make_crater_sprite();
				SDL_Rect r;
				r.x = (int)c->x - crater_spr->w / 2;
				r.y = (int)c->y - crater_spr->h / 2;
				r.w = crater_spr->w;
				r.h = crater_spr->h;
				SDL_BlitSurface(crater_spr, NULL, cur_bground, &r);
				if(player.ok && distance(c->x, c->y, player.x, player.y) < 56 && !enhanced_bunker_absorb((int)(st ? tune.stunner_damage : tune.burrower_damage))) {
					player.health -= (int)(st ? tune.stunner_damage : tune.burrower_damage);
					if(player.health <= 0)
						player.die();
					if(st) {
						// the shock: no walking for a moment (aiming and firing still work), then a few seconds of immunity
						if(enhanced_ticks >= stun_immune_until) {
							player_stun = STUN_TICKS;
							stun_immune_until = enhanced_ticks + STUN_TICKS + STUN_IMMUNE;
							player.dirx = player.diry = 0;
							set_message("stunned!");
						}
					}
					else {
						float kx, ky;
						move_xy(7.0f, compute_angle(player.x, player.y, c->x, c->y), &kx, &ky);		// away from the burst
						player.dirx += kx;
						player.diry += ky;
					}
				}
				if(config.sound)
					Mix_PlayChannel(-1, snd_explo[1], 0);
			}
			return true;
		}
		default: {		// up and fighting; goes back under when the player leaves the arm
			if(!with_player || distance(c->x, c->y, player.x, player.y) > 560) {
				c->bstate = 0;
				c->hidden = true;
				c->btimer = 60 * (RAND(5, 9));
				dust_puff(c->x, c->y, 8, 1.5f);
				c->health = c->maxhp;
				return true;
			}
			return false;
		}
	}
}

static int burrowers_in_cell(int ci) {
	int n = 0;
	for(CRITTER *c = critter_first; c; c = c->next)
		if((c->type == CT_BURROWER || c->type == CT_STUNNER) && !c->remove && cell_of_critter(c) == ci)
			n++;
	return n;
}

static void spawn_burrower(int ci) {
	for(int tries=0; tries<60; tries++) {
		int lx = (RAND(80, CELL_W - 80)), ly = (RAND(80, CELL_H - 80));
		float x = (float)(lx + (CELL_GX[ci] - gx) * CELL_W), y = (float)(ly + (CELL_GY[ci] - gy) * CELL_H);
		float d = distance(x, y, player.x, player.y);
		if((d < 220 || d > 380) && !test_stunnow)
			continue;
		if(world_blocked((int)x, (int)y, false) || world_blocked((int)x + 14, (int)y, false) || world_blocked((int)x - 14, (int)y, false) ||
		   world_blocked((int)x, (int)y + 14, false) || world_blocked((int)x, (int)y - 14, false))
			continue;
		// after at least one Hive Tyrant has been slain some of them stun: 30 % of the burrowers after the first, 15 % more
		// for each further one (at most 80 %)
		float chance = stat_tyrants > 0 ? tune.stunner_chance_first + tune.stunner_chance_step * (stat_tyrants - 1) : 0.0f;
		if(chance > 0.80f)
			chance = 0.80f;
		bool stun_variant = force_stunner || RANDF(0, 1) < chance;
		if(test_stunnow) {		// test builds: already rumbling beside the player, erupting in half a second
			x = player.x + 30;
			y = player.y;
		}
		CRITTER *c = add_critter(x, y, 0);
		critter_set_type(c, stun_variant ? CT_STUNNER : CT_BURROWER);
		if(test_stunnow) {
			c->bstate = 1;
			c->btimer = 30;
		}
		c->hidden = true;
		c->bstate = 0;
		c->btimer = 60 * (RAND(2, 5));
		return;
	}
}

static void burrow_update(int here) {
	if(here <= 0 || !player.ok || enhanced_ticks < (long)(60 * tune.burrower_first_seconds))		// never in the centre; nothing at the start
		return;
	if(--burrow_timer > 0)
		return;
	burrow_timer = (int)(60 * RANDF(tune.burrower_min_seconds, tune.burrower_max_seconds));	// few and far between
	int allowed = (enhanced_ticks > 60 * 480) ? 2 : 1;
	if(burrowers_in_cell(here) < allowed)
		spawn_burrower(here);
}


///////////////////////////////////////////////////////////////////////////////
// Bunker: a rare kit, dropped by guardians in the arms (never more than one kit or bunker at a time). R1 deploys it
// a little ahead of the player; stepping onto it puts the player inside: no walking, but aiming and firing out, a
// light regeneration, and the bugs wear the bunker down instead of the player. It lasts until its integrity is gone
// (it also fatigues slowly, and an empty bunker crumbles after a while); then it goes off like a small mine, which
// clears the bugs crowding it, and the player is free.

static void blit_center(SDL_Surface *bmp, SDL_Surface *sp, int x, int y);
static void rect(SDL_Surface *bmp, int x, int y, int w, int h, int r, int g, int b);
#define NUM_TRAPS	3
static void traps_draw_under(SDL_Surface *bmp);
static void tyrant_draw_under(SDL_Surface *bmp);
static void flame_draw_under(SDL_Surface *bmp);
void spawn_heal_plant_at(float x, float y);

#define BUNKER_INTEGRITY	(tune.bunker_integrity)
#define BUNKER_ABSORB		(tune.bunker_absorb)
#define BUNKER_FATIGUE		0.4f			// integrity lost per second regardless
#define BUNKER_ENTER		22
#define BUNKER_IDLE_TICKS	(60 * 90)
#define BLAST_RADIUS		125.0f
#define BLAST_DAMAGE		(tune.bunker_blast_damage)

struct BUNKER {
	bool ok;
	float x, y;
	float integrity;
	bool inside;
	int idle;
	bool relock;			// the player stepped out: it takes them leaving the doorway before walking in re-enters
};
struct KIT {
	bool ok;
	float x, y;
	int life;
};
static BUNKER bunker;
static KIT kit;
static bool kit_carried = false;
static bool r_prev = false;
static int bunker_regen = 0;
static SDL_Surface *kit_spr = NULL, *bunker_floor = NULL, *bunker_ring[3] = { NULL, NULL, NULL };

bool enhanced_in_bunker() {
	return bunker.ok && bunker.inside;
}

static bool bunker_anything() {
	return kit.ok || kit_carried || bunker.ok;
}

static SDL_Surface *make_kit_sprite() {
	// the bunker kit: a little dome with a gun slit, olive with a white rim (nothing else is dome shaped)
	const int W_ = 34, H_ = 26;
	SDL_Surface *sp = SDL_CreateRGBSurface(SDL_SWSURFACE, W_, H_, 16, 0xf800, 0x07e0, 0x001f, 0);
	Uint32 key = SDL_MapRGB(sp->format, 255, 0, 255);
	SDL_FillRect(sp, NULL, key);
	for(int y=0; y<H_; y++) {
		for(int x=0; x<W_; x++) {
			float dx = x - W_ / 2.0f + 0.5f, dy = y - (H_ - 5.0f);
			int r = -1, g = 0, b = 0;
			float d = sqrtf(dx * dx + dy * dy * 1.25f);
			if(dy <= 0 && d < 16.0f) {
				float lit = 0.75f + 0.5f * (-dx * 0.4f - dy * 0.7f) / 16.0f;
				r = (int)(88 * lit); g = (int)(112 * lit); b = (int)(70 * lit);
				if(d > 14.0f) { r = 235; g = 235; b = 225; }					// white rim
				if(dy > -9.0f && dy < -6.0f && fabsf(dx) < 6.0f) { r = 18; g = 22; b = 16; }	// the slit
			}
			if(dy > 0 && dy < 4.0f && fabsf(dx) < 16.5f) { r = 235; g = 235; b = 225; }		// base line
			if(r >= 0)
				*(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format, r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
		}
	}
	SDL_SetColorKey(sp, SDL_SRCCOLORKEY, key);
	return sp;
}

static void make_bunker_sprites() {
	if(bunker_floor)
		return;
	kit_spr = make_kit_sprite();
	// the floor: a round slab under the player
	{
		const int S = 60;
		bunker_floor = SDL_CreateRGBSurface(SDL_SWSURFACE, S, S, 16, 0xf800, 0x07e0, 0x001f, 0);
		Uint32 key = SDL_MapRGB(bunker_floor->format, 255, 0, 255);
		SDL_FillRect(bunker_floor, NULL, key);
		for(int y=0; y<S; y++) for(int x=0; x<S; x++) {
			float dx = x - S / 2.0f + 0.5f, dy = y - S / 2.0f + 0.5f, d = sqrtf(dx * dx + dy * dy);
			if(d > 29.5f) continue;
			float n = 0.9f + 0.1f * sinf(x * 0.9f) * cosf(y * 1.3f);
			int v = (int)(96 * n);
			*(Uint16 *)((Uint8 *)bunker_floor->pixels + y * bunker_floor->pitch + x * 2) = (Uint16)SDL_MapRGB(bunker_floor->format, v, v - 4, v - 10);
		}
		SDL_SetColorKey(bunker_floor, SDL_SRCCOLORKEY, key);
	}
	// the ring of sandbags and concrete, in three states of repair; the inside stays see-through
	for(int st=0; st<3; st++) {
		const int S = 68;
		SDL_Surface *r = SDL_CreateRGBSurface(SDL_SWSURFACE, S, S, 16, 0xf800, 0x07e0, 0x001f, 0);
		Uint32 key = SDL_MapRGB(r->format, 255, 0, 255);
		SDL_FillRect(r, NULL, key);
		for(int y=0; y<S; y++) for(int x=0; x<S; x++) {
			float dx = x - S / 2.0f + 0.5f, dy = y - S / 2.0f + 0.5f, d = sqrtf(dx * dx + dy * dy);
			if(d > 33.0f || d < 22.0f) continue;
			float ang = atan2f(dy, dx);
			int seg = (int)((ang + 3.14159265f) / (6.2831853f / 14));
			float segshade = 0.82f + 0.18f * ((seg & 1) ? 1.0f : 0.0f);
			float edge = (d > 31.0f || d < 24.0f) ? 0.7f : 1.0f;			// darker lip
			float lit = 0.8f + 0.3f * (-dx * 0.5f - dy * 0.6f) / 33.0f;
			float base = 118.0f * segshade * edge * lit;
			float rr = base, gg = base * 0.97f, bb = base * 0.86f;
			if(st >= 1) { rr *= 1.00f; gg *= 0.86f; bb *= 0.72f; }			// scorched
			if(st == 2) { rr *= 0.92f; gg *= 0.78f; bb *= 0.7f; }
			// cracks
			int hash = (x * 7 + y * 13 + seg * 5) % 17;
			if(st >= 1 && hash == 0) { rr *= 0.35f; gg *= 0.35f; bb *= 0.35f; }
			if(st == 2 && (hash == 3 || hash == 7)) { rr *= 0.3f; gg *= 0.3f; bb *= 0.3f; }
			*(Uint16 *)((Uint8 *)r->pixels + y * r->pitch + x * 2) = (Uint16)SDL_MapRGB(r->format,
				(int)fminf(rr, 255), (int)fminf(gg, 255), (int)fminf(bb, 255));
		}
		SDL_SetColorKey(r, SDL_SRCCOLORKEY, key);
		bunker_ring[st] = r;
	}
}

// the bunker goes off like a small mine: bugs near it are hurt (the player, in the middle, is not)
static void bunker_blast(float x, float y) {
	create_explosion((int)x, (int)y, (RAND(0,1)), 0.5f);
	if(config.sound)
		Mix_PlayChannel(-1, snd_explo[(RAND(0,1))], 0);
	make_crater_sprite();
	SDL_Rect r;
	r.x = (int)x - crater_spr->w / 2;
	r.y = (int)y - crater_spr->h / 2;
	r.w = crater_spr->w;
	r.h = crater_spr->h;
	SDL_BlitSurface(crater_spr, NULL, cur_bground, &r);
	for(int k=0; k<24; k++)
		add_particle(x, y, RANDF(-3.2, 3.2), RANDF(-3.2, 3.2), RAND(200,255), RAND(120,200), RAND(40,90), RAND(12,30));
	CRITTER *c = critter_first;
	while(c) {
		CRITTER *next = c->next;
		if(!c->remove && !c->hidden) {
			float d = distance(c->x, c->y, x, y);
			if(d <= BLAST_RADIUS) {
				float f = 1.0f - d / BLAST_RADIUS;
				int dam = (int)(BLAST_DAMAGE * f);
				float ang = compute_angle(c->x, c->y, x, y);
				if(dam > 0) {
					c->health -= enhanced_damage(c, dam);
					if(c->health <= 0)
						c->die(ang);
				}
				float px, py;
				move_xy(RANDF(6, 8) * f / ctype_stats[c->type].mass, ang, &px, &py);
				c->dirx += px;
				c->diry += py;
			}
		}
		c = next;
	}
}

bool enhanced_bunker_target(float px, float py, float *deg) {
	CRITTER *best = NULL;
	float bd = 1e9f;
	for(CRITTER *c = critter_first; c; c = c->next) {
		if(c->hidden || c->health <= 0)
			continue;
		float d = distance(px, py, c->x, c->y);
		if(d < bd) {
			bd = d;
			best = c;
		}
	}
	if(!best || bd > 260)
		return false;
	*deg = (float)(atan2((double)(best->x - px), (double)(py - best->y)) * 180.0 / 3.14159265358979);
	return true;
}

bool enhanced_bunker_absorb(int damage) {
	if(!enhanced_in_bunker())
		return false;
	bunker.integrity -= damage * BUNKER_ABSORB;
	if(RAND(0,5) == 0)
		add_particle(bunker.x + RANDF(-24, 24), bunker.y + RANDF(-24, 24), RANDF(-1.2, 1.2), RANDF(-1.2, 0.4), 255, RAND(180,230), 80, RAND(5,10));
	return true;
}

static void spawn_trap_box(float x, float y, int type);

void enhanced_guardian_killed(float x, float y, int type) {
	if(RANDF(0, 1) < 0.10f) {				// now and then a trap crate falls out of a guardian as well
		float tx = x, ty = y;
		enhanced_unstick(&tx, &ty);
		spawn_trap_box(tx, ty, (RAND(0, NUM_TRAPS - 1)));
	}
	if(bunker_anything())
		return;
	float chance = (type == CT_BRUTE) ? tune.warden_drop_chance : tune.bunker_drop_chance;	// rare, but it should turn up: the warden is the best bet
	if(RANDF(0, 1) >= chance)
		return;
	float kx = x, ky = y;
	enhanced_unstick(&kx, &ky);
	kit.ok = true;
	kit.x = kx;
	kit.y = ky;
	kit.life = 60 * 120;
	set_message("the guardian dropped a bunker kit");
}

static void bunker_deploy() {
	float dx, dy;
	move_xy(1.0f, add_angle(player.angle, -60), &dx, &dy);		// the way the player aims
	static const int DIST[5] = { 52, 44, 36, 28, 0 };
	for(int i=0; i<5; i++) {
		float x = player.x + dx * DIST[i], y = player.y + dy * DIST[i];
		if(world_blocked((int)x, (int)y, false) || world_blocked((int)x + 20, (int)y, false) || world_blocked((int)x - 20, (int)y, false) ||
		   world_blocked((int)x, (int)y + 20, false) || world_blocked((int)x, (int)y - 20, false))
			continue;
		bunker.ok = true;
		bunker.x = x;
		bunker.y = y;
		bunker.integrity = BUNKER_INTEGRITY;
#ifdef FORMIDO_TESTHOOKS
		if(getenv("FORMIDO_BUNKERHP"))
			bunker.integrity = (float)atoi(getenv("FORMIDO_BUNKERHP"));
#endif
		bunker.inside = false;
		bunker.relock = false;
		bunker.idle = 0;
		kit_carried = false;
		set_message("bunker ready: step inside");
		if(config.sound)
			Mix_PlayChannel(-1, snd_power_up, 0);
		return;
	}
	set_message("no room to deploy the bunker here");
}

// R1 inside the bunker: step out (to collect pick-ups, say) and walk back in later. The bunker stays as it is and holds
// out until its integrity is gone.
static void bunker_step_out() {
	float dx, dy;
	move_xy(1.0f, add_angle(player.angle, -60), &dx, &dy);			// the way the player is aiming first, then round the compass
	for(int k=0; k<8; k++) {
		float a = k * 45.0f;
		float ox = dx, oy = dy;
		if(k) {
			float ca = cosf(a * 3.14159265f / 180.0f), sa = sinf(a * 3.14159265f / 180.0f);
			ox = dx * ca - dy * sa;
			oy = dx * sa + dy * ca;
		}
		float x = bunker.x + ox * 36, y = bunker.y + oy * 36;
		if(world_blocked((int)x, (int)y, false) || world_blocked((int)x + 10, (int)y, false) || world_blocked((int)x - 10, (int)y, false) ||
		   world_blocked((int)x, (int)y + 10, false) || world_blocked((int)x, (int)y - 10, false))
			continue;
		player.x = player.sx = x;
		player.y = player.sy = y;
		player.dirx = player.diry = 0;
		bunker.inside = false;
		bunker.relock = true;
		set_message("out of the bunker: walk back in to use it");
		return;
	}
	set_message("no room to step out here");
}

static void bunker_update() {
	bool r = key[SDLK_r] != 0;
	if(r && !r_prev && kit_carried && !bunker.ok && player.ok)
		bunker_deploy();
	else if(r && !r_prev && bunker.ok && bunker.inside && player.ok)
		bunker_step_out();
	r_prev = r;

	if(kit.ok) {
		if(--kit.life <= 0)
			kit.ok = false;
		else if(player.ok && !kit_carried && !bunker.ok && distance(player.x, player.y, kit.x, kit.y) < 26) {
			kit_carried = true;
			kit.ok = false;
			set_message("bunker kit: press R1 to deploy it");
			if(config.sound)
				Mix_PlayChannel(-1, snd_power_up, 0);
		}
	}

	if(!bunker.ok)
		return;

	bool expired = false;
	if(!player.ok)
		bunker.inside = false;
	if(!bunker.inside) {
		float d = player.ok ? distance(player.x, player.y, bunker.x, bunker.y) : 1e9f;
		if(bunker.relock && d >= BUNKER_ENTER + 8)
			bunker.relock = false;
		if(!bunker.relock && d < BUNKER_ENTER) {
			bunker.inside = true;
			bunker_regen = 0;
			set_message("inside the bunker: fire away, R1 steps out");
		}
		// (an empty bunker stands until its integrity is gone: it only fatigues, below)
	}
	if(bunker.inside) {
		player.x = bunker.x;
		player.y = bunker.y;
		player.sx = player.x;
		player.sy = player.y;
		player.dirx = player.diry = 0;
		if(++bunker_regen >= 20) {					// a light regeneration
			bunker_regen = 0;
			if(player.health < 200) {
				player.health++;
				prev_health = player.health;
			}
		}
	}
	bunker.integrity -= BUNKER_FATIGUE / 60.0f;
	if(bunker.integrity <= 0)
		expired = true;
	if(expired) {
		bunker_blast(bunker.x, bunker.y);
		if(bunker.inside)
			set_message("the bunker is breached: it goes up");
		bunker.ok = false;
		bunker.inside = false;
	}
}

void enhanced_draw_under(SDL_Surface *bmp) {
	traps_draw_under(bmp);
	flame_draw_under(bmp);
	tyrant_draw_under(bmp);
	if(!bunker_floor)
		return;
	if(kit.ok && (kit.life > 240 || (kit.life / 8) % 2 == 0)) {
		blit_center(bmp, glow_item[8], (int)kit.x, (int)kit.y);
		blit_center(bmp, kit_spr, (int)kit.x, (int)kit.y);
	}
	if(bunker.ok)
		blit_center(bmp, bunker_floor, (int)bunker.x, (int)bunker.y);
}

static void bunker_draw_over(SDL_Surface *bmp) {
	if(!bunker_floor)
		return;
	if(bunker.ok) {
		float f = bunker.integrity / BUNKER_INTEGRITY;
		blit_center(bmp, bunker_ring[f > 0.6f ? 0 : (f > 0.3f ? 1 : 2)], (int)bunker.x, (int)bunker.y);
		// its integrity, over the ring
		int w = 46, filled = (int)(w * (f < 0 ? 0 : f));
		rect(bmp, (int)bunker.x - w / 2 - 1, (int)bunker.y - 42, w + 2, 6, 14, 14, 12);
		rect(bmp, (int)bunker.x - w / 2, (int)bunker.y - 41, w, 4, 60, 40, 30);
		rect(bmp, (int)bunker.x - w / 2, (int)bunker.y - 41, filled, 4, f > 0.5f ? 120 : 230, f > 0.5f ? 220 : 150, 70);
	}
	if(kit_carried)
		blit_center(bmp, kit_spr, (int)player.x + 22, (int)player.y - 34);
}

///////////////////////////////////////////////////////////////////////////////
// Burning (napalm) and bleeding (spikes). Burning: 2 hit points every 10 ticks while it lasts, which is 3 seconds after
// the bug (or the player) was last in the fire. Bleeding: 1 hit point every 20 ticks for 5 seconds after the last
// spear, and everything else hurts 25 % more in the meantime.

#define BURN_TICKS		180
#define BLEED_TICKS		300
#define BLEED_VULN		(tune.bleed_vulnerability)

static int player_burn = 0, player_bleed = 0, player_slow = 0;

static int scaled_int(int dmg, float mult) {
	return (int)(dmg * mult + RANDF(0, 1));			// whole points on average
}

int enhanced_damage(CRITTER *c, int dmg) {
	float m = 1.0f;
	if(c->bleed > 0)
		m *= BLEED_VULN;
	if(c->stun > 0)
		m *= 1.5f;						// the Tyrant, dazed after a charge
	if(m > 1.0f)
		return scaled_int(dmg, m);
	return dmg;
}

int enhanced_player_bite(int bite) {
	float m = tune.bug_damage_mult;
	if(player_bleed > 0)
		m *= BLEED_VULN;
	if(m != 1.0f)
		return scaled_int(bite, m);
	return bite;
}

void enhanced_status_tick(CRITTER *c) {
	if(c->hidden || c->remove)
		return;
	if(c->slow > 0)
		c->slow--;
	if(c->burn > 0) {
		c->burn--;
		if(c->burn % 10 == 0) {
			c->health -= 2;
			if(c->health <= 0) {
				c->die(RANDF(0, 359));
				return;
			}
		}
		if(c->burn % 6 == 0)
			add_particle(c->x + RANDF(-8, 8), c->y + RANDF(-8, 4), RANDF(-0.3, 0.3), RANDF(-1.5, -0.6), 255, (RAND(90,170)), 20, (RAND(8,14)));
	}
	if(c->bleed > 0) {
		c->bleed--;
		if(c->bleed % 20 == 0) {
			c->health -= 1;
			if(c->health <= 0) {
				c->die(RANDF(0, 359));
				return;
			}
			add_particle(c->x + RANDF(-8, 8), c->y + RANDF(-4, 8), RANDF(-0.3, 0.3), RANDF(0.1, 0.8), 190, 30, 30, (RAND(8,14)));
		}
	}
}

static void player_status_tick() {
	if(!player.ok || enhanced_in_bunker()) {
		player_burn = player_bleed = player_slow = 0;
		return;
	}
	if(player_slow > 0)
		player_slow--;
	if(player_burn > 0) {
		player_burn--;
		if(player_burn % 10 == 0 && player.health > 2)
			player.health -= 2;
		if(player_burn % 6 == 0)
			add_particle(player.x + RANDF(-8, 8), player.y + RANDF(-8, 4), RANDF(-0.3, 0.3), RANDF(-1.5, -0.6), 255, (RAND(90,170)), 20, (RAND(8,14)));
	}
	if(player_bleed > 0) {
		player_bleed--;
		if(player_bleed % 20 == 0 && player.health > 2) {
			player.health -= 1;
			add_particle(player.x + RANDF(-8, 8), player.y + RANDF(-4, 8), RANDF(-0.3, 0.3), RANDF(0.1, 0.8), 190, 30, 30, (RAND(8,14)));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Tracer rounds: every third shot of the main gun (the flamethrower has no tracers) is drawn as a short bright streak
// that flies from the muzzle to where the shot ended, a few frames long. The shot itself still lands at once.

#define MAX_TRACERS	10
struct TRACER {
	bool ok;
	float x, y, dx, dy;		// start and unit direction
	float len, pos;			// length of the path, and where the head of the streak is
};
static TRACER tracers[MAX_TRACERS];
static int tracer_shots = 0;

void enhanced_tracer(float x0, float y0, float x1, float y1) {
	if(++tracer_shots < 3)
		return;
	tracer_shots = 0;
	float dx = x1 - x0, dy = y1 - y0;
	float len = sqrtf(dx * dx + dy * dy);
	if(len < 24.0f)
		return;
	for(int i=0; i<MAX_TRACERS; i++) {
		if(!tracers[i].ok) {
			tracers[i].ok = true;
			tracers[i].x = x0;
			tracers[i].y = y0;
			tracers[i].dx = dx / len;
			tracers[i].dy = dy / len;
			tracers[i].len = len;
			tracers[i].pos = 8.0f;
			return;
		}
	}
}

static void tracers_update() {
	for(int i=0; i<MAX_TRACERS; i++) {
		if(!tracers[i].ok)
			continue;
		tracers[i].pos += 40.0f;							// about 2400 pixels a second
		if(tracers[i].pos - 56.0f > tracers[i].len)
			tracers[i].ok = false;
	}
}

static void tracers_draw(SDL_Surface *bmp) {
	for(int i=0; i<MAX_TRACERS; i++) {
		if(!tracers[i].ok)
			continue;
		const TRACER &t = tracers[i];
		float head = t.pos < t.len ? t.pos : t.len;
		float tail = t.pos - 56.0f;
		if(tail < 0)
			tail = 0;
		for(float d=tail; d<=head; d+=2.0f) {
			float k = (d - (t.pos - 56.0f)) / 56.0f;			// 0 at the tail, 1 at the head
			if(k < 0)
				k = 0;
			int r = 255, g = (int)(120 + 130 * k), b = (int)(30 + 130 * k * k);
			int s = k > 0.7f ? 3 : 2;
			rect(bmp, (int)(t.x + t.dx * d) - s / 2, (int)(t.y + t.dy * d) - s / 2, s, s, r, g, b);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// The flamethrower. After a number of kills a canister drops where the last bug fell. Picking it up swaps the gun for a
// flamethrower: firing sprays a cone of flame that does a great deal of damage to everything in it and sets it
// burning, but it reaches only a short way. It runs out of fuel (seconds of firing) or spoils (seconds carried), and
// the usual gun is back.

struct FLAMEPU {
	bool ok;
	float x, y;
	int life;
};
static FLAMEPU flame_pu;
static int flame_fuel = 0;				// ticks of firing left; 0 = no flamethrower
static long flame_hold_until = 0;
static int flame_kill_count = 0;
static int flame_show = 0;				// ticks the flames stay drawn after a burst
static float flame_dirx = 0, flame_diry = -1, flame_free = 100;
static SDL_Surface *flame_spr = NULL, *flame_glow = NULL;

#ifdef FORMIDO_TESTHOOKS
static int flame_hits = 0, flame_killed = 0;
#endif
static bool flame_anything() {
	return flame_pu.ok || flame_fuel > 0;
}

bool enhanced_flame_active() {
	return flame_fuel > 0 && enhanced_ticks < flame_hold_until;
}

static void flame_reset() {
	flame_pu.ok = false;
	flame_fuel = 0;
	flame_hold_until = 0;
	flame_kill_count = 0;
	flame_show = 0;
}

static void flame_make_sprites() {
	if(flame_spr)
		return;
	const int W_ = 26, H_ = 32;
	SDL_Surface *sp = SDL_CreateRGBSurface(SDL_SWSURFACE, W_, H_, 16, 0xf800, 0x07e0, 0x001f, 0);
	Uint32 key = SDL_MapRGB(sp->format, 255, 0, 255);
	SDL_FillRect(sp, NULL, key);
	for(int y=0; y<H_; y++) {
		for(int x=0; x<W_; x++) {
			int r = -1, g = 0, b = 0;
			float dx = x - W_ / 2.0f + 0.5f;
			if(y >= 10 && y < 30 && fabsf(dx) < 9.5f) {			// the tank: red with a bright edge on the left
				float lit = 0.70f + 0.45f * (-dx / 9.5f) * 0.6f + 0.25f;
				r = (int)(212 * lit); g = (int)(48 * lit); b = (int)(34 * lit);
				if(y < 13 || y >= 27) { r = (int)(r * 0.62f); g = (int)(g * 0.62f); b = (int)(b * 0.62f); }	// the dark bands
				if(fabsf(dx) > 8.2f) { r = 250; g = 232; b = 220; }												// pale rim
			}
			if(y >= 7 && y < 10 && fabsf(dx) < 4.5f) { r = 90; g = 90; b = 96; }									// valve
			if(y >= 2 && y < 7 && dx > -1.5f && dx < 8.5f) { r = 120; g = 120; b = 126; }							// nozzle going right
			// the flame glyph on the tank
			float fx = dx, fy = y - 21.0f;
			if(y >= 15 && y < 27) {
				float wdt = (fy < 0 ? 5.0f + fy * 0.28f : 5.0f - fy * 0.55f);
				if(fabsf(fx) < wdt * 0.55f) { r = 255; g = 170 + (int)(fy * 6); b = 40; }
				if(fabsf(fx) < wdt * 0.25f && fy > -2) { r = 255; g = 240; b = 150; }
			}
			if(r >= 0)
				*(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format, r > 255 ? 255 : r, g > 255 ? 255 : (g < 0 ? 0 : g), b > 255 ? 255 : b);
		}
	}
	SDL_SetColorKey(sp, SDL_SRCCOLORKEY, key);
	flame_spr = sp;
	flame_glow = make_glow_rgb(255, 120, 30, 56, 120.0f);
}

static void flame_drop(float x, float y) {
	// where the bug fell if it is on the screen, otherwise a little ahead of the player
	if(x < 30 || y < 30 || x > SCREEN_W - 30 || y > SCREEN_H - 30) {
		float dx, dy;
		move_xy(60.0f, add_angle(player.angle, -60), &dx, &dy);
		x = player.x + dx;
		y = player.y + dy;
	}
	enhanced_unstick(&x, &y);
	flame_pu.ok = true;
	flame_pu.x = x;
	flame_pu.y = y;
	flame_pu.life = (int)(tune.flame_ground_seconds * 60);
	set_message("a flamethrower canister dropped");
#ifdef FORMIDO_TESTHOOKS
	fprintf(stderr, "flame: canister dropped at tick %ld (%d,%d)\n", enhanced_ticks, (int)x, (int)y);
#endif
}

static void flame_update() {
	if(flame_show > 0)
		flame_show--;
	if(flame_fuel > 0 && enhanced_ticks >= flame_hold_until) {
		flame_fuel = 0;
		set_message("the flamethrower's fuel has spoiled");
	}
	if(flame_pu.ok) {
		if(--flame_pu.life <= 0)
			flame_pu.ok = false;
		else if(player.ok && distance(player.x, player.y, flame_pu.x, flame_pu.y) < 26) {
			flame_pu.ok = false;
			flame_fuel = (int)(tune.flame_fuel_seconds * 60);
			flame_hold_until = enhanced_ticks + (long)(tune.flame_hold_seconds * 60);
			flame_kill_count = 0;
			set_message("flamethrower: short range, fuel is limited");
			if(config.sound)
				Mix_PlayChannel(-1, snd_power_up, 0);
		}
	}
}

void enhanced_flame_fire() {
	float dx, dy;
	move_xy(1.0f, add_angle(player.angle, -60), &dx, &dy);
	flame_dirx = dx;
	flame_diry = dy;
	float ox = player.x + dx * 12, oy = player.y + dy * 12;
	float range = tune.flame_range;
	float tn = tanf(tune.flame_cone_degrees * 0.5f * 3.14159265f / 180.0f);

	// how far the flame gets down the middle before rock stops it
	flame_free = range;
	for(int t=12; t<=(int)range; t+=6) {
		if(world_blocked((int)(player.x + dx * t), (int)(player.y + dy * t), true)) {
			flame_free = (float)t;
			break;
		}
	}
	flame_show = 5;

	int used = (int)tune.flame_fire_delay_ticks;
	flame_fuel -= used;
	if(flame_fuel <= 0) {
		flame_fuel = 0;
		set_message("the flamethrower sputters out");
#ifdef FORMIDO_TESTHOOKS
		fprintf(stderr, "flame: out of fuel at tick %ld: %d bug hits, %d bugs killed by the flame\n", enhanced_ticks, flame_hits, flame_killed);
#endif
	}

	for(CRITTER *c = critter_first; c; c = c->next) {
		if(c->remove || c->hidden || c->health <= 0)
			continue;
		float vx = c->x - ox, vy = c->y - oy;
		float f = vx * dx + vy * dy;
		if(f < -8.0f || f > range + c->hs * 0.5f)
			continue;
		float l = fabsf(vx * dy - vy * dx);
		if(l > (f > 0 ? f : 0) * tn + c->hs * 0.9f)
			continue;
		// rock between the nozzle and the bug stops the flame
		bool blocked = false;
		float dist = sqrtf(vx * vx + vy * vy);
		for(float t=14; t<dist - 4 && !blocked; t+=10)
			if(world_blocked((int)(ox + vx / dist * t), (int)(oy + vy / dist * t), true))
				blocked = true;
		if(blocked)
			continue;
		int dmg = (int)tune.flame_damage;
		if(c->type == CT_TYRANT) {
			dmg = (int)(dmg * tune.flame_tyrant_share + 0.5f);
			if(dmg < 1)
				dmg = 1;
		}
		c->health -= enhanced_damage(c, dmg);
		c->burn = BURN_TICKS;
#ifdef FORMIDO_TESTHOOKS
		flame_hits++;
#endif
		if(RAND(0,3) == 0)
			c->blood(RAND(1,2), RANDF(0, 359));
		if(c->health <= 0) {
#ifdef FORMIDO_TESTHOOKS
			flame_killed++;
#endif
			c->die(RANDF(0, 359));
		}
	}
}

// a kill: after enough of them a canister drops (not while one lies about or is carried)
static void flame_count_kill(int type, float x, float y) {
	if(type == CT_PHANTOM || flame_anything())
		return;
	if(++flame_kill_count >= (int)tune.flame_every_kills) {
		flame_kill_count = 0;
		flame_drop(x, y);
	}
}

static void flame_draw_under(SDL_Surface *bmp) {
	if(flame_pu.ok && flame_spr && (flame_pu.life > 240 || (flame_pu.life / 8) % 2 == 0)) {
		blit_center(bmp, flame_glow, (int)flame_pu.x, (int)flame_pu.y);
		blit_center(bmp, flame_spr, (int)flame_pu.x, (int)flame_pu.y);
	}
}

static void flame_draw_over(SDL_Surface *bmp) {
	if(flame_show <= 0 || !player.ok)
		return;
	float range = tune.flame_range;
	float tn = tanf(tune.flame_cone_degrees * 0.5f * 3.14159265f / 180.0f);
	float ox = player.x + flame_dirx * 12, oy = player.y + flame_diry * 12;
	float px = -flame_diry, py = flame_dirx;			// across the cone
	for(int k=0; k<110; k++) {
		float u = RANDF(0, 1);
		u = u * u * 0.6f + u * 0.4f;						// more of it near the nozzle
		float t = 6 + u * (flame_free - 6);
		if(t < 6)
			continue;
		float lat = RANDF(-1, 1) * (t * tn * 0.95f + 3);
		int s = (int)(11.0f - 7.0f * (t / range)) + RAND(0, 3);
		float x = ox + flame_dirx * t + px * lat, y = oy + flame_diry * t + py * lat;
		float w = t / range;
		int r = 255, g, b;
		if(w < 0.3f)      { g = RAND(215, 250); b = RAND(110, 190); }
		else if(w < 0.65f) { g = RAND(130, 190); b = RAND(20, 50); }
		else              { g = RAND(50, 100); b = RAND(10, 30); r = RAND(200, 245); }
		rect(bmp, (int)x - s / 2, (int)y - s / 2, s, s, r, g, b);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Badges over burning, bleeding and slowed bugs (and the player)

static SDL_Surface *badge_spr[4] = { NULL, NULL, NULL, NULL };		// flame, drop, slowed, stunned

static bool badge_mask(int kind, int x, int y) {			// 0..9 each way
	float dx = x - 4.5f, dy = y - 4.5f;
	if(kind == 0) {			// flame: pointed at the top, round at the bottom
		float w = 4.2f * (y / 9.0f);
		return y <= 9 && fabsf(dx) < w && !(y > 6 && fabsf(dx) > 3.2f);
	}
	if(kind == 1) {			// drop
		float cy = y - 6.0f;
		return (dx * dx + cy * cy < 11.0f) || (y < 6 && fabsf(dx) < (y + 0.5f) * 0.55f);
	}
	if(kind == 3)			// stunned: a spark
		return (fabsf(dx) < 1.3f && fabsf(dy) < 4.6f) || (fabsf(dy) < 1.3f && fabsf(dx) < 4.6f) || (fabsf(dx) + fabsf(dy) < 3.2f);
	// slowed: a heavy down arrow
	if(y < 5 && fabsf(dx) < 1.8f)
		return true;
	return y >= 4 && y <= 8 && fabsf(dx) < (8.6f - y) * 1.1f;
}

static void make_badge_sprites() {
	if(badge_spr[0])
		return;
	static const int COL[4][3] = { {255,150,40}, {220,30,40}, {110,200,255}, {255,235,60} };
	for(int k=0; k<4; k++) {
		const int S = 12;
		SDL_Surface *sp = SDL_CreateRGBSurface(SDL_SWSURFACE, S, S, 16, 0xf800, 0x07e0, 0x001f, 0);
		Uint32 key = SDL_MapRGB(sp->format, 255, 0, 255);
		SDL_FillRect(sp, NULL, key);
		bool m[S + 2][S + 2];
		for(int y=0; y<S+2; y++) for(int x=0; x<S+2; x++) m[y][x] = false;
		for(int y=0; y<10; y++) for(int x=0; x<10; x++)
			m[y + 1][x + 1] = badge_mask(k, x, y);
		for(int y=0; y<S; y++) for(int x=0; x<S; x++) {
			bool on = m[y][x];
			bool edge = !on && ((x > 0 && m[y][x-1]) || (x < S + 1 && m[y][x+1]) || (y > 0 && m[y-1][x]) || (y < S + 1 && m[y+1][x]));
			if(on) {
				float k2 = 0.8f + 0.3f * (1.0f - (float)y / S);
				if(k == 0 && y > 6) { *(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format, 255, 235, 120); continue; }	// a bright core
				*(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format,
					(int)fminf(COL[k][0] * k2, 255), (int)fminf(COL[k][1] * k2, 255), (int)fminf(COL[k][2] * k2, 255));
			}
			else if(edge)
				*(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format, 20, 14, 12);
		}
		SDL_SetColorKey(sp, SDL_SRCCOLORKEY, key);
		badge_spr[k] = sp;
	}
}

void enhanced_draw_badges(SDL_Surface *bmp, int cx, int top, bool burning, bool bleeding, bool slowed, bool stunned) {
	if(!badge_spr[0])
		return;
	int n = (burning ? 1 : 0) + (bleeding ? 1 : 0) + (slowed ? 1 : 0) + (stunned ? 1 : 0);
	if(n == 0)
		return;
	int x = cx - n * 7 + 1;
	SDL_Rect r;
	for(int k=0; k<4; k++) {
		bool on = (k == 0) ? burning : (k == 1) ? bleeding : (k == 2) ? slowed : stunned;
		if(!on)
			continue;
		r.x = x; r.y = top; r.w = 12; r.h = 12;
		SDL_BlitSurface(badge_spr[k], NULL, bmp, &r);
		x += 14;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Traps: crates with a few charges of one kind turn up in the arms (and sometimes from guardians). R2 leaves a trap
// behind the player: napalm (a field of fire) or spikes (booby-trapped spears that also slow the bugs). A trap hurts
// the bugs standing on it a little at a time; it has an integrity of its own, which the bugs wear down, and it fades
// away with time. The player is not affected.

#define TRAP_NAPALM	0
#define TRAP_SPIKES	1
#define TRAP_GLUE	2
#define NUM_TRAPS	3
#define MAX_TRAPS	6
#define MAX_TBOXES	3

static const char *TRAP_NAME[NUM_TRAPS] = { "napalm", "spikes", "glue" };
static const int TRAP_COL[NUM_TRAPS][3] = { {255,140,40}, {196,196,176}, {230,215,70} };
static const int TRAP_CHARGES[NUM_TRAPS] = { 3, 4, 3 };
static const float TRAP_RADIUS[NUM_TRAPS] = { 54.0f, 38.0f, 46.0f };
static const int TRAP_LIFE[NUM_TRAPS] = { 60 * 16, 60 * 40, 60 * 30 };
static const float TRAP_INTEGRITY[NUM_TRAPS] = { 420.0f, 320.0f, 380.0f };
static const int TRAP_PERIOD[NUM_TRAPS] = { 5, 8, 6 };			// ticks between damage ticks
static const int TRAP_DAMAGE[NUM_TRAPS] = { 3, 3, 0 };				// per damage tick (napalm 36 hp/s, spikes 22 hp/s)

struct TRAP {
	bool ok;
	int type;
	float x, y;
	float integrity;
	int life;
};
struct TBOX {
	bool ok;
	float x, y;
	int type;
	int life;
};
static TRAP traps[MAX_TRAPS];
static TBOX tbox[MAX_TBOXES];
static int trap_loaded = -1;
static int trap_charges = 0;
static int trap_cool = 0;
static bool t_prev = false;
static int tbox_timer = 0;
static SDL_Surface *tbox_spr[NUM_TRAPS], *spike_spr[3], *glue_spr[3], *fire_glow = NULL;

static void make_trap_sprites() {
	if(fire_glow)
		return;
	// the pickups: a round trap disc in the trap's colour with its symbol in the middle (nothing else is round)
	for(int t=0; t<NUM_TRAPS; t++) {
		const int S = 30;
		SDL_Surface *sp = SDL_CreateRGBSurface(SDL_SWSURFACE, S, S, 16, 0xf800, 0x07e0, 0x001f, 0);
		Uint32 key = SDL_MapRGB(sp->format, 255, 0, 255);
		SDL_FillRect(sp, NULL, key);
		for(int y=0; y<S; y++) for(int x=0; x<S; x++) {
			float dx = x - S / 2.0f + 0.5f, dy = y - S / 2.0f + 0.5f, d = sqrtf(dx * dx + dy * dy);
			if(d > 14.5f) continue;
			int r, g, b;
			if(d > 11.5f) { r = TRAP_COL[t][0]; g = TRAP_COL[t][1]; b = TRAP_COL[t][2]; if(d > 13.5f) { r /= 2; g /= 2; b /= 2; } }
			else { float lit = 0.8f + 0.3f * (-dx * 0.4f - dy * 0.5f) / 11.0f; r = (int)(44 * lit); g = (int)(40 * lit); b = (int)(38 * lit); }
			if(d <= 11.5f) {
				if(t == TRAP_NAPALM) {			// a flame
					float w = 6.0f * ((dy + 8) / 16.0f);
					if(dy > -8 && dy < 8 && fabsf(dx) < (w > 0 ? w : 0) && !(dy > 3 && fabsf(dx) > 3.5f)) { r = 255; g = 130 + (int)((dy + 8) * 7); b = 30; }
				}
				else if(t == TRAP_SPIKES) {		// three spears
					for(int k=-1; k<=1; k++) {
						float sx = dx - k * 5.0f;
						if(fabsf(sx) < 1.0f && dy > -4 + abs(k) * 2 && dy < 8) { r = 215; g = 215; b = 200; }
						if(dy < -3 + abs(k) * 2 + 3 && dy > -7 + abs(k) * 2 && fabsf(sx) < (dy + 7 - abs(k) * 2) * 0.9f) { r = 240; g = 240; b = 230; }
					}
				}
				else {							// a drop of glue
					float cy = dy - 2.0f;
					bool body = (dx * dx + cy * cy < 30.0f) || (dy < 2.0f && dy > -8.0f && fabsf(dx) < (dy + 8.0f) * 0.55f);
					if(body) { r = 232; g = 218; b = 70; if(dx < -1 && dy < 3 && dy > -1) { r = 255; g = 250; b = 190; } }
				}
			}
			*(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format, r, g, b);
		}
		SDL_SetColorKey(sp, SDL_SRCCOLORKEY, key);
		tbox_spr[t] = sp;
	}
	// the spikes laid out on the ground, whole, bent and broken
	for(int st=0; st<3; st++) {
		const int S = 84;
		SDL_Surface *sp = SDL_CreateRGBSurface(SDL_SWSURFACE, S, S, 16, 0xf800, 0x07e0, 0x001f, 0);
		Uint32 key = SDL_MapRGB(sp->format, 255, 0, 255);
		SDL_FillRect(sp, NULL, key);
		for(int k=0; k<9; k++) {
			float ang = k * 6.2831853f / 9 + 0.3f + (st == 2 ? 0.5f * ((k * 7) % 3 - 1) : 0.0f);
			float len = (st == 0 ? 30.0f : (st == 1 ? 24.0f : 14.0f)) * (0.85f + 0.15f * ((k * 5) % 3) / 2.0f);
			for(float t=4; t<len; t+=0.5f) {
				float w = 2.4f * (1.0f - t / (len + 4));
				float px = S / 2 + cosf(ang) * t, py = S / 2 + sinf(ang) * t * 0.85f;
				for(int oy=-2; oy<=2; oy++) for(int ox=-2; ox<=2; ox++) {
					if(ox * ox + oy * oy > w * w) continue;
					int x = (int)px + ox, y = (int)py + oy;
					if(x < 0 || y < 0 || x >= S || y >= S) continue;
					float lit = 0.7f + 0.5f * (t / len);
					int v = (int)(150 * lit);
					*(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format, v, v - 6, v - 22);
				}
			}
		}
		// a worn base to which the spears are fixed
		for(int y=0; y<S; y++) for(int x=0; x<S; x++) {
			float dx = x - S / 2.0f + 0.5f, dy = (y - S / 2.0f + 0.5f) / 0.85f;
			if(dx * dx + dy * dy < 36.0f)
				*(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format, 60, 46, 34);
		}
		SDL_SetColorKey(sp, SDL_SRCCOLORKEY, key);
		spike_spr[st] = sp;
	}
	// the glue puddle: a glossy amber blob, in three states of dryness
	for(int st=0; st<3; st++) {
		const int W_ = 104, H_ = 88;
		SDL_Surface *sp = SDL_CreateRGBSurface(SDL_SWSURFACE, W_, H_, 16, 0xf800, 0x07e0, 0x001f, 0);
		Uint32 key = SDL_MapRGB(sp->format, 255, 0, 255);
		SDL_FillRect(sp, NULL, key);
		for(int y=0; y<H_; y++) for(int x=0; x<W_; x++) {
			float dx = x - W_ / 2.0f + 0.5f, dy = (y - H_ / 2.0f + 0.5f) / 0.85f;
			float d = sqrtf(dx * dx + dy * dy), th = atan2f(dy, dx);
			float edge = 44.0f * (st == 0 ? 1.0f : (st == 1 ? 0.88f : 0.74f)) * (0.88f + 0.08f * sinf(5 * th + 1.0f) + 0.05f * sinf(9 * th + 2.0f));
			if(d > edge) continue;
			float depth = 1.0f - d / edge;
			float lit = 0.8f + 0.35f * (-dx * 0.5f - dy * 0.6f) / 44.0f;
			float dry = st == 0 ? 0.0f : (st == 1 ? 0.35f : 0.65f);			// dries towards a pale film
			float r = (200 + 50 * depth * lit) * (1 - dry) + 196 * dry;
			float g = (176 + 50 * depth * lit) * (1 - dry) + 150 * dry;
			float b = (50 + 20 * depth) * (1 - dry) + 96 * dry;
			if(d > edge - 3.0f) { r *= 0.62f; g *= 0.62f; b *= 0.6f; }			// the rim
			// a glossy highlight and a few bubbles
			float hx = dx + 14, hy = dy + 14;
			if(hx * hx * 0.5f + hy * hy < 30.0f && st == 0) { r = 255; g = 252; b = 200; }
			int hh = (x * 11 + y * 7) % 41;
			if(st < 2 && hh == 0 && d < edge - 8) { r = 255; g = 250; b = 190; }
			if(st == 2 && ((x / 4 + y / 4) % 3 == 0)) continue;			// almost gone: patchy
			*(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format,
				(int)fminf(r, 255), (int)fminf(g, 255), (int)fminf(b, 255));
		}
		SDL_SetColorKey(sp, SDL_SRCCOLORKEY, key);
		glue_spr[st] = sp;
	}

	// the glow of a napalm field
	{
		const int S = 128;
		fire_glow = SDL_CreateRGBSurface(SDL_SWSURFACE, S, S, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
		for(int y=0; y<S; y++) for(int x=0; x<S; x++) {
			float d = sqrtf((x - S / 2.0f + 0.5f) * (x - S / 2.0f + 0.5f) + (y - S / 2.0f + 0.5f) * (y - S / 2.0f + 0.5f)) / (S / 2.0f);
			float a = d >= 1.0f ? 0.0f : (1.0f - d) * (1.0f - d) * 0.55f + (d < 0.6f ? 0.18f : 0.0f);
			Uint8 *p = (Uint8 *)fire_glow->pixels + y * fire_glow->pitch + x * 4;
			p[0] = 255; p[1] = 120; p[2] = 30;
			p[3] = (Uint8)(a * 255);
		}
	}
}

void trap_shift(float dx, float dy) {
	for(int i=0; i<MAX_TRAPS; i++) { traps[i].x += dx; traps[i].y += dy; }
	for(int i=0; i<MAX_TBOXES; i++) { tbox[i].x += dx; tbox[i].y += dy; }
}

static void spawn_trap_box(float x, float y, int type) {
	for(int i=0; i<MAX_TBOXES; i++) {
		if(!tbox[i].ok) {
			tbox[i].ok = true;
			tbox[i].x = x;
			tbox[i].y = y;
			tbox[i].type = type;
			tbox[i].life = 60 * 60;
			return;
		}
	}
}

static void lay_trap() {
	float dx, dy;
	move_xy(1.0f, add_angle(player.angle, -60), &dx, &dy);			// the way the player aims; the trap goes behind
	float x = player.x - dx * 26, y = player.y - dy * 26;
	if(world_blocked((int)x, (int)y, false))
		{ x = player.x; y = player.y; }
	for(int i=0; i<MAX_TRAPS; i++) {
		if(!traps[i].ok) {
			traps[i].ok = true;
			traps[i].type = trap_loaded;
			traps[i].x = x;
			traps[i].y = y;
			traps[i].integrity = TRAP_INTEGRITY[trap_loaded];
			traps[i].life = TRAP_LIFE[trap_loaded];
			if(trap_loaded == TRAP_NAPALM) {
				make_crater_sprite();
				SDL_Rect r;
				r.x = (int)x - crater_spr->w / 2;
				r.y = (int)y - crater_spr->h / 2;
				r.w = crater_spr->w;
				r.h = crater_spr->h;
				SDL_BlitSurface(crater_spr, NULL, cur_bground, &r);
			}
			if(config.sound)
				Mix_PlayChannel(-1, snd_throw, 0);
			trap_charges--;
			trap_cool = 40;
			if(trap_charges <= 0)
				trap_loaded = -1;
			return;
		}
	}
	set_message("too many traps already");
}

static void trap_update(int here) {
	// R2 lays a trap
	bool t = key[SDLK_t] != 0;
	if(t && !t_prev && trap_loaded >= 0 && trap_charges > 0 && trap_cool <= 0 && player.ok && !enhanced_in_bunker())
		lay_trap();
	t_prev = t;
	if(trap_cool > 0)
		trap_cool--;

	// crates turn up now and then while the player is out in an arm
	if(here > 0 && player.ok) {
		if(--tbox_timer <= 0) {
			tbox_timer = 60 * (RAND(70, 120));
			int have = 0;
			for(int i=0; i<MAX_TBOXES; i++)
				if(tbox[i].ok) have++;
			if(have == 0) {
				for(int tries=0; tries<50; tries++) {
					int lx = RAND(100, CELL_W - 100), ly = RAND(100, CELL_H - 100);
					int x = lx + (CELL_GX[here] - gx) * CELL_W, y = ly + (CELL_GY[here] - gy) * CELL_H;
					if(world_blocked(x, y, false) || world_blocked(x + 16, y, false) || world_blocked(x - 16, y, false) ||
					   world_blocked(x, y + 16, false) || world_blocked(x, y - 16, false) || world_thorns(x, y))
						continue;
					if(distance((float)x, (float)y, player.x, player.y) < 120)
						continue;
					spawn_trap_box((float)x, (float)y, RAND(0, NUM_TRAPS - 1));
					break;
				}
			}
		}
	}
	for(int i=0; i<MAX_TBOXES; i++) {
		if(!tbox[i].ok)
			continue;
		if(--tbox[i].life <= 0) { tbox[i].ok = false; continue; }
		if(player.ok && distance(player.x, player.y, tbox[i].x, tbox[i].y) < 26) {
			// one kind of trap at a time: a new crate replaces whatever was left (a crate of the same kind refills it)
			int type = tbox[i].type;
			trap_loaded = type;
			trap_charges = TRAP_CHARGES[type];
			tbox[i].ok = false;
			char buf[64];
			snprintf(buf, sizeof(buf), "%s x%d: R2 lays one, stay off it", TRAP_NAME[type], trap_charges);
			set_message(buf);
			if(config.sound)
				Mix_PlayChannel(-1, snd_power_up, 0);
		}
	}

	// the traps at work
	int fires = 0;
	for(int i=0; i<MAX_TRAPS; i++) {
		TRAP *tr = &traps[i];
		if(!tr->ok)
			continue;
		int ty = tr->type;
		tr->life--;
		if(ty == TRAP_NAPALM)
			tr->integrity -= TRAP_INTEGRITY[TRAP_NAPALM] / TRAP_LIFE[TRAP_NAPALM] * 1.0f;		// the fuel burns off with time
		bool tick = (enhanced_ticks + i) % TRAP_PERIOD[ty] == 0;
		for(CRITTER *c = critter_first; c; c = c->next) {
			if(c->remove || c->hidden)
				continue;
			if(distance(c->x, c->y, tr->x, tr->y) > TRAP_RADIUS[ty] + c->hs * 0.5f)
				continue;
			if(ty == TRAP_SPIKES) {			// stuck on the spears: slowed (a Tyrant hardly notices)
				float k = (c->type == CT_TYRANT) ? 0.94f : 0.78f;
				c->dirx *= k;
				c->diry *= k;
				c->slow = 12;
			}
			if(ty == TRAP_GLUE && c->type == CT_TYRANT) {		// too big to be held: only slowed
				c->dirx *= 0.6f;
				c->diry *= 0.6f;
				c->slow = 12;
				tr->integrity -= 0.25f;
				continue;
			}
			if(ty == TRAP_GLUE) {			// stuck fast: the middle holds a bug still, the rim slows it hard
				c->dirx *= 0.1f;
				c->diry *= 0.1f;
				c->slow = 12;
				if(distance(c->x, c->y, tr->x, tr->y) < TRAP_RADIUS[TRAP_GLUE] * 0.55f)
					c->dirx = c->diry = 0;
				tr->integrity -= 0.25f;		// every bug in it wears it down
				continue;
			}
			if(!tick)
				continue;
			int dam = TRAP_DAMAGE[ty] > 0 ? (int)(TRAP_DAMAGE[ty] * tune.trap_damage_mult + 0.5f) : 0;
			if(TRAP_DAMAGE[ty] > 0 && dam < 1)
				dam = 1;
			if(ty == TRAP_NAPALM)
				c->burn = BURN_TICKS;				// keeps burning after it has left the field
			else
				c->bleed = BLEED_TICKS;				// the spears leave it bleeding and easier to hurt
			c->health -= (ty == TRAP_NAPALM) ? enhanced_damage(c, dam) : dam;
			tr->integrity -= dam * (ty == TRAP_NAPALM ? 0.5f : 1.0f);
			if(ty == TRAP_NAPALM && RAND(0,2) == 0)
				add_particle(c->x + RANDF(-8, 8), c->y + RANDF(-8, 6), RANDF(-0.4, 0.4), RANDF(-1.6, -0.6), 255, RAND(90,170), 20, RAND(8,16));
			if(ty == TRAP_SPIKES && RAND(0,3) == 0)
				c->blood(RAND(1,2), RANDF(0, 359));
			if(c->health <= 0)
				c->die(RANDF(0, 359));
		}
		// the player is no safer on a trap than a bug is: same damage, same slowing, same wear on the trap
		if(player.ok && !enhanced_in_bunker() && distance(player.x, player.y, tr->x, tr->y) < TRAP_RADIUS[ty]) {
			if(ty == TRAP_SPIKES) {
				player.dirx *= 0.78f;
				player.diry *= 0.78f;
				player_slow = 12;
			}
			if(ty == TRAP_GLUE) {			// the same for the player, a little less sticky in the middle so it can wade out
				player.dirx *= 0.15f;
				player.diry *= 0.15f;
				player_slow = 12;
				tr->integrity -= 0.25f;
			}
			if(tick && TRAP_DAMAGE[ty] > 0) {
				int dam = (int)(TRAP_DAMAGE[ty] * tune.trap_damage_mult + 0.5f);
				if(dam < 1)
					dam = 1;
				if(ty == TRAP_NAPALM)
					player_burn = BURN_TICKS;
				else
					player_bleed = BLEED_TICKS;
				player.health -= (ty == TRAP_NAPALM && player_bleed > 0) ? scaled_int(dam, BLEED_VULN) : dam;
				tr->integrity -= dam * (ty == TRAP_NAPALM ? 0.5f : 1.0f);
				if(ty == TRAP_NAPALM)
					add_particle(player.x + RANDF(-8, 8), player.y + RANDF(-8, 6), RANDF(-0.4, 0.4), RANDF(-1.6, -0.6), 255, RAND(90,170), 20, RAND(8,16));
				else
					add_particle(player.x + RANDF(-8, 8), player.y + RANDF(-4, 8), RANDF(-0.5, 0.5), RANDF(-0.9, 0.1), 200, 40, 40, RAND(8,14));
				if(player.health <= 0)
					player.die();
			}
		}
		// flames of a napalm field
		if(ty == TRAP_NAPALM && fires < 2 && tr->integrity > 0) {
			fires++;
			float frac = tr->integrity / TRAP_INTEGRITY[TRAP_NAPALM];
			if(enhanced_ticks % 2 == 0) {
				int n = frac > 0.5f ? 2 : 1;
				for(int k=0; k<n; k++) {
					float a = RANDF(0, 6.2831853f), r = RANDF(0, TRAP_RADIUS[TRAP_NAPALM] * 0.9f);
					add_particle(tr->x + cosf(a) * r, tr->y + sinf(a) * r * 0.85f, RANDF(-0.25, 0.25), RANDF(-2.0, -0.8), 255, RAND(80,170), 20, RAND(10,22));
				}
			}
		}
		if(tr->life <= 0 || tr->integrity <= 0) {
			tr->ok = false;
			for(int k=0; k<6; k++)
				add_particle(tr->x + RANDF(-18, 18), tr->y + RANDF(-10, 10), RANDF(-0.4, 0.4), RANDF(-0.9, -0.3), 120, 110, 100, RAND(14,30));
		}
	}
}

static void traps_draw_under(SDL_Surface *bmp) {
	if(!fire_glow)
		return;
	for(int i=0; i<MAX_TBOXES; i++) {
		if(tbox[i].ok && (tbox[i].life > 240 || (tbox[i].life / 8) % 2 == 0))
			{
				blit_center(bmp, glow_item[4 + tbox[i].type], (int)tbox[i].x, (int)tbox[i].y);
				blit_center(bmp, tbox_spr[tbox[i].type], (int)tbox[i].x, (int)tbox[i].y);
			}
	}
	for(int i=0; i<MAX_TRAPS; i++) {
		if(!traps[i].ok)
			continue;
		float f = traps[i].integrity / TRAP_INTEGRITY[traps[i].type];
		float lf = (float)traps[i].life / TRAP_LIFE[traps[i].type];
		if(lf < f) f = lf;
		if(traps[i].type == TRAP_SPIKES)
			blit_center(bmp, spike_spr[f > 0.6f ? 0 : (f > 0.3f ? 1 : 2)], (int)traps[i].x, (int)traps[i].y);
		else if(traps[i].type == TRAP_GLUE)
			blit_center(bmp, glue_spr[f > 0.6f ? 0 : (f > 0.3f ? 1 : 2)], (int)traps[i].x, (int)traps[i].y);
		else if(f > 0.15f)
			blit_center(bmp, fire_glow, (int)traps[i].x, (int)traps[i].y);
	}
}

///////////////////////////////////////////////////////////////////////////////
// The Hive Tyrant. After a long time in the game (5 minutes, then 7 to 8 minutes after each Tyrant is slain) the ground
// trembles for 15 seconds and a Hive Tyrant emerges: an elite boss with one or two mutations. The weakest bugs (drones,
// runners) run from it; the heavier ones screen for it. Killing it fully heals the player and gives the Tyrant's spoils
// for 90 seconds; afterwards things settle, but every bug is a little tougher for good (hive_level).

#define MUT_BROOD	0
#define MUT_CHARGER	1
#define MUT_SPITTER	2
#define MUT_STOMPER	3
#define NUM_MUTS	4
static const char *MUT_NAME[NUM_MUTS] = { "brood-mother", "charger", "spitter", "stomper" };

#define TY_WAITING	0
#define TY_WARNING	1
#define TY_ACTIVE	2

static int ty_phase = TY_WAITING;
static long ty_next = 60 * 60 * 5;				// when the next one stirs
static int ty_warn = 0;
static int ty_cycle = 0;
static int ty_muts[2] = { 0, 0 };
static int ty_nmuts = 1;
static int ty_sprite_next = 0;
static bool ty_alive = false;					// known from the boss's own update (no pointers into the bug list)
static float ty_x = 0, ty_y = 0;
static int ty_hp = 0, ty_maxhp = 1;
static int ty_missing = 0;
// per-mutation clocks
static int cd_brood = 0, cd_spit = 0, cd_stomp = 0, cd_charge = 0;
static int stomp_t = 0;
static int charge_state = 0, charge_t = 0;		// 0 idle, 1 winding up, 2 charging
static float charge_dx = 0, charge_dy = 0;
static bool charge_hit = false;
static int ty_alert = 0;						// a "!" shows over the Tyrant while it winds up

struct ACID {
	bool ok;
	float x, y;
	int warn;				// ticks until it lands (a ring shows where)
	int life;				// ticks the pool lasts after landing
};
#define MAX_ACID	5
static ACID acid[MAX_ACID];
static SDL_Surface *acid_spr = NULL;

bool enhanced_tyrant_active() {
	return ty_alive;
}

int enhanced_reload() {
	if(enhanced_flame_active())
		return (int)tune.flame_fire_delay_ticks;
	return (int)(enhanced_ticks < boon_until ? tune.boon_fire_delay_ticks : tune.fire_delay_ticks);
}

static bool has_mut(int m) {
	for(int i=0; i<ty_nmuts; i++)
		if(ty_muts[i] == m)
			return true;
	return false;
}

// the weak run away, the strong stand in front of it
int enhanced_bug_mode(CRITTER *c) {
	if(!ty_alive || c->type == CT_TYRANT || c->guard || c->brood || c->hidden || c->type == CT_BURROWER || c->type == CT_STUNNER || c->type == CT_PHANTOM)
		return 0;
	if(c->type == CT_DRONE || c->type == CT_RUNNER)
		return 1;
	return 2;
}

void enhanced_bug_target(CRITTER *c, float *tx, float *ty) {
	if(enhanced_bug_mode(c) == 1) {					// away from the Tyrant, as far as the ground allows
		float ax = c->x - ty_x, ay = c->y - ty_y;
		float l = sqrtf(ax * ax + ay * ay);
		if(l < 1) { ax = 1; ay = 0; l = 1; }
		*tx = c->x + ax / l * 300;
		*ty = c->y + ay / l * 300;
		return;
	}
	// a screen in front of the Tyrant, between it and the player; bugs close to the player just fight
	float px = player.x - ty_x, py = player.y - ty_y;
	float l = sqrtf(px * px + py * py);
	if(l < 1 || distance(c->x, c->y, player.x, player.y) < 150)
		return;
	px /= l;
	py /= l;
	int h = (int)(((long)c >> 4) % 9);
	float front = 100.0f + (h % 3) * 26.0f;
	float side = ((h / 3) - 1) * 44.0f;
	*tx = ty_x + px * front - py * side;
	*ty = ty_y + py * front + px * side;
}

static void make_acid_sprite() {
	if(acid_spr)
		return;
	const int W_ = 92, H_ = 76;
	acid_spr = SDL_CreateRGBSurface(SDL_SWSURFACE, W_, H_, 16, 0xf800, 0x07e0, 0x001f, 0);
	Uint32 key = SDL_MapRGB(acid_spr->format, 255, 0, 255);
	SDL_FillRect(acid_spr, NULL, key);
	for(int y=0; y<H_; y++) for(int x=0; x<W_; x++) {
		float dx = x - W_ / 2.0f + 0.5f, dy = (y - H_ / 2.0f + 0.5f) / 0.82f;
		float d = sqrtf(dx * dx + dy * dy), th = atan2f(dy, dx);
		float edge = 40.0f * (0.86f + 0.08f * sinf(4 * th + 0.5f) + 0.06f * sinf(7 * th + 1.3f));
		if(d > edge) continue;
		float depth = 1.0f - d / edge;
		float r = 60 + 60 * depth, g = 150 + 90 * depth, b = 30 + 20 * depth;
		if(d > edge - 3.0f) { r *= 0.55f; g *= 0.6f; b *= 0.5f; }
		if(((x * 13 + y * 7) % 37) == 0 && d < edge - 8) { r = 210; g = 255; b = 150; }
		*(Uint16 *)((Uint8 *)acid_spr->pixels + y * acid_spr->pitch + x * 2) = (Uint16)SDL_MapRGB(acid_spr->format,
			(int)fminf(r, 255), (int)fminf(g, 255), (int)fminf(b, 255));
	}
	SDL_SetColorKey(acid_spr, SDL_SRCCOLORKEY, key);
}

static void tyrant_sprites_step(int n) {
	while(n-- > 0 && ty_sprite_next < 105) {
		int f = ty_sprite_next / 35, a = ty_sprite_next % 35;
		if(!ctype_gfx[CT_TYRANT][f][a] && critter_gfx[f][a] && critter_gfx[f][a]->format->BitsPerPixel == 16)
			ctype_gfx[CT_TYRANT][f][a] = make_variant(critter_gfx[f][a], CT_TYRANT);
		ty_sprite_next++;
	}
}

static void spawn_brood(CRITTER *boss) {
	for(int k=0; k<3; k++) {
		float x = boss->x + RANDF(-50, 50), y = boss->y + RANDF(-50, 50);
		if(world_blocked((int)x, (int)y, false)) { x = boss->x; y = boss->y; }
		CRITTER *c = add_critter(x, y, 0);
		critter_set_type(c, CT_RUNNER);
		c->brood = true;
		for(int q=0; q<4; q++)
			add_particle(x, y, RANDF(-1.5, 1.5), RANDF(-1.5, 1.5), 200, 60, 90, (RAND(8,18)));
	}
}

static void launch_acid(float x, float y) {
	for(int i=0; i<MAX_ACID; i++) {
		if(!acid[i].ok) {
			acid[i].ok = true;
			acid[i].x = x;
			acid[i].y = y;
			acid[i].warn = 48;
			acid[i].life = 60 * 7;
			return;
		}
	}
}

static void hurt_player(int dmg, float kx, float ky) {
	if(!player.ok)
		return;
	if(enhanced_bunker_absorb(dmg))
		return;
	player.health -= dmg;
	player.dirx += kx;
	player.diry += ky;
	if(player.health <= 0)
		player.die();
}

bool enhanced_tyrant_update(CRITTER *c) {
	ty_alive = true;
	ty_x = c->x;
	ty_y = c->y;
	ty_hp = c->health;
	ty_maxhp = c->maxhp > 0 ? c->maxhp : 1;
	ty_missing = 0;
	ty_alert = 0;
	if(!player.ok)
		return false;
	float dist = distance(c->x, c->y, player.x, player.y);

	// dazed after a charge: stands still, takes extra damage (enhanced_damage)
	if(c->stun > 0) {
		c->stun--;
		c->frame = 1;
		c->dirx = c->diry = 0;
		return true;
	}

	// brood-mother: new runners that are not afraid of it
	if(has_mut(MUT_BROOD) && --cd_brood <= 0) {
		cd_brood = 60 * (RAND(5, 7));
		int brood = 0;
		for(CRITTER *o = critter_first; o; o = o->next)
			if(o->brood && !o->remove)
				brood++;
		if(brood < 9)
			spawn_brood(c);
	}
	// spitter: acid lands where the player stands
	if(has_mut(MUT_SPITTER) && --cd_spit <= 0 && dist < 560) {
		cd_spit = 60 * (RAND(3, 5));
		launch_acid(player.x + RANDF(-12, 12), player.y + RANDF(-12, 12));
	}
	// stomper: a slow wind-up, then a shockwave all around
	if(has_mut(MUT_STOMPER)) {
		if(stomp_t > 0) {
			ty_alert = 2;
			if(--stomp_t == 0) {
				float r = 150.0f;
				for(int k=0; k<30; k++) {
					float a = k * 6.2831853f / 30;
					add_particle(c->x + cosf(a) * 40, c->y + sinf(a) * 34, cosf(a) * 3.4f, sinf(a) * 3.0f, 170, 130, 80, (RAND(14,26)));
				}
				if(config.sound)
					Mix_PlayChannel(-1, snd_explo[0], 0);
				if(dist < r) {
					float kx, ky;
					move_xy(6.0f, compute_angle(player.x, player.y, c->x, c->y), &kx, &ky);
					hurt_player(28, kx, ky);
				}
				make_crater_sprite();
				SDL_Rect rc;
				rc.x = (int)c->x - crater_spr->w / 2; rc.y = (int)c->y - crater_spr->h / 2; rc.w = crater_spr->w; rc.h = crater_spr->h;
				SDL_BlitSurface(crater_spr, NULL, cur_bground, &rc);
			}
			else if(stomp_t % 6 == 0)
				add_particle(c->x + RANDF(-30, 30), c->y + RANDF(10, 34), RANDF(-0.4, 0.4), RANDF(-1.0, -0.2), 170, 130, 80, (RAND(10,18)));
			c->frame = 1;
			c->dirx = c->diry = 0;
			return true;
		}
		if(--cd_stomp <= 0 && dist < 230) {
			stomp_t = 54;
			cd_stomp = 60 * (RAND(6, 9));
		}
	}
	// charger: stands and glares, then rushes along a straight line, then is dazed
	if(has_mut(MUT_CHARGER)) {
		if(charge_state == 1) {
			ty_alert = 1;
			c->dirx = c->diry = 0;
			if(--charge_t <= 0) {
				float ang = compute_angle(player.x, player.y, c->x, c->y);
				move_xy(1.0f, ang, &charge_dx, &charge_dy);
				charge_state = 2;
				charge_t = 40;
				charge_hit = false;
			}
			return true;
		}
		if(charge_state == 2) {
			float nx = c->x + charge_dx * 7.0f, ny = c->y + charge_dy * 7.0f;
			if(world_blocked((int)nx, (int)ny, false) || --charge_t <= 0) {
				bool crashed = world_blocked((int)nx, (int)ny, false);
				charge_state = 0;
				cd_charge = 60 * (RAND(8, 12));
				if(crashed) {
					c->stun = 120;					// slammed into the rock: dazed
					for(int k=0; k<18; k++)
						add_particle(c->x, c->y, RANDF(-3, 3), RANDF(-3, 3), 190, 150, 90, (RAND(12,26)));
				}
				return true;
			}
			c->x = nx;
			c->y = ny;
			c->sx = nx;
			c->sy = ny;
			if(RAND(0,2) == 0)
				add_particle(c->x, c->y + 12, RANDF(-0.8, 0.8), RANDF(-0.8, 0.2), 170, 130, 80, (RAND(10,20)));
			if(!charge_hit && distance(c->x, c->y, player.x, player.y) < c->hs + 22) {
				charge_hit = true;
				hurt_player(30, charge_dx * 8.0f, charge_dy * 8.0f);
			}
			c->frame += 0.4f;
			if((int)c->frame > 3) c->frame = 0;
			return true;
		}
		if(--cd_charge <= 0 && dist > 170 && dist < 540) {
			charge_state = 1;
			charge_t = 54;
		}
	}
	return false;
}

static void acid_update() {
	for(int i=0; i<MAX_ACID; i++) {
		if(!acid[i].ok)
			continue;
		if(acid[i].warn > 0) {
			if(--acid[i].warn == 0)
				for(int k=0; k<10; k++)
					add_particle(acid[i].x, acid[i].y, RANDF(-2, 2), RANDF(-2, 1), 120, 230, 60, (RAND(8,16)));
			continue;
		}
		if(--acid[i].life <= 0) {
			acid[i].ok = false;
			continue;
		}
		if(player.ok && !enhanced_in_bunker() && distance(player.x, player.y, acid[i].x, acid[i].y) < 38 && enhanced_ticks % 5 == 0) {
			player.health -= 2;
			if(RAND(0,2) == 0)
				add_particle(player.x + RANDF(-8, 8), player.y + RANDF(-6, 6), RANDF(-0.5, 0.5), RANDF(-1.0, -0.2), 120, 230, 60, (RAND(8,14)));
			if(player.health <= 0)
				player.die();
		}
	}
}

static void tyrant_spawn() {
	// somewhere well away from the player on open ground: usually in the next screen over, so that it walks in
	float bx = 0, by = 0, best = -1;
	for(int tries=0; tries<200; tries++) {
		int ci = (RAND(0, 4));
		int lx = (RAND(90, CELL_W - 90)), ly = (RAND(90, CELL_H - 90));
		float x = (float)(lx + (CELL_GX[ci] - gx) * CELL_W), y = (float)(ly + (CELL_GY[ci] - gy) * CELL_H);
		float d = distance(x, y, player.x, player.y);
		if(d < 250)
			continue;
		bool ok = true;
		for(int dy2=-1; dy2<=1 && ok; dy2++) for(int dx2=-1; dx2<=1 && ok; dx2++)
			if(world_blocked((int)x + dx2 * 34, (int)y + dy2 * 28, false))
				ok = false;
		if(!ok)
			continue;
		if(d > best) {
			best = d;
			bx = x;
			by = y;
		}
		if(d >= 380)
			break;
	}
	if(best < 0) {
		bx = player.x + 300;
		by = player.y;
		enhanced_unstick(&bx, &by);
	}
	CRITTER *c = add_critter(bx, by, 0);
	critter_set_type(c, CT_TYRANT);
#ifdef FORMIDO_TESTHOOKS
	if(getenv("FORMIDO_TYRANTHP")) {
		c->maxhp = atoi(getenv("FORMIDO_TYRANTHP"));
		c->health = c->maxhp;
	}
#endif
#ifdef FORMIDO_TESTHOOKS
	bot_tyrants_seen++;
#endif
	ty_alive = true;
	ty_x = bx;
	ty_y = by;
	ty_hp = c->health;
	ty_maxhp = c->maxhp;
	ty_phase = TY_ACTIVE;
	cd_brood = 60 * 3;
	cd_spit = 60 * 4;
	cd_stomp = 60 * 5;
	cd_charge = 60 * 4;
	stomp_t = 0;
	charge_state = 0;
	for(int k=0; k<40; k++)
		add_particle(bx + RANDF(-30, 30), by + RANDF(-20, 30), RANDF(-3, 3), RANDF(-3, 1), 150, 100, 60, (RAND(18,40)));
	char buf[80];
	if(ty_nmuts == 1)
		snprintf(buf, sizeof(buf), "a hive tyrant emerges: the %s", MUT_NAME[ty_muts[0]]);
	else
		snprintf(buf, sizeof(buf), "hive tyrant: %s and %s", MUT_NAME[ty_muts[0]], MUT_NAME[ty_muts[1]]);
	set_message(buf);
	msg_timer = 60 * 7;
}

void enhanced_tyrant_slain(float x, float y) {
	ty_alive = false;
	ty_phase = TY_WAITING;
	ty_cycle++;
	hive_level++;
	stat_tyrants++;
	ty_next = enhanced_ticks + (long)(3600 * tune.tyrant_interval_minutes) + 60 * (long)(RAND(0, 60));
	boon_until = enhanced_ticks + (long)(60 * tune.boon_seconds);
	charge_state = 0;
	stomp_t = 0;
	for(int i=0; i<MAX_ACID; i++)
		acid[i].ok = false;
	// the fall of the Tyrant
	for(int k=0; k<3; k++)
		create_explosion((int)(x + RANDF(-40, 40)), (int)(y + RANDF(-30, 30)), (RAND(0,1)), RANDF(0.3f, 0.5f));
	for(int k=0; k<50; k++)
		add_particle(x, y, RANDF(-4, 4), RANDF(-4, 4), RAND(200,255), RAND(150,220), RAND(40,110), (RAND(18,40)));
	if(config.sound)
		Mix_PlayChannel(-1, snd_explo[1], 0);
	// the spoils: fully healed, hits count more, faster fire, a little quicker, a steady mend, for 90 seconds
	if(player.ok)
		player.health = 200;
	prev_health = player.health;
	set_message("the tyrant falls: take the hive's spoils");
	msg_timer = 60 * 7;
}

static void tyrant_update() {
	// the boon's regeneration
	if(enhanced_ticks < boon_until && player.ok && enhanced_ticks % 30 == 0 && player.health < 200) {
		player.health++;
		prev_health = player.health;
	}
	if(enhanced_ticks < boon_until && player.ok && RAND(0,24) == 0)
		add_particle(player.x + RANDF(-14, 14), player.y + RANDF(-6, 10), RANDF(-0.2, 0.2), RANDF(-0.9, -0.3), 255, 215, 80, (RAND(18,34)));
	acid_update();

	if(ty_phase == TY_WAITING) {
		if(enhanced_ticks >= ty_next && player.ok) {
			ty_phase = TY_WARNING;
			ty_warn = 60 * 15;
			ty_sprite_next = 0;
			ty_nmuts = ty_cycle >= 1 ? 2 : 1;			// the first one has a single power, later ones two
			ty_muts[0] = (RAND(0, NUM_MUTS - 1));
			do { ty_muts[1] = (RAND(0, NUM_MUTS - 1)); } while(ty_muts[1] == ty_muts[0]);
#ifdef FORMIDO_TESTHOOKS
			if(getenv("FORMIDO_MUT"))
				ty_muts[0] = atoi(getenv("FORMIDO_MUT")) % NUM_MUTS;
			if(getenv("FORMIDO_MUT2")) {
				ty_nmuts = 2;
				ty_muts[1] = atoi(getenv("FORMIDO_MUT2")) % NUM_MUTS;
			}
			if(getenv("FORMIDO_TYRANT"))
				ty_warn = 60 * 4;
#endif
			set_message("the ground trembles: something vast is stirring");
			msg_timer = 60 * 6;
		}
	}
	else if(ty_phase == TY_WARNING) {
		tyrant_sprites_step(4);
		make_acid_sprite();
		if(ty_warn == 60 * 5) {
			set_message("the hive tyrant is almost here");
			msg_timer = 60 * 4;
		}
		if(--ty_warn <= 0) {
			tyrant_sprites_step(105);
			tyrant_spawn();
		}
	}
	else {
#ifdef FORMIDO_TESTHOOKS
		static int active_ticks = 0;
		if(getenv("FORMIDO_KILLTYRANT") && ++active_ticks > 300) {		// test: slay it after 5 seconds
			active_ticks = -1000000;
			for(CRITTER *c = critter_first; c; c = c->next)
				if(c->type == CT_TYRANT && !c->remove) {
					c->health = 0;
					c->die(0);
					break;
				}
		}
#endif
		// safety: if the Tyrant vanished without being slain (it should not), start the wait again
		if(!ty_alive || ++ty_missing > 600) {
			if(ty_missing > 600 || !ty_alive) {
				ty_alive = false;
				ty_phase = TY_WAITING;
				ty_next = enhanced_ticks + 60 * 60 * 3;
				ty_missing = 0;
			}
		}
		else if(false)
			ty_missing = 0;
	}
}

static void tyrant_draw_under(SDL_Surface *bmp) {
	if(!acid_spr)
		return;
	for(int i=0; i<MAX_ACID; i++) {
		if(!acid[i].ok)
			continue;
		if(acid[i].warn > 0) {			// where it will land: a red ring that closes in
			float r = 8.0f + acid[i].warn * 0.7f;
			for(int k=0; k<28; k++) {
				float a = k * 6.2831853f / 28;
				rect(bmp, (int)(acid[i].x + cosf(a) * r), (int)(acid[i].y + sinf(a) * r * 0.82f), 2, 2, 255, 60, 40);
			}
		}
		else
			blit_center(bmp, acid_spr, (int)acid[i].x, (int)acid[i].y);
	}
}

static void tyrant_draw_over(SDL_Surface *bmp) {
	// the boon, while it lasts
	if(enhanced_ticks < boon_until) {
		int secs = (int)((boon_until - enhanced_ticks) / 60);
		char line[64];
		snprintf(line, sizeof(line), "tyrant's spoils  %d:%02d", secs / 60, secs % 60);
		rect(bmp, 8, SCREEN_H - 138, 7, 7, 255, 215, 80);
		font_write(bmp, FONT_NORMAL, 20, SCREEN_H - 143, line);
	}

	if(!ty_alive)
		return;
	// the boss bar across the top
	int w = 300, x0 = (SCREEN_W - w) / 2, y0 = 30;
	float f = ty_hp > 0 ? (float)ty_hp / ty_maxhp : 0.0f;
	rect(bmp, x0 - 2, y0 - 2, w + 4, 12, 18, 10, 12);
	rect(bmp, x0, y0, w, 8, 70, 20, 30);
	rect(bmp, x0, y0, (int)(w * f), 8, f > 0.5f ? 210 : 235, f > 0.5f ? 50 : 120, f > 0.5f ? 60 : 40);
	char name[80];
	if(ty_nmuts == 1)
		snprintf(name, sizeof(name), "hive tyrant: %s", MUT_NAME[ty_muts[0]]);
	else
		snprintf(name, sizeof(name), "hive tyrant: %s, %s", MUT_NAME[ty_muts[0]], MUT_NAME[ty_muts[1]]);
	font_write(bmp, FONT_NORMAL, (SCREEN_W - font_length(FONT_NORMAL, name)) / 2, y0 + 14, name);
	// what it is about to do
	if(ty_alert == 1)
		font_write(bmp, FONT_BOLD, (int)ty_x - 10, (int)ty_y - 90, (char *)"!");
	if(ty_alert == 2) {
		float r = 150.0f * (1.0f - stomp_t / 54.0f);
		for(int k=0; k<32; k++) {
			float a = k * 6.2831853f / 32;
			rect(bmp, (int)(ty_x + cosf(a) * r), (int)(ty_y + sinf(a) * r * 0.85f), 2, 2, 255, 150, 60);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Hallucinations: while the mushroom's effect (player.drugs) lasts, phantoms appear around the player. They look like
// flickering violet bugs and bite like real ones, but vanish the moment the effect ends. Killing one may leave a healing
// plant where it fell, so eating the mushroom is not a pure penalty.

#define MAX_PHANTOMS	6

// Spores: now and then a phantom leaves a spore pod. It is a mushroom with a different look: the same effect when picked up.
// Guards against an endless chain: at most one spore around, a modest chance, it rots in 25 s, and taking one locks out spore
// drops for 150 seconds (the effect itself lasts 10 to 15).
#define SPORE_CHANCE	(tune.spore_chance)
#define SPORE_LIFE		(60 * 25)
#define SPORE_LOCKOUT	((int)(60 * tune.spore_lockout_seconds))
static bool spore_ok = false;
static float spore_x = 0, spore_y = 0;
static int spore_life = 0;
static long spore_lock_until = 0;
static SDL_Surface *spore_spr = NULL, *spore_glow = NULL;

static void make_spore_sprite() {
	if(spore_spr)
		return;
	const int S = 26;
	spore_spr = SDL_CreateRGBSurface(SDL_SWSURFACE, S, S, 16, 0xf800, 0x07e0, 0x001f, 0);
	Uint32 key = SDL_MapRGB(spore_spr->format, 255, 0, 255);
	SDL_FillRect(spore_spr, NULL, key);
	for(int y=0; y<S; y++) for(int x=0; x<S; x++) {
		float dx = x - S / 2.0f + 0.5f, dy = y - S / 2.0f + 0.5f, d = sqrtf(dx * dx + dy * dy);
		if(d > 11.5f) continue;
		// a round pod with a bright rim, a few pale spots and a tiny stalk-less glow: nothing like the red-capped mushroom
		float lit = 0.75f + 0.45f * (-dx * 0.5f - dy * 0.6f) / 12.0f;
		int r = (int)(40 * lit), g = (int)(190 * lit), b = (int)(170 * lit);
		if(d > 9.0f) { r = 20; g = 90; b = 110; }
		int hh = (x * 7 + y * 11) % 23;
		if(d < 8.0f && (hh == 0 || hh == 9)) { r = 230; g = 255; b = 240; }
		*(Uint16 *)((Uint8 *)spore_spr->pixels + y * spore_spr->pitch + x * 2) = (Uint16)SDL_MapRGB(spore_spr->format,
			r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
	}
	SDL_SetColorKey(spore_spr, SDL_SRCCOLORKEY, key);
	spore_glow = make_glow_rgb(80, 235, 205, 56, 130.0f);
}

static int phantom_timer = 0;
static bool phantoms_out = false;

bool enhanced_phantom_hidden(CRITTER *c) {
	// a flicker: gone for a few ticks now and then, out of step between phantoms
	return ((enhanced_ticks / 3 + (long)(c->maxhp & 7) * 5) % 11) == 0;
}

void enhanced_phantom_slain(float x, float y) {
	for(int k=0; k<12; k++)
		add_particle(x, y, RANDF(-2.5, 2.5), RANDF(-2.5, 2.5), 150, 80, 240, (RAND(10,24)));
	if(RANDF(0, 1) < 0.30f)
		spawn_heal_plant_at(x, y);
	else if(!spore_ok && enhanced_ticks >= spore_lock_until && RANDF(0, 1) < SPORE_CHANCE) {
		float sx = x, sy = y;
		enhanced_unstick(&sx, &sy);
		spore_ok = true;
		spore_x = sx;
		spore_y = sy;
		spore_life = SPORE_LIFE;
	}
}

static void phantom_update() {
	if(spore_ok) {
		if(--spore_life <= 0)
			spore_ok = false;
		else if(player.ok && distance(player.x, player.y, spore_x, spore_y) < 24) {
			spore_ok = false;
			// the same as the mushroom: full health, the screen-warping effect, bugs bite less
			player.drugs = (RAND(10, 15)) * 60;
			player.health = 200;
			prev_health = player.health;
			spore_lock_until = enhanced_ticks + SPORE_LOCKOUT;
			if(config.sound)
				Mix_PlayChannel(-1, snd_mushroom, 0);
			for(int k=0; k<24; k++)
				add_particle(player.x, player.y, RANDF(-2.5, 2.5), RANDF(-2.5, 2.5), 80, 235, 205, (RAND(18,40)));
			set_message("spores: the visions return");
		}
	}
	bool tripping = player.ok && player.drugs > 0;
	if(tripping) {
		if(--phantom_timer <= 0) {
			phantom_timer = 60 * (RAND(2, 4));
			int n = 0;
			for(CRITTER *c = critter_first; c; c = c->next)
				if(c->type == CT_PHANTOM && !c->remove)
					n++;
			if(n < (int)tune.phantom_max) {
				for(int tries=0; tries<40; tries++) {
					float a = RANDF(0, 6.2831853f), r = RANDF(210, 340);
					float x = player.x + cosf(a) * r, y = player.y + sinf(a) * r * 0.8f;
					if(world_blocked((int)x, (int)y, false) || world_blocked((int)x + 12, (int)y, false) || world_blocked((int)x - 12, (int)y, false))
						continue;
					CRITTER *c = add_critter(x, y, 0);
					critter_set_type(c, CT_PHANTOM);
					for(int k=0; k<8; k++)
						add_particle(x, y, RANDF(-1.8, 1.8), RANDF(-1.8, 1.8), 150, 80, 240, (RAND(10,22)));
					phantoms_out = true;
					break;
				}
			}
		}
	}
	else if(phantoms_out) {
		// the effect is over: they were never there
		phantoms_out = false;
		for(CRITTER *c = critter_first; c; c = c->next) {
			if(c->type == CT_PHANTOM && !c->remove) {
				c->remove = true;
				for(int k=0; k<10; k++)
					add_particle(c->x, c->y, RANDF(-1.5, 1.5), RANDF(-1.5, 1.5), 150, 80, 240, (RAND(10,24)));
			}
		}
		phantom_timer = 60;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Special ammunition, found in the arms: about double damage against one rank, a little less against the rest

struct AMMO_BOX {
	float x, y;
	int type;
	int life;
	bool ok;
};
#define MAX_AMMO_BOXES 4
static AMMO_BOX abox[MAX_AMMO_BOXES];
static int ammo_timer = 0;
static SDL_Surface *ammo_spr[NUM_AMMO];

float enhanced_ammo_mult(int rank) {
	float boon = enhanced_ticks < boon_until ? tune.boon_damage_mult : 1.0f;			// the Tyrant's spoils: all hits count more
	if(ammo_type < 0 || ammo_left <= 0)
		return boon;
	return boon * ((rank == ammo_type) ? 1.0f + (AMMO_BONUS[ammo_type] - 1.0f) * tune.ammo_bonus_scale : 0.9f);
}

void enhanced_ammo_fired() {
	if(ammo_type < 0 || ammo_left <= 0)
		return;
	ammo_recent[ammo_type] += 1.0f;
	if(--ammo_left <= 0) {
		char buf[64];
		snprintf(buf, sizeof(buf), "out of %s rounds", AMMO_NAME[ammo_type]);
		set_message(buf);
		ammo_type = -1;
	}
}

void enhanced_ammo_hit(float x, float y) {
	if(ammo_type < 0)
		return;
	for(int k=0; k<3; k++)
		add_particle(x, y, RANDF(-1.8, 1.8), RANDF(-1.8, 1.8), AMMO_COL[ammo_type][0], AMMO_COL[ammo_type][1], AMMO_COL[ammo_type][2], (RAND(6,12)));
}

static SDL_Surface *make_ammo_sprite(int t) {
	// special rounds: an upright cartridge, brass casing and a tip in the colour of the rank it is made against
	const int W_ = 18, H_ = 30;
	SDL_Surface *sp = SDL_CreateRGBSurface(SDL_SWSURFACE, W_, H_, 16, 0xf800, 0x07e0, 0x001f, 0);
	Uint32 key = SDL_MapRGB(sp->format, 255, 0, 255);
	SDL_FillRect(sp, NULL, key);
	for(int y=0; y<H_; y++) {
		for(int x=0; x<W_; x++) {
			float dx = x - W_ / 2.0f + 0.5f;
			int r = -1, g = 0, b = 0;
			float lit = 0.8f + 0.35f * (-dx / 7.0f);
			if(y >= 13 && y < 28 && fabsf(dx) < 6.5f) {				// the casing
				r = (int)(206 * lit); g = (int)(160 * lit); b = (int)(64 * lit);
				if(y >= 24) { r = (int)(150 * lit); g = (int)(116 * lit); b = (int)(46 * lit); }		// the rim at the base
				if(y >= 13 && y < 17) { r = (int)(AMMO_COL[t][0] * lit); g = (int)(AMMO_COL[t][1] * lit); b = (int)(AMMO_COL[t][2] * lit); }
			}
			else if(y >= 2 && y < 13) {								// the bullet, pointed
				float w = 6.0f * sqrtf((y - 1.5f) / 11.5f);
				if(fabsf(dx) < w) { r = (int)(AMMO_COL[t][0] * lit * 1.1f); g = (int)(AMMO_COL[t][1] * lit * 1.1f); b = (int)(AMMO_COL[t][2] * lit * 1.1f); }
			}
			if(r >= 0) {
				bool edge = (fabsf(dx) > 5.2f && y >= 13) || y == 28 || y == 2;
				if(edge) { r /= 2; g /= 2; b /= 2; }
				*(Uint16 *)((Uint8 *)sp->pixels + y * sp->pitch + x * 2) = (Uint16)SDL_MapRGB(sp->format, r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
			}
		}
	}
	SDL_SetColorKey(sp, SDL_SRCCOLORKEY, key);
	return sp;
}

static void spawn_ammo_box(int here) {
	int slot = -1;
	for(int i=0; i<MAX_AMMO_BOXES; i++) {
		if(!abox[i].ok) { slot = i; break; }
	}
	if(slot < 0)
		return;
	for(int tries=0; tries<50; tries++) {
		int lx = (RAND(100, CELL_W - 100)), ly = (RAND(100, CELL_H - 100));
		int x = lx + (CELL_GX[here] - gx) * CELL_W, y = ly + (CELL_GY[here] - gy) * CELL_H;
		if(world_blocked(x, y, false) || world_blocked(x + 16, y, false) || world_blocked(x - 16, y, false) ||
		   world_blocked(x, y + 16, false) || world_blocked(x, y - 16, false))
			continue;
		if(distance((float)x, (float)y, player.x, player.y) < 140)
			continue;
		float cx = pcell[here].lx + (CELL_GX[here] - gx) * CELL_W, cy = pcell[here].ly + (CELL_GY[here] - gy) * CELL_H;
		if(distance((float)x, (float)y, cx, cy) < 90)
			continue;
		int t;
		do { t = (RAND(0, NUM_AMMO - 1)); } while(t == ammo_type);
		abox[slot].x = (float)x;
		abox[slot].y = (float)y;
		abox[slot].type = t;
		abox[slot].life = 60 * 50;
		abox[slot].ok = true;
		return;
	}
}

static void ammo_update(int here) {
	if(ammo_type >= 0 && enhanced_ticks >= ammo_until) {
		char buf[64];
		snprintf(buf, sizeof(buf), "your %s rounds are spent", AMMO_NAME[ammo_type]);
		set_message(buf);
		ammo_type = -1;
		ammo_left = 0;
	}
	if(here > 0 && player.ok) {
		if(--ammo_timer <= 0) {
			ammo_timer = (int)(60 * RANDF(tune.ammo_min_seconds, tune.ammo_max_seconds));			// infrequent, and only while out in an arm
			bool already = false;
			for(int i=0; i<MAX_AMMO_BOXES; i++) {
				if(abox[i].ok && cell_index(gx + fdiv((int)abox[i].x, CELL_W), gy + fdiv((int)abox[i].y, CELL_H)) == here)
					already = true;
			}
			if(!already)
				spawn_ammo_box(here);
		}
	}
	for(int i=0; i<MAX_AMMO_BOXES; i++) {
		if(!abox[i].ok)
			continue;
		if(--abox[i].life <= 0) {
			abox[i].ok = false;
			continue;
		}
		if(player.ok && distance(player.x, player.y, abox[i].x, abox[i].y) < 24) {
			ammo_type = abox[i].type;
			ammo_left = AMMO_ROUNDS;
			ammo_until = enhanced_ticks + 60 * AMMO_SECONDS;
			abox[i].ok = false;
			char buf[64];
			snprintf(buf, sizeof(buf), "%s rounds: strong against %s", AMMO_NAME[ammo_type], RANK_NAME[ammo_type]);
			set_message(buf);
			if(config.sound)
				Mix_PlayChannel(-1, snd_power_up, 0);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// Screen changes

void enhanced_shift(float dx, float dy) {
	player.x += dx; player.y += dy;
	player.sx += dx; player.sy += dy;
	for(int i=0; i<MAX_AMMO_BOXES; i++) {
		abox[i].x += dx;
		abox[i].y += dy;
	}
	kit.x += dx; kit.y += dy;
	flame_pu.x += dx; flame_pu.y += dy;
	bunker.x += dx; bunker.y += dy;
	trap_shift(dx, dy);
	spore_x += dx; spore_y += dy;
	for(int i=0; i<MAX_ACID; i++) { acid[i].x += dx; acid[i].y += dy; }
	ty_x += dx; ty_y += dy;
	shift_critters(dx, dy);
	shift_gibs(dx, dy);
	shift_bombs(dx, dy);
	shift_power_ups(dx, dy);
	shift_particles(dx, dy);
	shift_explosions(dx, dy);
}

static void switch_cell(int ngx, int ngy) {
	int dx = ngx - gx, dy = ngy - gy;
	enhanced_shift((float)(-dx * CELL_W), (float)(-dy * CELL_H));
	// a few pixels inside, so that stepping back and forth on the line does not flicker
	if(dx > 0) { player.x += 12; player.sx += 12; }
	if(dx < 0) { player.x -= 12; player.sx -= 12; }
	if(dy > 0) { player.y += 12; player.sy += 12; }
	if(dy < 0) { player.y -= 12; player.sy -= 12; }
	gx = ngx;
	gy = ngy;
	int i = cell_index(gx, gy);
	cur_bground = scr[i].cur;
	orig_bground = scr[i].orig;

	// the guardians stand where the arm's cell is
	if(i > 0 && need_guards[i])
		spawn_cell_guards(i);

	// the first visit in a long while: the arm has something lying around
	if(i > 0 && enhanced_ticks - arm_last_loot[i] > 60 * 40) {
		arm_last_loot[i] = enhanced_ticks;
		arm_timer[i] = 60 * 3;
	}
}


///////////////////////////////////////////////////////////////////////////////
// Per tick

static int loot_type() {
	int r = (RAND(0,99));
	if(r < 30) return POWER_UP_HEALTH;
	if(r < 58) return POWER_UP_BOMB;
	if(r < 86) return POWER_UP_MINE;
	return POWER_UP_MUSHROOM;
}

static void spawn_arm_loot(int ci) {
	for(int tries=0; tries<40; tries++) {
		int lx = (RAND(90, CELL_W - 90)), ly = (RAND(90, CELL_H - 90));
		int x = lx + (CELL_GX[ci] - gx) * CELL_W;
		int y = ly + (CELL_GY[ci] - gy) * CELL_H;
		if(world_blocked(x, y, false) || world_blocked(x + 14, y + 14, false) || world_blocked(x - 14, y - 14, false))
			continue;
		create_power_up_at(x, y, loot_type(), 60 * 25);
		return;
	}
}

void enhanced_update() {
	enhanced_ticks++;
#ifdef FORMIDO_TESTHOOKS
	if(getenv("FORMIDO_LEGEND") && enhanced_ticks == 120)
		enhanced_show_legend();			// test builds: legend.bmp is saved when it opens
#endif
#ifdef FORMIDO_TESTHOOKS
	if(player.ok) {
		bot_alive_ticks++;
		for(CRITTER *c = critter_first; c; c = c->next) {
			if(c->remove || c->hidden || c->guard || c->type == CT_BURROWER || c->type == CT_STUNNER || c->type == CT_TYRANT || enhanced_bug_mode(c))
				continue;
			float dx = c->x - player.x, dy = c->y - player.y;
			if(dx * dx + dy * dy < 150.0f * 150.0f)
				continue;
			bot_far_ticks++;
			if(fabsf(c->x - c->sx) + fabsf(c->y - c->sy) < 0.4f)
				bot_stall_ticks++;
		}
		int drop = bot_prev_hp - player.health;
		if(drop > 0) {
			bot_dmg_total += drop;
			bot_dmg_ticks++;
			bot_hits[drop > 7 ? 7 : drop]++;
		}
		if(player.health < 100 && bot_first_hurt < 0)
			bot_first_hurt = enhanced_ticks;
	}
	bot_prev_hp = player.health;
	bot_minute_log();
	if(getenv("FORMIDO_BOT") && enhanced_ticks > 60L * 60L * 120L) {		// a run that survives two hours is cut off
		extern bool main_loop_done;
		main_loop_done = true;
	}
#endif
#ifdef FORMIDO_TESTHOOKS
	if(getenv("FORMIDO_HPLOG") && enhanced_ticks % (atoi(getenv("FORMIDO_HPLOG")) > 1 ? atoi(getenv("FORMIDO_HPLOG")) : 60) == 0)
		fprintf(stderr, "tick %ld hp %d at %d,%d blocked %d thorn %d cell %d,%d\n", enhanced_ticks, player.health, (int)player.x, (int)player.y, (int)world_blocked((int)player.x, (int)player.y, false), (int)world_thorns((int)player.x, (int)player.y), gx, gy);
	if(getenv("FORMIDO_HPLOG") && enhanced_ticks % (atoi(getenv("FORMIDO_HPLOG")) > 1 ? atoi(getenv("FORMIDO_HPLOG")) : 60) == 0) {
		int n = 0;
		for(CRITTER *c = critter_first; c; c = c->next)
			if(c->type == CT_PHANTOM && !c->remove)
				n++;
		fprintf(stderr, "phantoms %d drugs %d stun %d\n", n, player.drugs, player_stun);
	}
#endif

#ifdef FORMIDO_TESTHOOKS
	if(getenv("FORMIDO_GOD") && player.ok && player.health < 150)		// test: no dying while watching other things
		player.health = 150;
#endif

	// 0. never stay inside rock: a shove from the swarm, a blast or a burst can leave the player where no
	// direction is free
	if(player.ok && enhanced_unstick(&player.x, &player.y)) {
		player.sx = player.x;
		player.sy = player.y;
		player.dirx = player.diry = 0;
	}

	// 1. the player walks into the next screen
	int pcx = gx + fdiv((int)player.x, CELL_W);
	int pcy = gy + fdiv((int)player.y, CELL_H);
	if((pcx != gx || pcy != gy) && cell_index(pcx, pcy) >= 0)
		switch_cell(pcx, pcy);

	// 2. a little health back on the stone platform, when nothing has bitten for a while
	if(player.health < prev_health)
		last_damage_tick = enhanced_ticks;
	prev_health = player.health;
	if(player.ok && gx == 0 && gy == 0) {
		float dx = (player.x - CELL_W / 2) / 100.0f, dy = (player.y - CELL_H / 2) / 112.0f;
#ifdef FORMIDO_TESTHOOKS
		if(dx * dx + dy * dy < 1.0f)
			bot_platform_ticks++;
#endif
		bool medbay = mode_on() && mode == 3;
		if(dx * dx + dy * dy < 1.0f && player.health < 200 && enhanced_ticks - last_damage_tick > (long)(medbay ? tune.medbay_regen_delay_ticks : tune.platform_regen_delay_ticks)) {
			if(++regen_acc >= (int)(medbay ? tune.medbay_regen_ticks : tune.platform_regen_ticks)) {
				regen_acc = 0;
				player.health++;
#ifdef FORMIDO_TESTHOOKS
				bot_healed++;
#endif
				prev_health = player.health;
			}
			if((RAND(0,19)) == 0)
				add_particle(player.x + RANDF(-12, 12), player.y + RANDF(-4, 8), RANDF(-0.2, 0.2), RANDF(-0.8, -0.3), 80, 255, 120, (RAND(25,50)));
		}
	}

	// 2a. thorns: a scratch on touching them, and one now and then while standing in them
	if(player.ok && !enhanced_in_bunker()) {
		int px = (int)player.x, py = (int)player.y;
		bool in = world_thorns(px, py) || world_thorns(px + 9, py) || world_thorns(px - 9, py) ||
			world_thorns(px, py + 9) || world_thorns(px, py - 9);
		if(in) {
			if((thorn_out >= 30 || enhanced_ticks - thorn_hit >= 25) && player.health > 2) {
				player.health -= 1;
				thorn_hit = enhanced_ticks;
				for(int k=0; k<3; k++)
					add_particle(player.x + RANDF(-8, 8), player.y + RANDF(-6, 6), RANDF(-0.6, 0.6), RANDF(-0.9, 0.1), 190, 40, 40, (RAND(8,16)));
			}
			thorn_out = 0;
		}
		else if(thorn_out < 1000)
			thorn_out++;
	}

	// 2b. power cells and what the pylons do
	cells_update(cell_index(gx, gy));
	ammo_update(cell_index(gx, gy));
	burrow_update(cell_index(gx, gy));
	heal_plant_update(cell_index(gx, gy));
	bunker_update();
	flame_update();
	tracers_update();
	trap_update(cell_index(gx, gy));
	tyrant_update();
	phantom_update();
	player_status_tick();
	if(player_stun > 0) {
		player_stun--;
		if(player.ok) {
			player.dirx = player.diry = 0;
			if(RAND(0,2) == 0)
				add_particle(player.x + RANDF(-16, 16), player.y + RANDF(-16, 12), RANDF(-1.2, 1.2), RANDF(-1.2, 0.4), 255, 240, (RAND(90,150)), (RAND(6,12)));
		}
	}
	if(player.ok && player.health <= 0)
		player.die();

	// 3. the hive watches
	if(enhanced_ticks % 60 == 0)
		director_sample();
	if(msg_timer > 0)
		msg_timer--;

	// 4. loot in the arms
	int here = cell_index(gx, gy);
	if(here > 0 && player.ok) {
		if(arm_timer[here] > 0) {
			if(--arm_timer[here] == 0) {
				int near_items = 0;
				for(int i=0; i<MAX_POWER_UPS; i++)
					if(power_up[i].ok)
						near_items++;
				if(near_items < 5)
					spawn_arm_loot(here);
				arm_timer[here] = 60 * 7;
			}
		}
	}
}

void enhanced_begin() {
	tune_load("formido_tuning.cfg");		// the tuning file in the port folder: read at the start of every enhanced match
#ifdef FORMIDO_TESTHOOKS
	if(getenv("FORMIDO_SEED"))
		srand((unsigned)atoi(getenv("FORMIDO_SEED")));
	if(getenv("FORMIDO_BOT"))
		bot_in_match = true;
	bot_peak_bugs = 0;
	bot_healed = 0;
	bot_platform_ticks = 0;
	bot_dmg_total = bot_dmg_ticks = bot_alive_ticks = 0;
	bot_far_ticks = bot_stall_ticks = 0;
	bot_first_hurt = -1;
	bot_prev_hp = 200;
	for(int i=0; i<8; i++)
		bot_hits[i] = 0;
	bot_tyrants_seen = 0;
	bot_last_tick = -1;
#endif
	if(!world_loaded)
		load_world();
	make_sprites();
	for(int i=0; i<5; i++)
		SDL_BlitSurface(scr[i].orig, NULL, scr[i].cur, NULL);
	gx = gy = 0;
	cur_bground = scr[0].cur;
	orig_bground = scr[0].orig;

	enhanced_ticks = 0;
	nav_stamp = -1000;
	nav_src = -1;
#ifdef FORMIDO_TESTHOOKS
	// test builds: FORMIDO_T0=<seconds> starts the hive's clock later, so every rank shows up at once
	if(getenv("FORMIDO_T0"))
		enhanced_ticks = (long)atoi(getenv("FORMIDO_T0")) * 60;
#endif
	last_damage_tick = 0;
	prev_health = 200;
	regen_acc = 0;
	hist_n = 0;
	throws_recent = 0;
	adapt_camp = adapt_kite = adapt_bombs = adapt_low = 0;
	dominant = 0;
	msg[0] = '\0';
	msg_timer = 0;
	for(int i=0; i<5; i++) {
		arm_last_loot[i] = -100000;
		arm_timer[i] = 0;
	}
	make_cell_sprites();
	for(int i=0; i<NUM_AMMO; i++) {
		if(!ammo_spr[i])
			ammo_spr[i] = make_ammo_sprite(i);
		ammo_recent[i] = 0;
		ammo_counter[i] = 0;
		ammo_counter_on[i] = false;
	}
	for(int i=0; i<MAX_AMMO_BOXES; i++)
		abox[i].ok = false;
	ammo_type = -1;
	ammo_left = 0;
	ammo_timer = (int)(60 * RANDF(tune.ammo_min_seconds, tune.ammo_max_seconds));
	burrow_timer = 60 * (RAND(40, 80));
	rumble_messages = 0;
	if(!hplant_img) {
		SDL_Surface *img = IMG_Load(DAT("plant_green.dat"));
		hplant_img = img;			// without the picture the plants simply do not appear
	}
	for(int i=0; i<MAX_HEAL_PLANTS; i++)
		hplant[i].ok = false;
	hplant_timer = 60 * (RAND(70, 130));
	hplant_ready = false;
	hplant_messages = 0;
	thorn_out = 100;
	thorn_hit = 0;
	make_bunker_sprites();
	flame_make_sprites();
	make_trap_sprites();
	make_badge_sprites();
	for(int i=0; i<MAX_TRAPS; i++)
		traps[i].ok = false;
	for(int i=0; i<MAX_TBOXES; i++)
		tbox[i].ok = false;
	for(int i=0; i<NUM_CTYPES; i++)
		stat_kills[i] = 0;
	stat_cells = 0;
	stat_tyrants = 0;
	player_stun = 0;
	stun_immune_until = 0;
	phantom_timer = 60;
	phantoms_out = false;
	spore_ok = false;
	spore_lock_until = 0;
	make_spore_sprite();
	player_burn = player_bleed = player_slow = 0;
	ty_phase = TY_WAITING;
	ty_next = (long)(3600 * tune.tyrant_first_minutes);
	ty_cycle = 0;
	hive_level = 0;
	boon_until = 0;
	ty_alive = false;
	ty_missing = 0;
	charge_state = 0;
	stomp_t = 0;
	for(int i=0; i<MAX_ACID; i++)
		acid[i].ok = false;
	trap_loaded = -1;
	trap_charges = 0;
	trap_cool = 0;
	t_prev = false;
	tbox_timer = 60 * (RAND(60, 100));
	kit.ok = false;
	kit_carried = false;
	flame_reset();
	for(int i=0; i<MAX_TRACERS; i++)
		tracers[i].ok = false;
	tracer_shots = 0;
	bunker.ok = false;
	bunker.inside = false;
	r_prev = false;
	for(int i=0; i<5; i++) {
		need_guards[i] = (i > 0);
		sealed[i] = (i > 0);
		alert[i] = false;
		garrison[i] = 1.0f;
		if(i > 0)
			roll_garrison(i);
	}
	carrying = 0;
	mode = 0;
	mode_until = 0;
	place_cells();
#ifdef FORMIDO_TESTHOOKS
	for(int i=1; i<5; i++)
		fprintf(stderr, "power cell %s at %d,%d\n", DIR_NAME[i], (int)pcell[i].lx, (int)pcell[i].ly);
	fprintf(stderr, "timers (ticks): ammo %d, burrower %d, healing plant %d\n", ammo_timer, burrow_timer, hplant_timer);
	if(getenv("FORMIDO_NOGUARD"))		// no guardians (to watch other things in an arm)
		for(int i=1; i<5; i++) {
			need_guards[i] = false;
			sealed[i] = false;
		}
	if(getenv("FORMIDO_ONTHORN") && getenv("FORMIDO_WARP")) {		// stand in the arm's first thorn patch
		int w = atoi(getenv("FORMIDO_WARP"));
		for(int k=0; k<CELL_W*CELL_H; k++) {
			if(w >= 1 && w <= 4 && scr[w].col[k] == 3) {
				player.x = (float)(k % CELL_W + (CELL_GX[w] - gx) * CELL_W);
				player.y = (float)(k / CELL_W + (CELL_GY[w] - gy) * CELL_H);
				player.sx = player.x;
				player.sy = player.y;
				break;
			}
		}
	}
	if(getenv("FORMIDO_HEAL"))			// a healing plant soon
		hplant_timer = 1;
	if(getenv("FORMIDO_HURT"))			// start hurt
		player.health = atoi(getenv("FORMIDO_HURT"));
	if(getenv("FORMIDO_SPORE")) {		// a spore pod lies beside the player
		spore_ok = true;
		spore_x = player.x + 50;
		spore_y = player.y + 20;
		spore_life = SPORE_LIFE;
	}
	if(getenv("FORMIDO_STUNNER"))		// stunning burrowers regardless of the Tyrant rule
		force_stunner = true;
	if(getenv("FORMIDO_STUNNOW"))
		test_stunnow = true;
	if(getenv("FORMIDO_TYRANTS"))		// pretend this many Tyrants were slain
		stat_tyrants = atoi(getenv("FORMIDO_TYRANTS"));
	if(getenv("FORMIDO_MUSH"))			// start under the mushroom's effect
		player.drugs = atoi(getenv("FORMIDO_MUSH")) * 60;
	if(getenv("FORMIDO_TYRANT"))		// the Tyrant stirs soon (4 s of warning)
		ty_next = enhanced_ticks + 60 * 2;
	if(getenv("FORMIDO_TRAP")) {		// start with traps: 0 napalm, 1 spikes
		trap_loaded = atoi(getenv("FORMIDO_TRAP")) % NUM_TRAPS;
		trap_charges = 6;
	}
	if(getenv("FORMIDO_SHOWITEMS")) {	// lay every kind of pickup out on the centre screen
		int y0 = 70;
		for(int i=0; i<4; i++)
			create_power_up_at(70 + i * 70, y0, i, 60 * 600);									// the classic ones
		for(int i=0; i<4; i++) {
			abox[i].ok = true; abox[i].x = 70.0f + i * 70; abox[i].y = (float)(y0 + 70); abox[i].type = i; abox[i].life = 60 * 600;
		}
		for(int i=0; i<NUM_TRAPS; i++) {
			tbox[i].ok = true; tbox[i].x = 70.0f + i * 70; tbox[i].y = (float)(y0 + 140); tbox[i].type = i; tbox[i].life = 60 * 600;
		}
		kit.ok = true; kit.x = 280; kit.y = (float)(y0 + 140); kit.life = 60 * 600;
		hplant[0].ok = true; hplant[0].x = 70; hplant[0].y = (float)(y0 + 235); hplant[0].pool = 40; hplant[0].life = 60 * 600;
		hplant[1].ok = true; hplant[1].x = 140; hplant[1].y = (float)(y0 + 235); hplant[1].pool = 14; hplant[1].life = 60 * 600;
		traps[0].ok = true; traps[0].type = TRAP_SPIKES; traps[0].x = 400; traps[0].y = 120; traps[0].integrity = 320; traps[0].life = 60 * 600;
		traps[1].ok = true; traps[1].type = TRAP_GLUE; traps[1].x = 520; traps[1].y = 130; traps[1].integrity = 380; traps[1].life = 60 * 600;
		traps[2].ok = true; traps[2].type = TRAP_GLUE; traps[2].x = 520; traps[2].y = 250; traps[2].integrity = 150; traps[2].life = 60 * 600;
		traps[3].ok = true; traps[3].type = TRAP_NAPALM; traps[3].x = 400; traps[3].y = 250; traps[3].integrity = 420; traps[3].life = 60 * 600;
	}
	if(getenv("FORMIDO_FLAME")) {		// start with a flamethrower, and a canister lying ahead
		flame_fuel = (int)(tune.flame_fuel_seconds * 60);
		flame_hold_until = enhanced_ticks + (long)(tune.flame_hold_seconds * 60);
	}
	if(getenv("FORMIDO_FLAMEDROP")) {
		flame_pu.ok = true; flame_pu.x = player.x + 70; flame_pu.y = player.y; flame_pu.life = 60 * 600;
	}
	if(getenv("FORMIDO_KIT"))			// start carrying a bunker kit
		kit_carried = true;
	if(getenv("FORMIDO_BURROW"))		// a burrower soon, in whichever arm the player is in
		burrow_timer = 60;
	if(getenv("FORMIDO_AMMO")) {		// start with special rounds, and have a crate drop soon
		ammo_type = atoi(getenv("FORMIDO_AMMO")) % NUM_AMMO;
		ammo_left = 999;
		ammo_until = enhanced_ticks + 60 * AMMO_SECONDS;
		ammo_timer = 90;
	}
	if(getenv("FORMIDO_MODE")) {		// start with a pylon mode on
		mode = atoi(getenv("FORMIDO_MODE"));
		mode_until = enhanced_ticks + 60 * 600;
	}
	if(getenv("FORMIDO_WARP")) {		// start next to an arm's power cell
		int w = atoi(getenv("FORMIDO_WARP"));
		if(w >= 1 && w <= 4) {
			switch_cell(CELL_GX[w], CELL_GY[w]);
			player.x = pcell[w].lx + 60;
			player.y = pcell[w].ly;
			player.sx = player.x;
			player.sy = player.y;
		}
	}
	if(getenv("FORMIDO_STUCK") && getenv("FORMIDO_WARP")) {			// start embedded in the arm's rock
		int w = atoi(getenv("FORMIDO_WARP"));
		if(w >= 1 && w <= 4 && (gx != CELL_GX[w] || gy != CELL_GY[w]))
			switch_cell(CELL_GX[w], CELL_GY[w]);
		int best = -1;
		for(int k=0; k<CELL_W*CELL_H && w >= 1 && w <= 4; k++) {
			int x = k % CELL_W, y = k / CELL_W;
			if(x > 200 && x < 440 && y > 200 && y < 280 && scr[w].col[k] == 1 &&
			   scr[w].col[k - 6] == 1 && scr[w].col[k + 6] == 1 && scr[w].col[k - 6 * CELL_W] == 1 && scr[w].col[k + 6 * CELL_W] == 1) {
				best = k;
				break;
			}
		}
		if(best >= 0) {
			player.x = (float)(best % CELL_W);
			player.y = (float)(best / CELL_W);
			player.sx = player.x;
			player.sy = player.y;
			fprintf(stderr, "embedded at %d,%d\n", (int)player.x, (int)player.y);
		}
	}
#endif
}

void enhanced_end() {
	if(center_cur) {
		cur_bground = center_cur;
		orig_bground = center_orig;
	}
	gx = gy = 0;
}


///////////////////////////////////////////////////////////////////////////////
// HUD

static void rect(SDL_Surface *bmp, int x, int y, int w, int h, int r, int g, int b) {
	SDL_Rect rc;
	rc.x = x; rc.y = y; rc.w = w; rc.h = h;
	SDL_FillRect(bmp, &rc, SDL_MapRGB(bmp->format, r, g, b));
}

static void blit_center(SDL_Surface *bmp, SDL_Surface *sp, int x, int y) {
	SDL_Rect r;
	r.x = x - sp->w / 2;
	r.y = y - sp->h / 2;
	r.w = sp->w;
	r.h = sp->h;
	SDL_BlitSurface(sp, NULL, bmp, &r);
}

static void draw_world_overlays(SDL_Surface *bmp) {
	int here = cell_index(gx, gy);

	// the power cell lying in this arm
	if(here > 0 && pcell[here].present) {
		int x = (int)(pcell[here].lx + (CELL_GX[here] - gx) * CELL_W);
		int y = (int)(pcell[here].ly + (CELL_GY[here] - gy) * CELL_H) + (int)(sinf(enhanced_ticks * 0.07f) * 2.5f);
		blit_center(bmp, glow_spr[here], x, y);
		blit_center(bmp, cell_spr[here], x, y);
		if(sealed[here]) {			// a ring of light: the guardians have to be beaten first
			for(int k=0; k<24; k++) {
				float a = k * 6.2831853f / 24 + enhanced_ticks * 0.03f;
				rect(bmp, x + (int)(cosf(a) * 30), y + (int)(sinf(a) * 30), 2, 2,
					ARM_COL[here][0], ARM_COL[here][1], ARM_COL[here][2]);
			}
		}
	}

	// the active pylon glows in its cell's colour, the field shows as a ring around the platform
	if(gx == 0 && gy == 0 && mode_on()) {
		blit_center(bmp, glow_spr[mode], PYL_X[mode], PYL_Y[mode]);
		if(mode == 4) {
			for(int k=0; k<56; k++) {
				float a = k * 6.2831853f / 56 + enhanced_ticks * 0.01f;
				int x = (int)(CELL_W / 2 + cosf(a) * 210), y = (int)(CELL_H / 2 + sinf(a) * 210 * 0.9f);
				rect(bmp, x, y, 2, 2, ARM_COL[4][0], ARM_COL[4][1], ARM_COL[4][2]);
			}
		}
	}

	// healing plants: green and bright while there is plenty left, then yellowing, browning, withered
	for(int i=0; i<MAX_HEAL_PLANTS; i++) {
		if(!hplant[i].ok || !hplant_img)
			continue;
		float frac = (float)hplant[i].pool / HEAL_POOL;
		float lf = (float)hplant[i].life / HEAL_LIFE;
		if(lf < frac)
			frac = lf;
		int frame = frac > 0.66f ? 0 : (frac > 0.33f ? 1 : (frac > 0.12f ? 2 : 3));
		if(frame <= 1)
			blit_center(bmp, glow_spr[5], (int)hplant[i].x, (int)hplant[i].y - 14);
		SDL_Rect src, dst;
		src.x = frame * 80; src.y = 0; src.w = 80; src.h = 80;
		dst.x = (int)hplant[i].x - 40; dst.y = (int)hplant[i].y - 62; dst.w = 80; dst.h = 80;
		SDL_BlitSurface(hplant_img, &src, bmp, &dst);
	}

	// a spore pod
	if(spore_ok && spore_spr && (spore_life > 180 || (spore_life / 7) % 2 == 0)) {
		blit_center(bmp, spore_glow, (int)spore_x, (int)spore_y);
		blit_center(bmp, spore_spr, (int)spore_x, (int)(spore_y + sinf(enhanced_ticks * 0.1f) * 2.0f));
	}

	// special ammunition lying about
	for(int i=0; i<MAX_AMMO_BOXES; i++) {
		if(abox[i].ok && (abox[i].life > 180 || (abox[i].life / 6) % 2 == 0))
			{
				blit_center(bmp, glow_item[abox[i].type], (int)abox[i].x, (int)abox[i].y);
				blit_center(bmp, ammo_spr[abox[i].type], (int)abox[i].x, (int)abox[i].y);
			}
	}

	bunker_draw_over(bmp);
	flame_draw_over(bmp);
	tracers_draw(bmp);

	// badges over the player
	enhanced_draw_badges(bmp, (int)player.x, (int)player.y - 36, player_burn > 0, player_bleed > 0, player_slow > 0, player_stun > 0);

	tyrant_draw_over(bmp);

	// the cell the player carries
	if(carrying)
		blit_center(bmp, cell_spr[carrying], (int)player.x, (int)player.y - 40);
}

void enhanced_draw(SDL_Surface *bmp) {
	draw_world_overlays(bmp);
	// the map of the cross: the screen we are on is light, bugs tint a screen red
	int cnt[5] = { 0, 0, 0, 0, 0 };
	for(CRITTER *c = critter_first; c; c = c->next) {
		int i = cell_index(gx + fdiv((int)c->x, CELL_W), gy + fdiv((int)c->y, CELL_H));
		if(i >= 0 && !c->remove && !c->guard && !c->hidden)
			cnt[i]++;
	}
	const int CW = 18, CH = 14, GAP = 2;
	int ox = SCREEN_W - 3 * CW - 2 * GAP - 8;
	int oy = SCREEN_H - 3 * CH - 2 * GAP - 8;
	int here = cell_index(gx, gy);
	for(int i=0; i<5; i++) {
		int cx = ox + (CELL_GX[i] + 1) * (CW + GAP);
		int cy = oy + (CELL_GY[i] + 1) * (CH + GAP);
		int red = cnt[i] > 12 ? 12 : cnt[i];
		rect(bmp, cx - 1, cy - 1, CW + 2, CH + 2, 12, 10, 8);
		if(i == here)
			rect(bmp, cx, cy, CW, CH, 150 + red * 6, 200 - red * 8, 120 - red * 6);
		else
			rect(bmp, cx, cy, CW, CH, 56 + red * 14, 50 - red * 2, 40 - red * 2);
	}
	// the arms' power cells: a coloured dot, hollow while the guardians still hold it
	for(int i=1; i<5; i++) {
		if(!pcell[i].present)
			continue;
		int cx = ox + (CELL_GX[i] + 1) * (CW + GAP) + CW / 2 - 2;
		int cy = oy + (CELL_GY[i] + 1) * (CH + GAP) + CH / 2 - 2;
		rect(bmp, cx, cy, 4, 4, ARM_COL[i][0], ARM_COL[i][1], ARM_COL[i][2]);
		if(sealed[i]) {
			rect(bmp, cx + 1, cy + 1, 2, 2, 20, 16, 14);
			// how heavy the guard is: one to three pips under the dot
			int lv = garrison_level(i);
			for(int k=0; k<lv; k++)
				rect(bmp, cx - (lv - 1) + k * 3, cy + 6, 2, 2, lv == 3 ? 255 : 230, lv == 3 ? 90 : 230, lv == 3 ? 70 : 220);
		}
	}
	// the player
	{
		int cx = ox + (CELL_GX[here] + 1) * (CW + GAP);
		int cy = oy + (CELL_GY[here] + 1) * (CH + GAP);
		int px = (int)(player.x * CW / CELL_W), py = (int)(player.y * CH / CELL_H);
		if(px < 1) px = 1;
		if(px > CW - 3) px = CW - 3;
		if(py < 1) py = 1;
		if(py > CH - 3) py = CH - 3;
		rect(bmp, cx + px, cy + py, 2, 2, 20, 30, 20);
	}

	// what the pylons are doing, and the cell on the way
	{
		char line[80] = "";
		if(carrying)
			snprintf(line, sizeof(line), "carrying the %s cell", DIR_NAME[carrying]);
		else if(mode_on()) {
			int secs = (int)((mode_until - enhanced_ticks) / 60);
			snprintf(line, sizeof(line), "pylons: %s  %d:%02d", MODE_NAME[mode], secs / 60, secs % 60);
		}
		if(line[0]) {
			int c = carrying ? carrying : mode;
			rect(bmp, 8, SCREEN_H - 50, 7, 7, ARM_COL[c][0], ARM_COL[c][1], ARM_COL[c][2]);
			font_write(bmp, FONT_NORMAL, 20, SCREEN_H - 55, line);
		}
	}

	// burning and bleeding, right of the health bar
	{
		int x = 118;
		if(player_burn > 0) {
			rect(bmp, x, SCREEN_H - 20, 7, 7, 255, 140, 40);
			font_write(bmp, FONT_NORMAL, x + 10, SCREEN_H - 27, (char *)"burning");
			x += 104;
		}
		if(player_bleed > 0) {
			rect(bmp, x, SCREEN_H - 20, 7, 7, 200, 30, 30);
			font_write(bmp, FONT_NORMAL, x + 10, SCREEN_H - 27, (char *)"bleeding");
			x += 116;
		}
		if(player_slow > 0) {
			rect(bmp, x, SCREEN_H - 20, 7, 7, 110, 200, 255);
			font_write(bmp, FONT_NORMAL, x + 10, SCREEN_H - 27, (char *)"slowed");
			x += 96;
		}
		if(player_stun > 0) {
			rect(bmp, x, SCREEN_H - 20, 7, 7, 255, 235, 60);
			font_write(bmp, FONT_NORMAL, x + 10, SCREEN_H - 27, (char *)"stunned");
		}
	}

	// traps and the bunker kit
	if(trap_loaded >= 0) {
		char line[64];
		snprintf(line, sizeof(line), "traps: %s x%d  (R2)", TRAP_NAME[trap_loaded], trap_charges);
		rect(bmp, 8, SCREEN_H - 94, 7, 7, TRAP_COL[trap_loaded][0], TRAP_COL[trap_loaded][1], TRAP_COL[trap_loaded][2]);
		font_write(bmp, FONT_NORMAL, 20, SCREEN_H - 99, line);
	}
	if(flame_fuel > 0) {
		char line[64];
		int fuel = (int)((flame_fuel + 59) / 60);
		int left = (int)((flame_hold_until - enhanced_ticks) / 60);
		snprintf(line, sizeof(line), "flamethrower  fuel %ds  spoils in %ds", fuel, left < 0 ? 0 : left);
		rect(bmp, 8, SCREEN_H - 160, 7, 7, 255, 130, 40);
		font_write(bmp, FONT_NORMAL, 20, SCREEN_H - 165, line);
	}
	if(kit_carried) {
		rect(bmp, 8, SCREEN_H - 116, 7, 7, 220, 220, 200);
		font_write(bmp, FONT_NORMAL, 20, SCREEN_H - 121, (char *)"bunker kit  (R1)");
	}

	// the rounds in the gun
	if(ammo_type >= 0) {
		char line[80];
		snprintf(line, sizeof(line), "%s  %d  %ds", AMMO_NAME[ammo_type], ammo_left, (int)((ammo_until - enhanced_ticks) / 60));
		rect(bmp, 8, SCREEN_H - 72, 7, 7, AMMO_COL[ammo_type][0], AMMO_COL[ammo_type][1], AMMO_COL[ammo_type][2]);
		font_write(bmp, FONT_NORMAL, 20, SCREEN_H - 77, line);
	}

	// the hive's message
	if(msg_timer > 0 && (msg_timer > 40 || (msg_timer / 4) % 2 == 0)) {
		int w = font_length(FONT_NORMAL2, msg);
		font_write(bmp, FONT_NORMAL2, (SCREEN_W - w) / 2, 10, msg);
	}
}


///////////////////////////////////////////////////////////////////////////////
// Laser sight

static void blend_pixel(SDL_Surface *s, int x, int y, int r, int g, int b, float a) {
	if(x < 0 || y < 0 || x >= s->w || y >= s->h)
		return;
	Uint16 *p = (Uint16 *)((Uint8 *)s->pixels + y * s->pitch + x * 2);
	Uint8 or_, og, ob;
	SDL_GetRGB(*p, s->format, &or_, &og, &ob);
	*p = (Uint16)SDL_MapRGB(s->format, (int)(or_ + (r - or_) * a), (int)(og + (g - og) * a), (int)(ob + (b - ob) * a));
}

void laser_sight_draw(SDL_Surface *bmp) {
	if(!player.ok || config.laser == 0 || (config.laser == 1 && !enhanced))
		return;
	if(bmp->format->BitsPerPixel != 16)
		return;

	// from the muzzle, along the way the bullets leave (the sprite's gun hangs 40 degrees off the body, the
	// bullets go 60 degrees off it)
	float mx, my, dx, dy;
	move_xy(28, add_angle(player.angle, -40), &mx, &my);
	mx += player.x;
	my += player.y;
	move_xy(1.0f, add_angle(player.angle, -60), &dx, &dy);

	const int RANGE = enhanced_flame_active() ? (int)tune.flame_range : 260;
	if(SDL_MUSTLOCK(bmp))
		SDL_LockSurface(bmp);
	int end = RANGE;
	for(int t=0; t<=RANGE; t++) {
		float px = mx + dx * t, py = my + dy * t;
		if(t > 4 && t % 3 == 0) {
			if(check_bul_collision((int)px, (int)py)) { end = t; break; }
			bool hit = false;
			if(t % 6 == 0) {
				for(CRITTER *c = critter_first; c; c = c->next) {
					if(c->remove || c->hidden)
						continue;
					if(rects_collide(px, py, 1, 1, c->x - c->hs, c->y - c->hs, 2 * c->hs, 2 * c->hs)) { hit = true; break; }
				}
			}
			if(hit) { end = t; break; }
		}
		float a = 0.50f * (1.0f - (float)t / (RANGE * 1.2f));
		if(a < 0.14f) a = 0.14f;
		blend_pixel(bmp, (int)px, (int)py, 255, 40, 30, a);
		// a faint halo to each side so that the line is easy to see on the sand
		blend_pixel(bmp, (int)(px - dy + 0.5f), (int)(py + dx + 0.5f), 255, 40, 30, a * 0.25f);
		blend_pixel(bmp, (int)(px + dy + 0.5f), (int)(py - dx + 0.5f), 255, 40, 30, a * 0.25f);
	}
	// a small dot where it ends
	{
		int ex = (int)(mx + dx * end), ey = (int)(my + dy * end);
		for(int yy=-1; yy<=1; yy++)
			for(int xx=-1; xx<=1; xx++)
				blend_pixel(bmp, ex + xx, ey + yy, 255, 60, 40, (xx == 0 && yy == 0) ? 0.85f : 0.45f);
	}
	if(SDL_MUSTLOCK(bmp))
		SDL_UnlockSurface(bmp);
}


///////////////////////////////////////////////////////////////////////////////
// The legend (L2)

void enhanced_bug_killed(int type, float x, float y) {
	if(type >= 0 && type < NUM_CTYPES)
		stat_kills[type]++;
	flame_count_kill(type, x, y);
}

// a scaled copy of (a part of) a sprite into box x box around (cx, cy); nearest neighbour, colour key and alpha respected
static void blit_fit(SDL_Surface *dst, SDL_Surface *src, const SDL_Rect *part, int cx, int cy, int box) {
	if(!src || dst->format->BitsPerPixel != 16)
		return;
	SDL_Rect all;
	all.x = 0; all.y = 0; all.w = src->w; all.h = src->h;
	const SDL_Rect *r = part ? part : &all;
	float scale = (float)box / (float)(r->w > r->h ? r->w : r->h);
	if(scale > 1.0f)
		scale = 1.0f;
	int dw = (int)(r->w * scale + 0.5f), dh = (int)(r->h * scale + 0.5f);
	int x0 = cx - dw / 2, y0 = cy - dh / 2;
	int bpp = src->format->BytesPerPixel;
	if(SDL_MUSTLOCK(src))
		SDL_LockSurface(src);
	if(SDL_MUSTLOCK(dst))
		SDL_LockSurface(dst);
	for(int y=0; y<dh; y++) {
		for(int x=0; x<dw; x++) {
			int sx = r->x + (int)((x + 0.5f) / scale), sy = r->y + (int)((y + 0.5f) / scale);
			if(sx >= src->w || sy >= src->h)
				continue;
			Uint8 *p = (Uint8 *)src->pixels + sy * src->pitch + sx * bpp;
			Uint32 v = (bpp == 2) ? *(Uint16 *)p : (bpp == 4 ? *(Uint32 *)p : (p[0] | (p[1] << 8) | (p[2] << 16)));
			if((src->flags & SDL_SRCCOLORKEY) && v == src->format->colorkey)
				continue;
			Uint8 rr, gg, bb, aa;
			SDL_GetRGBA(v, src->format, &rr, &gg, &bb, &aa);
			if(aa < 20)
				continue;
			int px = x0 + x, py = y0 + y;
			if(px < 0 || py < 0 || px >= dst->w || py >= dst->h)
				continue;
			Uint16 *q = (Uint16 *)((Uint8 *)dst->pixels + py * dst->pitch + px * 2);
			if(aa < 255) {
				Uint8 orr, ogg, obb;
				SDL_GetRGB(*q, dst->format, &orr, &ogg, &obb);
				float a = aa / 255.0f;
				rr = (Uint8)(orr + (rr - orr) * a); gg = (Uint8)(ogg + (gg - ogg) * a); bb = (Uint8)(obb + (bb - obb) * a);
			}
			*q = (Uint16)SDL_MapRGB(dst->format, rr, gg, bb);
		}
	}
	if(SDL_MUSTLOCK(dst))
		SDL_UnlockSurface(dst);
	if(SDL_MUSTLOCK(src))
		SDL_UnlockSurface(src);
}

static void legend_row(SDL_Surface *bmp, int col, int row, int nicons, SDL_Surface **icons, const SDL_Rect *part, char *title, char *text) {
	int x = 8 + col * 338, y = 40 + row * 44;
	int iw = nicons > 1 ? 24 : 34;
	for(int k=0; k<nicons; k++)
		blit_fit(bmp, icons[k], (k == 0) ? part : NULL, x + iw / 2 + k * 25, y + 19, 28);
	int tx = x + (nicons > 1 ? nicons * 25 : 42) + 4;
	font_write(bmp, FONT_NORMAL2, tx, y + 2, title);
	font_write(bmp, FONT_NORMAL, tx, y + 22, text);
}

void enhanced_show_legend() {
	SDL_Surface *bmp = SDL_GetVideoSurface();
#ifdef FORMIDO_TESTHOOKS
	fprintf(stderr, "legend: open (video surface %p, %d bpp)\n", (void *)bmp, bmp ? bmp->format->BitsPerPixel : -1);
#endif
	if(!bmp || bmp->format->BitsPerPixel != 16)
		return;
	// the paused picture, dimmed
	if(SDL_MUSTLOCK(bmp))
		SDL_LockSurface(bmp);
	for(int y=0; y<bmp->h; y++) {
		Uint16 *p = (Uint16 *)((Uint8 *)bmp->pixels + y * bmp->pitch);
		for(int x=0; x<bmp->w; x++)
			p[x] = (Uint16)((p[x] >> 3) & 0x18E3);
	}
	if(SDL_MUSTLOCK(bmp))
		SDL_UnlockSurface(bmp);

	font_write(bmp, FONT_NORMAL2, (SCREEN_W - font_length(FONT_NORMAL2, (char *)"legend")) / 2, 10, (char *)"legend");

	SDL_Surface *one[4];
	SDL_Rect plant_part;
	plant_part.x = 8; plant_part.y = 8; plant_part.w = 64; plant_part.h = 64;

	one[0] = cell_spr[1]; one[1] = cell_spr[3];
	legend_row(bmp, 0, 0, 2, one, NULL, (char *)"power cells", (char *)"beat guards, take home");
	one[0] = ammo_spr[0]; one[1] = ammo_spr[3];
	legend_row(bmp, 0, 1, 2, one, NULL, (char *)"special rounds", (char *)"tip: the rank it hurts");
	one[0] = tbox_spr[0]; one[1] = tbox_spr[1]; one[2] = tbox_spr[2];
	legend_row(bmp, 0, 2, 3, one, NULL, (char *)"trap discs", (char *)"R2 lays, stay off");
	one[0] = kit_spr;
	legend_row(bmp, 0, 3, 1, one, NULL, (char *)"bunker kit", (char *)"R1 deploy, R1 leave");
	one[0] = hplant_img;
	legend_row(bmp, 0, 4, 1, one, hplant_img ? &plant_part : NULL, (char *)"healing plant", (char *)"stand near, it fades");
	one[0] = gfx_pu_health[0]; one[1] = gfx_pu_bomb[0]; one[2] = gfx_pu_mine[0]; one[3] = gfx_pu_mush[0];
	legend_row(bmp, 0, 5, 4, one, NULL, (char *)"classic pickups", (char *)"the original four");
	one[0] = NULL;
	{
		// the red plants: a small drawn burst
		int x = 8 + 17, y = 40 + 6 * 44 + 18;
		for(int k=0; k<9; k++) {
			float a = k * 6.2831853f / 9;
			for(int t=3; t<13; t++)
				rect(bmp, x + (int)(cosf(a) * t), y + (int)(sinf(a) * t * 0.85f), 2, 2, 230, 50, 40);
		}
		font_write(bmp, FONT_NORMAL2, 8 + 46, 40 + 6 * 44 + 2, (char *)"red plants");
		font_write(bmp, FONT_NORMAL, 8 + 46, 40 + 6 * 44 + 22, (char *)"thorns, light scratch");
	}
	{
		// the corner map
		int x = 8 + 4, y = 40 + 7 * 44 + 6;
		rect(bmp, x + 9, y, 8, 6, 120, 120, 100);
		rect(bmp, x, y + 7, 8, 6, 70, 60, 50); rect(bmp, x + 9, y + 7, 8, 6, 150, 200, 120); rect(bmp, x + 18, y + 7, 8, 6, 70, 60, 50);
		rect(bmp, x + 9, y + 14, 8, 6, 70, 60, 50);
		rect(bmp, x + 12, y + 3, 3, 3, 90, 160, 255);
		font_write(bmp, FONT_NORMAL2, 8 + 46, 40 + 7 * 44 + 2, (char *)"corner map");
		font_write(bmp, FONT_NORMAL, 8 + 46, 40 + 7 * 44 + 22, (char *)"hollow dot = sealed");
	}

	one[0] = badge_spr[0];
	legend_row(bmp, 1, 0, 1, one, NULL, (char *)"burning", (char *)"lingers after fire");
	one[0] = badge_spr[1];
	legend_row(bmp, 1, 1, 1, one, NULL, (char *)"bleeding", (char *)"takes 25% more dmg");
	one[0] = badge_spr[2];
	legend_row(bmp, 1, 2, 1, one, NULL, (char *)"slowed", (char *)"spikes and glue");
	one[0] = fire_glow;
	legend_row(bmp, 1, 3, 1, one, NULL, (char *)"napalm field", (char *)"burns what is inside");
	one[0] = spike_spr[0]; one[1] = glue_spr[0];
	legend_row(bmp, 1, 4, 2, one, NULL, (char *)"spikes, glue", (char *)"bleed, slow, hold");
	{
		int x = 346, y = 40 + 5 * 44;
		rect(bmp, x + 2, y + 12, 30, 12, 150, 28, 62);
		rect(bmp, x + 2, y + 12, 15, 12, 220, 70, 80);
		font_write(bmp, FONT_NORMAL2, x + 46, y + 2, (char *)"hive tyrant");
		font_write(bmp, FONT_NORMAL, x + 46, y + 22, (char *)"boss bar, slay it");
	}
	{
		int x = 346, y = 40 + 6 * 44;
		for(int k=0; k<3; k++)
			rect(bmp, x + 4 + k * 10, y + 22 - k * 3, 6, 6, 190, 150, 100);
		font_write(bmp, FONT_NORMAL2, x + 46, y + 2, (char *)"ground trembles");
		font_write(bmp, FONT_NORMAL, x + 46, y + 22, (char *)"burrower: move!");
	}
	one[0] = flame_spr;
	legend_row(bmp, 1, 7, 1, one, NULL, (char *)"flamethrower", (char *)"after many kills");
	// a light stats strip
	{
		rect(bmp, 14, 394, SCREEN_W - 28, 2, 90, 90, 80);
		int secs = (int)(enhanced_ticks / 60);
		char l1[100], l2[100];
		snprintf(l1, sizeof(l1), "time %d:%02d  score %d  tyrants %d  hive %d  cells %d", secs / 60, secs % 60, kills, stat_tyrants, hive_level, stat_cells);
		snprintf(l2, sizeof(l2), "drones %d runners %d strikers %d brutes %d burrow %d",
			stat_kills[CT_DRONE], stat_kills[CT_RUNNER], stat_kills[CT_STRIKER], stat_kills[CT_BRUTE], stat_kills[CT_BURROWER] + stat_kills[CT_STUNNER]);
		font_write(bmp, FONT_NORMAL2, 14, 400, l1);
		font_write(bmp, FONT_NORMAL, 14, 424, l2);
	}
	font_write(bmp, FONT_NORMAL, (SCREEN_W - font_length(FONT_NORMAL, (char *)"press any button to go on")) / 2, SCREEN_H - 24, (char *)"press any button to go on");

#ifdef FORMIDO_TESTHOOKS
	SDL_SaveBMP(bmp, "legend.bmp");		// test builds: what the legend looks like
#endif

	// wait: first for every button to be let go (L2 itself), then for a new press
	unsigned prev = 0xFFFFFFFFu;
	bool released = false;
	for(int guard=0; guard<60 * 60 * 10; guard++) {
		SDL_Event ev;
		while(sf3500_poll(&ev)) {
			if(ev.type == SDL_QUIT) {
#ifdef FORMIDO_TESTHOOKS
				fprintf(stderr, "legend: closed by quit\n");
#endif
				return;
			}
		}
		if(sf3500_quit_requested())
			return;
		unsigned b = sf3500_buttons();
#ifdef FORMIDO_TESTHOOKS
		if(guard % 30 == 0)
			fprintf(stderr, "legend: frame %d buttons 0x%x released %d\n", guard, b, (int)released);
#endif
		if(b == 0)
			released = true;
		if(released && (b & ~(prev == 0xFFFFFFFFu ? 0u : prev)) != 0) {
#ifdef FORMIDO_TESTHOOKS
			fprintf(stderr, "legend: closed by buttons 0x%x after %d frames\n", b, guard);
#endif
			return;
		}
		prev = b;
		SDL_Flip(bmp);			// the glue's flip: present
		usleep(25000);			// ~40 frames a second is plenty for a still picture
	}
}


///////////////////////////////////////////////////////////////////////////////
// Test builds only: a bot that plays the enhanced game through the same buttons a person uses (FORMIDO_BOT=<skill 0..2>,
// FORMIDO_SEED=<n>, FORMIDO_FAST=1 for no drawing or pacing). It stays on the platform screen: it never fetches power cells,
// ammunition or traps from the arms, so what it measures is the game without the arm economy, a conservative baseline.
// skill 0: slow reactions (200 ms), sloppy aim; 1: 150 ms; 2: 100 ms, tight aim, uses pickups sensibly; 3: expert, 50 ms.

#ifdef FORMIDO_TESTHOOKS


static const unsigned B_LEFT = 1u << 0, B_RIGHT = 1u << 1, B_UP = 1u << 2, B_DOWN = 1u << 3, B_A = 1u << 4, B_B = 1u << 5,
	B_L1 = 1u << 6, B_R1 = 1u << 7, B_X = 1u << 8, B_Y = 1u << 9, B_R2 = 1u << 13;

unsigned enhanced_bot_buttons(void) {
	bot_calls++;
	if(!bot_in_match) {
		// the menu, by the clock (a pass of its loop can be microseconds): down once, to "play enhanced game", then select
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		static long long t0 = 0;
		long long now = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
		if(!t0)
			t0 = now;
		long long t = now - t0;
		if(t >= 1000 && t < 1250)
			return B_DOWN;
		if(t >= 1800 && t < 2050)
			return B_A;
		return 0;
	}
	static int skill = -1;
	static unsigned held = 0;
	static int wait = 0;
	static int pulse = 0;
	static int last_sector = 0, lag = 0;
	if(skill < 0)
		skill = atoi(getenv("FORMIDO_BOT"));
	int sk = skill >= 4 ? (skill == 4 ? 1 : 2) : skill;		// skills 4 and 5 are the mobile bots: skill 1 and 2 reactions and aim
	bool mobile = skill >= 4;
	static bool healing = false;
	static float arena_r = -1, heal_lo = 100, heal_hi = 175, pull = 9000;
	if(arena_r < 0) {		// the mobile bot's numbers can be changed for experiments
		arena_r = getenv("FORMIDO_BOTARENA") ? (float)atof(getenv("FORMIDO_BOTARENA")) : 235.0f;
		if(getenv("FORMIDO_BOTLO")) heal_lo = (float)atof(getenv("FORMIDO_BOTLO"));
		if(getenv("FORMIDO_BOTHI")) heal_hi = (float)atof(getenv("FORMIDO_BOTHI"));
		if(getenv("FORMIDO_BOTPULL")) pull = (float)atof(getenv("FORMIDO_BOTPULL"));
	}
	if(mobile) {
		// roams the screen, goes back to the platform when hurt, fights from it until mended
		if(!healing && player.health < heal_lo)
			healing = true;
		else if(healing && player.health >= heal_hi)
			healing = false;
	}
	if(!player.ok)
		return 0;
	if(skill == 9) {		// bunker test: deploy the kit, walk in, then hold fire with no direction at all
		if(enhanced_ticks >= 30 && enhanced_ticks < 40)
			return B_R1;
		if(enhanced_ticks >= 60 && enhanced_ticks < 140)
			return B_UP;
		if(getenv("FORMIDO_BUNKEREXIT")) {		// step out at tick 400, walk about, walk back in at tick 700
			if(enhanced_ticks >= 400 && enhanced_ticks < 410)
				return B_R1;
			if(enhanced_ticks >= 410 && enhanced_ticks < 700 && !enhanced_in_bunker()) {
				if(enhanced_ticks % 60 == 0)
					fprintf(stderr, "t=%ld outside: distance to bunker %.0f, in=%d\n", enhanced_ticks, distance(player.x, player.y, bunker.x, bunker.y), (int)enhanced_in_bunker());
				return enhanced_ticks < 520 ? (B_LEFT | B_A) : B_A;
			}
			if(enhanced_ticks >= 700 && !enhanced_in_bunker()) {
				unsigned b = B_A;
				if(player.x < bunker.x - 6) b |= B_RIGHT; else if(player.x > bunker.x + 6) b |= B_LEFT;
				if(player.y < bunker.y - 6) b |= B_DOWN; else if(player.y > bunker.y + 6) b |= B_UP;
				return b;
			}
			if(enhanced_ticks >= 700 && enhanced_ticks % 60 == 0)
				fprintf(stderr, "t=%ld back inside: %d bunker ok %d integrity %.0f\n", enhanced_ticks, (int)enhanced_in_bunker(), (int)bunker.ok, bunker.integrity);
		}
		if(enhanced_ticks >= 200)
			return B_A;
		return 0;
	}
	if(num_critters > bot_peak_bugs)
		bot_peak_bugs = num_critters;
	// a decision every few passes, like a person's reaction time (a pass is 3 ticks)
	int react = sk == 0 ? 4 : (sk == 1 ? 3 : (sk == 2 ? 2 : 1));
	if(--wait > 0 && pulse <= 0)
		return held;
	wait = react;
	if(pulse > 0)
		pulse--;

	float px = player.x, py = player.y;
	int near90 = 0, near130 = 0, near220 = 0;
	CRITTER *nearest = NULL;
	float nd = 1e9f;
	float reach = 130.0f + 20.0f * sk;
	struct BP { float x, y, R; };
	static BP bp[160];
	int nb = 0;
	for(CRITTER *c = critter_first; c && nb < 160; c = c->next) {
		if(c->remove || c->hidden)
			continue;
		float dx = px - c->x, dy = py - c->y;
		float d = sqrtf(dx * dx + dy * dy) + 0.01f;
		if(d < nd) { nd = d; nearest = c; }
		if(d < 90) near90++;
		if(d < 130) near130++;
		if(d < 220) near220++;
		float R = reach * ((c->type == CT_BRUTE || c->type == CT_TYRANT) ? 1.6f : 1.0f);
		if(d < 400) {
			// where it will be in about half a second: it keeps walking at us
			float step = (c->type == CT_RUNNER ? 40.0f : (c->type == CT_BRUTE || c->type == CT_TYRANT ? 14.0f : 26.0f));
			bp[nb].x = c->x + (px - c->x) / d * step;
			bp[nb].y = c->y + (py - c->y) / d * step;
			bp[nb].R = R;
			nb++;
		}
	}
	// a target worth walking to (sk 1 and 2): health, bombs and mines (never the mushroom), a healing plant when hurt
	float tx = 0, ty = 0;
	bool have_target = false;
	if(sk >= 1 && near90 == 0) {
		float bd = 1e9f;
		for(int i=0; i<MAX_POWER_UPS; i++) {
			if(!power_up[i].ok || power_up[i].type == POWER_UP_MUSHROOM)
				continue;
			float lim = (power_up[i].type == POWER_UP_HEALTH && player.health < 150) ? 300.0f : 150.0f;
			float d = distance(px, py, (float)power_up[i].x, (float)power_up[i].y);
			if(d < lim && d < bd) { bd = d; tx = (float)power_up[i].x; ty = (float)power_up[i].y; have_target = true; }
		}
		for(int i=0; i<MAX_HEAL_PLANTS; i++) {
			if(!hplant[i].ok || player.health >= 150)
				continue;
			float d = distance(px, py, hplant[i].x, hplant[i].y);
			if(d < 260 && d < bd) { bd = d; tx = hplant[i].x; ty = hplant[i].y; have_target = true; }
		}
	}
	// look ahead: score standing still and the eight ways to walk (45 pixels, about seven ticks of walking), keep the safest
	float cx = 320.0f - px, cy = 240.0f - py;
	float cd = sqrtf(cx * cx + cy * cy) + 0.01f;
	unsigned b = 0;
	{
		static const int DX[9] = { 0, 1, 1, 0, -1, -1, -1, 0, 1 };
		static const int DY[9] = { 0, 0, 1, 1, 1, 0, -1, -1, -1 };
		float best = 1e18f;
		int bi = 0;
		for(int k=0; k<9; k++) {
			float l = (DX[k] && DY[k]) ? 0.7071f : 1.0f;
			float fx = px + DX[k] * 45.0f * l, fy = py + DY[k] * 45.0f * l;
			if(k > 0 && world_blocked((int)fx, (int)fy, false))
				continue;
			float cost = 0;
			for(int i=0; i<nb; i++) {
				float dx = fx - bp[i].x, dy = fy - bp[i].y;
				float d = sqrtf(dx * dx + dy * dy);
				if(d < bp[i].R) {
					float w = (bp[i].R - d);
					cost += w * w;
				}
			}
			// stay near the middle of the platform (the camping bots), or roam most of the screen (the mobile ones)
			float ex = fx - 320.0f, ey = fy - 240.0f;
			float ed = sqrtf(ex * ex + ey * ey);
			float arena = (mobile && !healing) ? arena_r : 140.0f;
			if(ed > arena)
				cost += (ed - arena) * (ed - arena) * 1.5f;
			if(mobile && healing) {
				float pe = sqrtf((ex / 100.0f) * (ex / 100.0f) + (ey / 112.0f) * (ey / 112.0f));	// 1.0 = the platform's edge
				if(pe > 0.85f)
					cost += (pe - 0.85f) * pull;
			}
			if(have_target) {
				float td = distance(fx, fy, tx, ty);
				cost += td * 6.0f;
			}
			if(k == 0)
				cost += (near220 == 0 && cd < 90.0f && player.health < 200) ? -50.0f : 40.0f;		// standing still mends on the platform
			if(cost < best) { best = cost; bi = k; }
		}
		if(DX[bi] > 0) b |= B_RIGHT;
		if(DX[bi] < 0) b |= B_LEFT;
		if(DY[bi] > 0) b |= B_DOWN;
		if(DY[bi] < 0) b |= B_UP;
	}
	// aim (eight directions only) along the line that actually hits the closest bug; if no line hits one, towards the
	// nearest bug, which is walking at us anyway. A bullet hits what is within the bug's half size of the line.
	float range = 260.0f + 20.0f * sk;
	{
		int best_sector = -1;
		float best_d = 1e9f;
		float slack = sk == 0 ? 3.0f : 0.0f;
		for(int k=0; k<8; k++) {
			float ca = cosf(k * 0.785398f), sa = sinf(k * 0.785398f);
			for(CRITTER *c = critter_first; c; c = c->next) {
				if(c->remove || c->hidden || c->type == CT_PHANTOM)
					continue;
				float dx = c->x - px, dy = c->y - py;
				float along = dx * ca + dy * sa;
				if(along < 12.0f || along > range)
					continue;
				float lateral = fabsf(-dx * sa + dy * ca);
				if(lateral < c->hs + slack && along < best_d) {
					best_d = along;
					best_sector = k;
				}
			}
		}
		int sector = best_sector;
		if(sector < 0 && nearest && nd < range)
			sector = ((int)floorf(atan2f(nearest->y - py, nearest->x - px) / 0.785398f + 0.5f) + 8) % 8;
		if(sector >= 0) {
			if(sk == 0) {
				if(lag > 0) { sector = last_sector; lag--; }
				else if(RANDF(0, 1) < 0.25f) lag = 4;
			}
			last_sector = sector;
			unsigned a = 0;
			switch(sector & 7) {
				case 0: a = B_A; break;
				case 1: a = B_A | B_B; break;
				case 2: a = B_B; break;
				case 3: a = B_B | B_Y; break;
				case 4: a = B_Y; break;
				case 5: a = B_Y | B_X; break;
				case 6: a = B_X; break;
				case 7: a = B_X | B_A; break;
			}
			b |= B_L1 | a;
		}
	}
	// a bomb or a mine when they crowd in; the traps and the bunker kit if the bot ever has them
	if(player.special_weapon != 0 && near130 >= 4 && sk >= 1) {
		b = (b & ~(B_L1 | B_A | B_B | B_X | B_Y)) | B_B;		// B without L1 is "special"
		pulse = 1;
	}
	if(trap_loaded >= 0 && trap_charges > 0 && near130 >= 3 && sk >= 1)
		b |= B_R2;
	if(kit_carried && near130 >= 4 && player.health < 140)
		b |= B_R1;
	held = b;
	return b;
}

void enhanced_bot_report() {
	int total = 0;
	for(int i=0; i<NUM_CTYPES; i++)
		total += stat_kills[i];
	int dead_roaming = 0;
	for(CRITTER *c = critter_first; c; c = c->next)
		if(!c->remove && !c->hidden)
			dead_roaming++;
	fprintf(stderr, "nav floods %ld, %.2f ms each\n", nav_floods, nav_floods ? nav_us / 1000.0 / nav_floods : 0.0);
	if(getenv("FORMIDO_DUMP")) {
		printf("PLAYER %d %d\n", (int)player.x + (gx + 1) * CELL_W, (int)player.y + (gy + 1) * CELL_H);
		for(CRITTER *c = critter_first; c; c = c->next)
			if(!c->remove && !c->hidden)
{
				float ttx, tty;
				world_target(c->x, c->y, &ttx, &tty);
				printf("BUG %d %d %d %d %d rel=%.1f,%.1f d=%.2f,%.2f tgt=%.0f,%.0f bl=%d%d%d%d mode=%d gxy=%d,%d\n", (int)c->x + (gx + 1) * CELL_W, (int)c->y + (gy + 1) * CELL_H, c->type, c->guard, fabsf(c->x - c->sx) + fabsf(c->y - c->sy) < 0.4f,
					c->x, c->y, c->dirx, c->diry, ttx, tty, world_blocked((int)c->x+3,(int)c->y,false), world_blocked((int)c->x-3,(int)c->y,false), world_blocked((int)c->x,(int)c->y+3,false), world_blocked((int)c->x,(int)c->y-3,false), enhanced_bug_mode(c), gx, gy); }
	}
	printf("RESULT skill=%d seed=%s ticks=%ld secs=%ld score=%d kills=%d drone=%d runner=%d striker=%d brute=%d burrower=%d stunner=%d phantom=%d tyrant=%d tyrants_seen=%d tyrants_slain=%d hive=%d peak_bugs=%d bugs_at_end=%d capped=%d healed=%ld platform_secs=%ld dmg=%ld dmg_ticks=%ld alive_ticks=%ld far_ticks=%ld stall_ticks=%ld first_hurt_secs=%ld hits=%ld,%ld,%ld,%ld,%ld,%ld,%ld\n",
		atoi(getenv("FORMIDO_BOT")), getenv("FORMIDO_SEED") ? getenv("FORMIDO_SEED") : "-", enhanced_ticks, enhanced_ticks / 60, kills, total,
		stat_kills[CT_DRONE], stat_kills[CT_RUNNER], stat_kills[CT_STRIKER], stat_kills[CT_BRUTE], stat_kills[CT_BURROWER],
		stat_kills[CT_STUNNER], stat_kills[CT_PHANTOM], stat_kills[CT_TYRANT], bot_tyrants_seen, stat_tyrants, hive_level,
		bot_peak_bugs, dead_roaming, player.ok ? 1 : 0, bot_healed, bot_platform_ticks / 60,
		bot_dmg_total, bot_dmg_ticks, bot_alive_ticks, bot_far_ticks, bot_stall_ticks, bot_first_hurt < 0 ? -1L : bot_first_hurt / 60,
		bot_hits[1], bot_hits[2], bot_hits[3], bot_hits[4], bot_hits[5], bot_hits[6], bot_hits[7]);
	fflush(stdout);
}

static void bot_minute_log() {
	if(!getenv("FORMIDO_BOT") || enhanced_ticks % 3600 != 0 || enhanced_ticks == bot_last_tick)
		return;
	bot_last_tick = enhanced_ticks;
	int total = 0;
	for(int i=0; i<NUM_CTYPES; i++)
		total += stat_kills[i];
	printf("MIN %ld kills=%d bugs=%d hp=%d tyrants=%d\n", enhanced_ticks / 3600, total, num_critters, player.health, stat_tyrants);
	fflush(stdout);
}

#endif

#endif
