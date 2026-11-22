// SPDX-License-Identifier: GPL-2.0-or-later
/* Datafrog SF3500 glue for Formido.
 *
 *  - display: SDL runs on its "dummy" video driver (a plain software surface); every flip
 *    hands that surface to the device's display driver through hwdisp.
 *  - input:   the gamepad (joykey) is turned into SDL key events, so the game's own key
 *    handling stays untouched.
 *  - audio:   the few SDL_mixer calls Formido makes are answered here and mixed in software
 *    (sf3500_mixer -> hwaudio). Music is Ogg Vorbis through stb_vorbis.
 *  - images:  Formido's .dat files are PCX / PNG / JPEG / BMP; decoded here (own PCX reader,
 *    stb_image for the rest) so SDL_image is not needed.
 *
 * This file does not include init.h, so SDL_Flip / SDL_PollEvent below are SDL's own. */

#include "sf3500_glue.h"
#include "SDL_mixer.h"
#include "SDL_image.h"

#include "hwdisp.hpp"
#include "joykey.hpp"
#include "sf3500_mixer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#include "stb_image.h"

#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utility>
#include <vector>

/* ------------------------------------------------------------------ helpers */

static char g_err[256] = "";

static void set_err(const char *what, const char *file)
{
    snprintf(g_err, sizeof(g_err), "%s: %s", what, file ? file : "?");
}

static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static bool read_file(const char *file, std::vector<unsigned char> &out)
{
    FILE *f = fopen(file, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return false; }
    out.resize((size_t)n);
    size_t got = n ? fread(&out[0], 1, (size_t)n, f) : 0;
    fclose(f);
    return got == (size_t)n;
}

/* ------------------------------------------------------------------ images */

/* ZSoft PCX: 8 bits per plane, 1 plane (256-colour palette at the end) or 3 planes (RGB). */
static SDL_Surface *decode_pcx(const unsigned char *d, size_t n)
{
    if (n < 129 || d[0] != 10 || d[2] != 1 || d[3] != 8) return NULL;
    int xmin = d[4] | (d[5] << 8), ymin = d[6] | (d[7] << 8);
    int xmax = d[8] | (d[9] << 8), ymax = d[10] | (d[11] << 8);
    int planes = d[65];
    int bpl = d[66] | (d[67] << 8); /* bytes per plane per scanline */
    int w = xmax - xmin + 1, h = ymax - ymin + 1;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return NULL;
    if ((planes != 1 && planes != 3) || bpl < w) return NULL;

    const unsigned char *pal = NULL;
    if (planes == 1) {
        if (n < 128 + 769 || d[n - 769] != 12) return NULL;
        pal = d + n - 768;
    }

    SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (!s) return NULL;

    std::vector<unsigned char> line((size_t)bpl * planes);
    size_t pos = 128;
    for (int y = 0; y < h; y++) {
        size_t got = 0;
        while (got < line.size()) {
            if (pos >= n) { SDL_FreeSurface(s); return NULL; }
            unsigned char c = d[pos++];
            int run = 1;
            if (c >= 0xC0) {
                run = c & 0x3F;
                if (pos >= n) { SDL_FreeSurface(s); return NULL; }
                c = d[pos++];
            }
            while (run-- > 0 && got < line.size()) line[got++] = c;
        }
        unsigned char *row = (unsigned char *)s->pixels + (size_t)y * s->pitch;
        for (int x = 0; x < w; x++) {
            if (planes == 3) {
                row[x * 3 + 0] = line[x];
                row[x * 3 + 1] = line[bpl + x];
                row[x * 3 + 2] = line[2 * bpl + x];
            } else {
                const unsigned char *p = pal + 3 * line[x];
                row[x * 3 + 0] = p[0];
                row[x * 3 + 1] = p[1];
                row[x * 3 + 2] = p[2];
            }
        }
    }
    return s;
}

SDL_Surface *IMG_Load(const char *file)
{
    std::vector<unsigned char> buf;
    if (!read_file(file, buf) || buf.empty()) {
        set_err("cannot read image", file);
        return NULL;
    }
    SDL_Surface *s = NULL;
    if (buf[0] == 10) {
        s = decode_pcx(&buf[0], buf.size());
    } else {
        int w = 0, h = 0, comp = 0;
        unsigned char *px = stbi_load_from_memory(&buf[0], (int)buf.size(), &w, &h, &comp, 0);
        if (px) {
            if (comp == 2) { /* grey + alpha: widen to RGBA */
                unsigned char *rgba = (unsigned char *)malloc((size_t)w * h * 4);
                for (int i = 0; i < w * h; i++) {
                    rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = px[i * 2];
                    rgba[i * 4 + 3] = px[i * 2 + 1];
                }
                stbi_image_free(px);
                px = rgba;
                comp = 4;
            } else if (comp == 1) { /* grey: widen to RGB */
                unsigned char *rgb = (unsigned char *)malloc((size_t)w * h * 3);
                for (int i = 0; i < w * h; i++) rgb[i * 3 + 0] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = px[i];
                stbi_image_free(px);
                px = rgb;
                comp = 3;
            }
            bool alpha = (comp == 4);
            s = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, alpha ? 32 : 24, 0x0000FF, 0x00FF00, 0xFF0000,
                                     alpha ? 0xFF000000u : 0);
            if (s) {
                for (int y = 0; y < h; y++)
                    memcpy((unsigned char *)s->pixels + (size_t)y * s->pitch, px + (size_t)y * w * comp,
                           (size_t)w * comp);
            }
            free(px);
        }
    }
    if (!s) set_err("cannot decode image", file);
    return s;
}

const char *IMG_GetError(void) { return g_err; }

/* ------------------------------------------------------------------ audio */

#define MIX_RATE 48000
#define NUM_CHANNELS 24

struct Mix_Chunk {
    std::vector<int16_t> pcm; /* interleaved stereo S16 at MIX_RATE */
};

struct Mix_Music {
    std::vector<unsigned char> raw; /* the whole .ogg file */
};

struct Channel {
    Sf3500Voice *v;
    bool loop;
};

static bool g_audio = false;
static Channel g_chan[NUM_CHANNELS];

static void reap_channels(void)
{
    for (int i = 0; i < NUM_CHANNELS; i++) {
        if (g_chan[i].v && !g_chan[i].loop && !sf3500_voice_playing(g_chan[i].v)) {
            sf3500_voice_destroy(g_chan[i].v);
            g_chan[i].v = NULL;
        }
    }
}

static unsigned rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned rd32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }

/* RIFF WAVE, PCM, 8 or 16 bit, mono or stereo, any rate -> stereo S16 at MIX_RATE. */
Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *src, int freesrc)
{
    if (!src) { set_err("no sound source", NULL); return NULL; }
    std::vector<unsigned char> d;
    {
        unsigned char tmp[4096];
        int n;
        while ((n = SDL_RWread(src, tmp, 1, sizeof(tmp))) > 0) d.insert(d.end(), tmp, tmp + n);
    }
    if (freesrc) SDL_RWclose(src);

    if (d.size() < 44 || memcmp(&d[0], "RIFF", 4) != 0 || memcmp(&d[8], "WAVE", 4) != 0) {
        set_err("not a WAV file", NULL);
        return NULL;
    }
    unsigned fmt = 0, chans = 0, rate = 0, bits = 0;
    const unsigned char *data = NULL;
    size_t dlen = 0;
    for (size_t p = 12; p + 8 <= d.size();) {
        unsigned len = rd32(&d[p + 4]);
        const unsigned char *body = &d[p + 8];
        size_t avail = d.size() - (p + 8);
        if (!memcmp(&d[p], "fmt ", 4) && avail >= 16) {
            fmt = rd16(body);
            chans = rd16(body + 2);
            rate = rd32(body + 4);
            bits = rd16(body + 14);
        } else if (!memcmp(&d[p], "data", 4)) {
            data = body;
            dlen = len < avail ? len : avail;
            break;
        }
        p += 8 + len + (len & 1);
    }
    if (fmt != 1 || (bits != 8 && bits != 16) || (chans != 1 && chans != 2) || rate == 0 || !data) {
        set_err("unsupported WAV format", NULL);
        return NULL;
    }

    size_t frames_in = dlen / ((bits / 8) * chans);
    if (frames_in == 0) { set_err("empty WAV", NULL); return NULL; }
    std::vector<int16_t> in(frames_in * 2);
    for (size_t i = 0; i < frames_in; i++) {
        for (unsigned c = 0; c < 2; c++) {
            unsigned sc = (chans == 1) ? 0 : c;
            int v;
            if (bits == 8) v = ((int)data[i * chans + sc] - 128) << 8;
            else v = (int16_t)rd16(data + (i * chans + sc) * 2);
            in[i * 2 + c] = (int16_t)v;
        }
    }

    Mix_Chunk *ch = new Mix_Chunk();
    size_t frames_out = (size_t)((unsigned long long)frames_in * MIX_RATE / rate);
    if (frames_out == 0) frames_out = 1;
    ch->pcm.resize(frames_out * 2);
    for (size_t i = 0; i < frames_out; i++) {
        double pos = (double)i * rate / MIX_RATE;
        size_t i0 = (size_t)pos;
        size_t i1 = i0 + 1 < frames_in ? i0 + 1 : i0;
        double fr = pos - (double)i0;
        for (int c = 0; c < 2; c++) {
            double v = in[i0 * 2 + c] * (1.0 - fr) + in[i1 * 2 + c] * fr;
            ch->pcm[i * 2 + c] = (int16_t)(v + (v < 0 ? -0.5 : 0.5));
        }
    }
    return ch;
}

void Mix_FreeChunk(Mix_Chunk *chunk) { delete chunk; }

int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops)
{
    if (!g_audio || !chunk || chunk->pcm.empty()) return -1;
    reap_channels();
    if (channel < 0 || channel >= NUM_CHANNELS) {
        channel = -1;
        for (int i = 0; i < NUM_CHANNELS; i++)
            if (!g_chan[i].v) { channel = i; break; }
        if (channel < 0) return -1;
    } else if (g_chan[channel].v) {
        sf3500_voice_destroy(g_chan[channel].v);
        g_chan[channel].v = NULL;
    }
    Sf3500Voice *v = sf3500_voice_create_static(&chunk->pcm[0], (int)(chunk->pcm.size() / 2));
    if (!v) return -1; /* voice pool full: drop the sound */
    sf3500_voice_set_relative(v, true);
    sf3500_voice_set_gain(v, 1.0f);
    sf3500_voice_set_looping(v, loops < 0);
    sf3500_voice_play(v);
    g_chan[channel].v = v;
    g_chan[channel].loop = loops < 0;
    return channel;
}

int Mix_SetPanning(int channel, Uint8 left, Uint8 right)
{
    if (channel < 0 || channel >= NUM_CHANNELS || !g_chan[channel].v) return 0;
    float pan = ((float)right - (float)left) / 255.0f; /* -1 left .. +1 right */
    sf3500_voice_set_relative(g_chan[channel].v, false);
    sf3500_voice_set_x(g_chan[channel].v, pan * 400.0f); /* the mixer's pan range is 400 units */
    return 1;
}

int Mix_HaltChannel(int channel)
{
    if (channel < 0 || channel >= NUM_CHANNELS || !g_chan[channel].v) return 0;
    sf3500_voice_stop(g_chan[channel].v);
    sf3500_voice_destroy(g_chan[channel].v);
    g_chan[channel].v = NULL;
    g_chan[channel].loop = false;
    return 0;
}

/* ---- music ---- */

struct MusicState {
    Sf3500Voice *voice;
    stb_vorbis *vorbis;
    int channels;
    bool loop;
    Mix_Music *cur;
    int fade; /* 0 none, 1 in, 2 out */
    long long fade_start;
    int fade_ms;
};
static MusicState g_mus = { NULL, NULL, 2, false, NULL, 0, 0, 0 };
static int g_music_vol = 128; /* 0..128, SDL_mixer's scale */

static int music_fill(void *ud, int16_t *dst, int frames)
{
    MusicState *m = (MusicState *)ud;
    int n = stb_vorbis_get_samples_short_interleaved(m->vorbis, m->channels, dst, frames * m->channels);
    if (n < frames && m->loop) {
        stb_vorbis_seek_start(m->vorbis);
        n += stb_vorbis_get_samples_short_interleaved(m->vorbis, m->channels, dst + n * m->channels,
                                                      (frames - n) * m->channels);
    }
    return n;
}

static float music_gain(void)
{
    float g = g_music_vol / 128.0f;
    if (g_mus.fade && g_mus.fade_ms > 0) {
        float t = (float)(now_ms() - g_mus.fade_start) / (float)g_mus.fade_ms;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        g *= (g_mus.fade == 1) ? t : (1.0f - t);
    }
    return g;
}

static void music_stop(void)
{
    if (g_mus.voice) {
        sf3500_voice_stop(g_mus.voice);
        sf3500_voice_destroy(g_mus.voice); /* before closing the decoder it reads from */
        g_mus.voice = NULL;
    }
    if (g_mus.vorbis) {
        stb_vorbis_close(g_mus.vorbis);
        g_mus.vorbis = NULL;
    }
    g_mus.cur = NULL;
    g_mus.fade = 0;
}

/* Called from every flip and every poll: fades and finished-voice cleanup. */
static void audio_update(void)
{
    if (!g_audio) return;
    reap_channels();
    if (g_mus.voice) {
        if (g_mus.fade == 2 && now_ms() - g_mus.fade_start >= g_mus.fade_ms) {
            music_stop();
            return;
        }
        if (g_mus.fade == 1 && now_ms() - g_mus.fade_start >= g_mus.fade_ms) g_mus.fade = 0;
        sf3500_voice_set_gain(g_mus.voice, music_gain());
    }
}

Mix_Music *Mix_LoadMUS(const char *file)
{
    Mix_Music *m = new Mix_Music();
    if (!read_file(file, m->raw) || m->raw.empty()) {
        set_err("cannot read music", file);
        delete m;
        return NULL;
    }
    return m;
}

void Mix_FreeMusic(Mix_Music *music)
{
    if (!music) return;
    if (g_mus.cur == music) music_stop();
    delete music;
}

static int music_start(Mix_Music *music, int loops, int fade_ms)
{
    if (!g_audio || !music) return -1;
    music_stop();
    int err = 0;
    stb_vorbis *vb = stb_vorbis_open_memory(&music->raw[0], (int)music->raw.size(), &err, NULL);
    if (!vb) { set_err("cannot decode music", NULL); return -1; }
    stb_vorbis_info info = stb_vorbis_get_info(vb);
    g_mus.vorbis = vb;
    g_mus.channels = info.channels;
    g_mus.loop = loops < 0;
    g_mus.cur = music;
    g_mus.voice = sf3500_voice_create_stream(music_fill, &g_mus, (int)info.sample_rate, info.channels);
    if (!g_mus.voice) {
        stb_vorbis_close(vb);
        g_mus.vorbis = NULL;
        g_mus.cur = NULL;
        set_err("no free voice for music", NULL);
        return -1;
    }
    sf3500_voice_set_relative(g_mus.voice, true);
    sf3500_voice_set_looping(g_mus.voice, false); /* looping is done in music_fill */
    g_mus.fade = fade_ms > 0 ? 1 : 0;
    g_mus.fade_ms = fade_ms;
    g_mus.fade_start = now_ms();
    sf3500_voice_set_gain(g_mus.voice, music_gain());
    sf3500_voice_play(g_mus.voice);
    return 0;
}

int Mix_PlayMusic(Mix_Music *music, int loops) { return music_start(music, loops, 0); }
int Mix_FadeInMusic(Mix_Music *music, int loops, int ms) { return music_start(music, loops, ms); }

int Mix_FadeOutMusic(int ms)
{
    if (!g_mus.voice) return 0;
    if (ms <= 0) { music_stop(); return 1; }
    g_mus.fade = 2;
    g_mus.fade_ms = ms;
    g_mus.fade_start = now_ms();
    return 1;
}

int Mix_HaltMusic(void)
{
    music_stop();
    return 0;
}

int Mix_VolumeMusic(int volume)
{
    int prev = g_music_vol;
    if (volume >= 0) {
        g_music_vol = volume > 128 ? 128 : volume;
        if (g_mus.voice) sf3500_voice_set_gain(g_mus.voice, music_gain());
    }
    return prev;
}

int Mix_OpenAudio(int, Uint16, int, int)
{
    if (g_audio) return 0;
    g_audio = sf3500_mixer_init();
    fprintf(stderr, "formido: audio %s\n", g_audio ? "on" : "FAILED (playing without sound)");
    return 0; /* a missing audio path must not stop the game */
}

const char *Mix_GetError(void) { return g_err; }

void Mix_CloseAudio(void)
{
    if (!g_audio) return;
    music_stop();
    for (int i = 0; i < NUM_CHANNELS; i++) Mix_HaltChannel(i);
    sf3500_mixer_deinit();
    g_audio = false;
}

/* ------------------------------------------------------------------ display + input */

static bool g_hw = false;
static bool g_inited = false;
static int g_in_game = 0;
static bool g_twin = false;
static bool g_enh = false;
static long long g_last_present_ms = 0;
static int g_frame_ms = 33; /* never present faster than ~30 fps: steadier picture and sound on this device */

void sf3500_set_in_game(int in_game) { g_in_game = in_game; }
void sf3500_set_twin_stick(int on) { g_twin = on != 0; }
void sf3500_set_enhanced(int on) { g_enh = on != 0; }

static bool g_cap_from_env = false;
void sf3500_set_frame_cap(int fps)
{
    if (g_cap_from_env) return; /* FORMIDO_FRAME_MS (test builds and experiments) wins */
    if (fps < 10) fps = 30;
    if (fps > 60) fps = 60;
    g_frame_ms = 1000 / fps;
}

#ifdef FORMIDO_TESTHOOKS
static void test_shot(SDL_Surface *s);
#endif

int sf3500_flip(SDL_Surface *s)
{
    audio_update();
    if (!s || s != SDL_GetVideoSurface()) return 0; /* off-screen surface: nothing to show */
#ifdef FORMIDO_TESTHOOKS
    test_shot(s);
#endif
    if (g_hw) {
        long long t = now_ms();
        long long wait = g_last_present_ms + g_frame_ms - t;
        if (wait > 0 && wait <= g_frame_ms) usleep((useconds_t)(wait * 1000));
        g_last_present_ms = now_ms();
        hwdisp_present(s->pixels, s->w, s->h, s->pitch);
    }
    return 0;
}

enum Ctx {
    C_ALWAYS,
    C_MATCH,    /* in a match, either scheme */
    C_MENU,     /* anywhere but a match */
    C_NOTWIN,   /* not (a match with the twin-stick scheme) */
    C_TWIN,     /* a match with the twin-stick scheme, classic game (R1 fires along the aim) */
    C_ENH       /* a match of the enhanced game (R1 deploys the bunker) */
};

struct KeyMap {
    int jk;
    SDLKey sym;
    Ctx ctx;
    int repeat; /* 1: pressing again while held (menus, name entry), like a keyboard's key repeat */
};

/* Buttons -> keys.
 *  Menus and name entry: A selects (Return), B backs out (Escape), Start selects.
 *  Tank scheme (original controls): D-pad drives, A shoots, B is Special.
 *  Twin-stick scheme: the D-pad (left stick) walks. The right stick of these handhelds is wired to the
 *  A/B/X/Y buttons, so holding L1 turns the face buttons into aim-and-fire directions (X up, A right, B down,
 *  Y left, read by the game as i/l/k/j; two together aim diagonally). Without L1, A fires along the current
 *  aim, B is Special, and R1 also fires along the aim. See face_update() below.
 *  Both: Start pauses, Select ends the match (Escape). */
static const KeyMap g_map[] = {
    { JK_UP,     SDLK_UP,     C_ALWAYS, 1 },
    { JK_DOWN,   SDLK_DOWN,   C_ALWAYS, 1 },
    { JK_LEFT,   SDLK_LEFT,   C_ALWAYS, 1 },
    { JK_RIGHT,  SDLK_RIGHT,  C_ALWAYS, 1 },
    { JK_A,      SDLK_RETURN, C_ALWAYS, 0 },
    { JK_A,      SDLK_RCTRL,  C_NOTWIN, 0 },
    { JK_B,      SDLK_RSHIFT, C_NOTWIN, 0 },
    { JK_B,      SDLK_ESCAPE, C_MENU,   0 },
    { JK_R1,     SDLK_RCTRL,  C_TWIN,   0 },
    { JK_R1,     SDLK_r,      C_ENH,    0 },
    { JK_R2,     SDLK_t,      C_ENH,    0 },
    { JK_L2,     SDLK_F1,     C_ENH,    0 },
    { JK_START,  SDLK_p,      C_MATCH,  0 },
    { JK_START,  SDLK_RETURN, C_MENU,   0 },
    { JK_SELECT, SDLK_ESCAPE, C_ALWAYS, 0 },
};

static void push_key(SDLKey sym, bool down)
{
#ifdef FORMIDO_TESTHOOKS
    fprintf(stderr, "key %s %d (in_game=%d twin=%d) t=%lld\n", down ? "down" : "up  ", (int)sym, g_in_game, (int)g_twin, now_ms());
#endif
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.type = ev.type;
    ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.sym = sym;
    SDL_PushEvent(&ev);
}

static bool g_down[sizeof(g_map) / sizeof(g_map[0])];
static long long g_next_repeat[sizeof(g_map) / sizeof(g_map[0])];
static bool g_quit_pending = false;
int sf3500_quit_requested(void) { return g_quit_pending ? 1 : 0; }
static long long g_last_quit_ms = 0;

/* Twin-stick face buttons. Each press takes its role when it starts: with L1 held it is an aim button
 * until released, without L1 it is an action button until released (so letting go of L1 while the right
 * stick is still pushed does not turn the aim into a fire or a throw). */
static bool g_face_down[4];
static SDLKey g_face_key[4];

static void face_update(unsigned cur, bool twin)
{
    static const int jk[4] = { JK_A, JK_B, JK_X, JK_Y };
    static const SDLKey aim[4] = { SDLK_l, SDLK_k, SDLK_i, SDLK_j };          /* right, down, up, left */
    static const SDLKey act[4] = { SDLK_RCTRL, SDLK_RSHIFT, SDLK_UNKNOWN, SDLK_UNKNOWN }; /* fire, special */
    bool l1 = (cur & (1u << JK_L1)) != 0;
    for (int i = 0; i < 4; i++) {
        bool down = twin && (cur & (1u << jk[i])) != 0;
        if (down && !g_face_down[i]) {
            g_face_down[i] = true;
            g_face_key[i] = l1 ? aim[i] : act[i];
            if (g_face_key[i] != SDLK_UNKNOWN) push_key(g_face_key[i], true);
        } else if (!down && g_face_down[i]) {
            g_face_down[i] = false;
            if (g_face_key[i] != SDLK_UNKNOWN) push_key(g_face_key[i], false);
            g_face_key[i] = SDLK_UNKNOWN;
        }
    }
}

#ifdef FORMIDO_TESTHOOKS
/* Test builds only. FORMIDO_PAD="ms:mask,ms:mask,..." scripts the gamepad (mask bits as JK_* in
 * joykey.hpp: Left 1, Right 2, Up 4, Down 8, A 0x10, B 0x20, L1 0x40, R1 0x80, X 0x100, Y 0x200,
 * Start 0x400, Select 0x800; a mask holds until the next entry). FORMIDO_SHOT="ms,ms,..." saves
 * the screen as shot_<ms>.bmp at those times. Times run from the first poll. */
static unsigned scripted_pad(void)
{
    static long long t0 = 0;
    static std::vector<std::pair<long long, unsigned> > plan;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        t0 = now_ms();
        const char *e = getenv("FORMIDO_PAD");
        while (e && *e) {
            long long ms = strtoll(e, (char **)&e, 10);
            if (*e == ':') e++;
            unsigned m = (unsigned)strtoul(e, (char **)&e, 0);
            plan.push_back(std::make_pair(ms, m));
            if (*e == ',') e++;
        }
    }
    unsigned cur = 0;
    long long t = now_ms() - t0;
    for (size_t i = 0; i < plan.size(); i++)
        if (plan[i].first <= t) cur = plan[i].second;
    return cur;
}

static void test_shot(SDL_Surface *s)
{
    static long long t0 = 0;
    static std::vector<long long> shots;
    static size_t next = 0;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        t0 = now_ms();
        const char *e = getenv("FORMIDO_SHOT");
        while (e && *e) {
            shots.push_back(strtoll(e, (char **)&e, 10));
            if (*e == ',') e++;
        }
    }
    if (next < shots.size() && now_ms() - t0 >= shots[next]) {
        char name[64];
        snprintf(name, sizeof(name), "shot_%lld.bmp", shots[next]);
        SDL_SaveBMP(s, name);
        next++;
    }
}
#endif

#ifdef FORMIDO_TESTHOOKS
unsigned enhanced_bot_buttons(void);			/* the bot player (src/enhanced.cpp), test builds only */
#endif

static void pad_update(void)
{
#ifdef FORMIDO_TESTHOOKS
    unsigned cur = getenv("FORMIDO_BOT") ? enhanced_bot_buttons() : (getenv("FORMIDO_PAD") ? scripted_pad() : joykey_read());
#else
    unsigned cur = joykey_read();
#endif

    /* Select + Start together: leave the game from anywhere. From then on no other button counts: Start would
     * otherwise also arrive as Return (starting a game from the menu) and Select as Escape. */
    if ((cur & (1u << JK_SELECT)) && (cur & (1u << JK_START))) g_quit_pending = true;
    if (g_quit_pending) cur = 0;

    for (unsigned i = 0; i < sizeof(g_map) / sizeof(g_map[0]); i++) {
        const KeyMap &m = g_map[i];
        bool active = (cur & (1u << m.jk)) != 0;
        bool twin = g_in_game && g_twin;
        switch (m.ctx) {
        case C_MATCH:  if (!g_in_game) active = false; break;
        case C_MENU:   if (g_in_game) active = false; break;
        case C_NOTWIN: if (twin) active = false; break;
        case C_TWIN:   if (!twin || g_enh) active = false; break;
        case C_ENH:    if (!(g_in_game && g_enh)) active = false; break;
        default: break;
        }
        if (active != g_down[i]) {
            g_down[i] = active;
            push_key(m.sym, active);
            g_next_repeat[i] = now_ms() + 400;
        } else if (active && m.repeat && now_ms() >= g_next_repeat[i]) {
            push_key(m.sym, true);
            g_next_repeat[i] = now_ms() + 130;
        }
    }

    face_update(cur, g_in_game && g_twin);
}

int sf3500_poll(SDL_Event *e)
{
    if (g_inited) pad_update();
    audio_update();
    if (SDL_PollEvent(e))
        return 1;
    if (g_quit_pending) {
        /* One SDL_QUIT every 30 ms until the game has actually exited, so each screen the game passes through (match,
         * name entry, menu) sees it. It is handed out only once the queue is empty: the game loops overwrite their
         * "done" flag with every event, so a quit followed by a key event would be lost. */
        long long t = now_ms();
        if (t - g_last_quit_ms >= 30) {
            g_last_quit_ms = t;
            memset(e, 0, sizeof(*e));
            e->type = SDL_QUIT;
            return 1;
        }
    }
    return 0;
}

unsigned sf3500_buttons(void)
{
#ifdef FORMIDO_TESTHOOKS
    return getenv("FORMIDO_PAD") ? scripted_pad() : joykey_read();
#else
    return joykey_read();
#endif
}

void sf3500_shutdown(void)
{
    if (!g_inited) return;
    g_inited = false;
    Mix_CloseAudio();
    if (g_hw) hwdisp_deinit();
    g_hw = false;
}

void sf3500_init(void)
{
    if (g_inited) return;
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_NOMOUSE", "1", 1);
    const char *fc = getenv("FORMIDO_FRAME_MS");
    if (fc && atoi(fc) >= 0) { g_frame_ms = atoi(fc); g_cap_from_env = true; }

    g_hw = (hwdisp_init() == 0);
    if (!g_hw) fprintf(stderr, "formido: display driver not available, nothing will be shown\n");
    joykey_init();
    g_inited = true;
    atexit(sf3500_shutdown);
}
