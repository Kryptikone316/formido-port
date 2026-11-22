// SPDX-License-Identifier: GPL-2.0-or-later
/*
    Formido - Datafrog SF3500 port: tuning knobs (see include/tuning.h).
*/

#ifdef SF3500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tuning.h"

TUNING tune;

struct KNOB {
	const char *name;
	float *var;
	float def, lo, hi;
	const char *sec, *desc;
};

static KNOB knobs[] = {
#define X(name, def, lo, hi, sec, desc) { #name, &tune.name, def, lo, hi, sec, desc },
	TUNING_LIST(X)
#undef X
};
static const int NUM_KNOBS = sizeof(knobs) / sizeof(knobs[0]);

static void set_defaults() {
	for(int i=0; i<NUM_KNOBS; i++)
		*knobs[i].var = knobs[i].def;
}

static void write_knob(FILE *f, const KNOB &k, float value) {
	fprintf(f, "# %s (default %g, from %g to %g)\n%s = %g\n\n", k.desc, k.def, k.lo, k.hi, k.name, value);
}

static void write_default_file(const char *path) {
	FILE *f = fopen(path, "wt");
	if(!f) {
		fprintf(stderr, "tuning: cannot write %s\n", path);
		return;
	}
	fprintf(f, "# Formido (SF3500): tuning of the enhanced game.\n"
		"#\n"
		"# Change a number and start a new enhanced game to try it; the classic game is not affected.\n"
		"# Lines starting with # are comments. A value outside its range is brought back into it.\n"
		"# Delete this file to get a fresh copy with the defaults. Knobs added by a newer version are appended\n"
		"# at the end of the file without changing what is already there.\n");
	const char *sec = "";
	for(int i=0; i<NUM_KNOBS; i++) {
		if(strcmp(sec, knobs[i].sec) != 0) {
			sec = knobs[i].sec;
			fprintf(f, "\n\n########## %s ##########\n\n", sec);
		}
		write_knob(f, knobs[i], knobs[i].def);
	}
	fclose(f);
}

static char *trim(char *s) {
	while(*s && isspace((unsigned char)*s))
		s++;
	char *e = s + strlen(s);
	while(e > s && isspace((unsigned char)e[-1]))
		*--e = '\0';
	return s;
}

void tune_load(const char *path) {
	set_defaults();

	FILE *f = fopen(path, "rt");
	if(!f) {
		write_default_file(path);
		f = fopen(path, "rt");
		if(!f) {
			fprintf(stderr, "tuning: using the built-in defaults\n");
			return;
		}
	}

	bool seen[NUM_KNOBS];
	for(int i=0; i<NUM_KNOBS; i++)
		seen[i] = false;
	int read = 0, line_no = 0;
	char line[512];
	while(fgets(line, sizeof(line), f)) {
		line_no++;
		char *hash = strchr(line, '#');
		if(hash)
			*hash = '\0';
		char *t = trim(line);
		if(!*t)
			continue;
		char *eq = strchr(t, '=');
		if(!eq) {
			fprintf(stderr, "tuning: %s line %d: expected name = value\n", path, line_no);
			continue;
		}
		*eq = '\0';
		char *name = trim(t);
		char *val = trim(eq + 1);
		char *end = NULL;
		float v = (float)strtod(val, &end);
		if(end == val) {
			fprintf(stderr, "tuning: %s line %d: '%s' is not a number\n", path, line_no, val);
			continue;
		}
		int idx = -1;
		for(int i=0; i<NUM_KNOBS; i++)
			if(strcmp(knobs[i].name, name) == 0) { idx = i; break; }
		if(idx < 0) {
			fprintf(stderr, "tuning: %s line %d: unknown knob '%s'\n", path, line_no, name);
			continue;
		}
		if(v < knobs[idx].lo || v > knobs[idx].hi) {
			float c = v < knobs[idx].lo ? knobs[idx].lo : knobs[idx].hi;
			fprintf(stderr, "tuning: %s = %g is outside %g..%g, using %g\n", name, v, knobs[idx].lo, knobs[idx].hi, c);
			v = c;
		}
		*knobs[idx].var = v;
		seen[idx] = true;
		read++;
	}
	fclose(f);

	// knobs this file does not know yet (added by a newer version): appended, with the default
	int missing = 0;
	for(int i=0; i<NUM_KNOBS; i++)
		if(!seen[i])
			missing++;
	if(missing > 0) {
		FILE *a = fopen(path, "at");
		if(a) {
			fprintf(a, "\n\n########## added by a newer version ##########\n\n");
			for(int i=0; i<NUM_KNOBS; i++)
				if(!seen[i])
					write_knob(a, knobs[i], knobs[i].def);
			fclose(a);
		}
	}

	// keep ranges that make sense
	if(tune.garrison_max < tune.garrison_min)
		tune.garrison_max = tune.garrison_min;
	if(tune.runner_pack_max < tune.runner_pack_min)
		tune.runner_pack_max = tune.runner_pack_min;
	if(tune.burrower_max_seconds < tune.burrower_min_seconds)
		tune.burrower_max_seconds = tune.burrower_min_seconds;
	if(tune.ammo_max_seconds < tune.ammo_min_seconds)
		tune.ammo_max_seconds = tune.ammo_min_seconds;
	if(tune.plant_max_seconds < tune.plant_min_seconds)
		tune.plant_max_seconds = tune.plant_min_seconds;
	if(tune.cell_respawn_max_seconds < tune.cell_respawn_min_seconds)
		tune.cell_respawn_max_seconds = tune.cell_respawn_min_seconds;

	fprintf(stderr, "tuning: %d values read from %s%s\n", read, path, missing ? " (new knobs appended)" : "");
}

#endif
