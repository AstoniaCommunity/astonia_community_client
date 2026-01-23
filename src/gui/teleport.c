/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * Display Teleport Window and Helpers
 *
 * Display the teleport window, and maps mouse clicks
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "astonia.h"
#include "gui/gui.h"
#include "gui/gui_private.h"
#include "client/client.h"
#include "game/game.h"
#include "lib/cjson/cJSON.h"

int teleporter = 0;

// Forward declaration
DLL_EXPORT void set_teleport(int idx, int x, int y);

typedef struct {
	int x, y;
} coords;

#define MAXTELE   64
#define MAXMIRROR 26

static coords tele[MAXTELE];

static coords mirror_pos[MAXMIRROR] = {{346, 210}, {346, 222}, {346, 234}, {346, 246}, {346, 258}, {346, 270},
    {346, 282}, {346, 294}, {384, 210}, {384, 222}, {384, 234}, {384, 246}, {384, 258}, {384, 270}, {384, 282},
    {384, 294}, {429, 210}, {429, 222}, {429, 234}, {429, 246}, {429, 258}, {429, 270}, {429, 282}, {429, 294},
    {469, 210}, {469, 222}};

int clan_offset = 0;

static int teleport_parse_coords(const char *json_str, const char *source_name, coords *tele, size_t tele_count)
{
	int loaded = 0;
	cJSON *root = cJSON_Parse(json_str);
	if (!root) {
		warn("teleport: Failed to parse %s: %s", source_name, cJSON_GetErrorPtr());
		return -1;
	}

	cJSON *coords_arr = cJSON_GetObjectItem(root, "coords");
	if (!coords_arr || !cJSON_IsArray(coords_arr)) {
		warn("teleport: Missing coords array in %s", source_name);
		cJSON_Delete(root);
		return -1;
	}

	int count = cJSON_GetArraySize(coords_arr);
	int i;
	for (i = 0; i < count; i++) {
		cJSON *item = cJSON_GetArrayItem(coords_arr, i);
		if (!item || !cJSON_IsObject(item)) {
			continue;
		}

		cJSON *idx = cJSON_GetObjectItem(item, "idx");
		cJSON *x = cJSON_GetObjectItem(item, "x");
		cJSON *y = cJSON_GetObjectItem(item, "y");
		if (!idx || !x || !y || !cJSON_IsNumber(idx) || !cJSON_IsNumber(x) || !cJSON_IsNumber(y)) {
			continue;
		}

		int index = idx->valueint;
		if (index < 0 || (size_t)index >= tele_count) {
			continue;
		}

		tele[index].x = x->valueint;
		tele[index].y = y->valueint;
		loaded++;
	}

	cJSON_Delete(root);
	return loaded;
}

void teleport_init(void)
{
	const char *path = sv_ver == 35 ? "res/config/teleport_coords_v35.json" : "res/config/teleport_coords_v3.json";

	memset(tele, 0, sizeof(tele));

	char *json = load_ascii_file(path, MEM_TEMP);
	if (!json) {
		warn("teleport: Failed to read %s", path);
		return;
	}

	int count = teleport_parse_coords(json, path, tele, MAXTELE);
	xfree(json);

	if (count <= 0) {
		warn("teleport: No coords loaded from %s", path);
	}
}

DLL_EXPORT void set_teleport(int idx, int x, int y)
{
	if (idx < 0 || idx >= MAXTELE) {
		return;
	}

	tele[idx].x = x;
	tele[idx].y = y;
}

int get_teleport(int x, int y)
{
	int n;

	if (!teleporter) {
		return -1;
	}

	// map teleports
	for (n = 0; n < MAXTELE; n++) {
		if (!tele[n].x) {
			break;
		}
		if (tele[n].x == -1) {
			continue;
		}
		if (!may_teleport[n]) {
			continue;
		}

		if (abs(tele[n].x + dotx(DOT_TEL) - x) < 8 && abs(tele[n].y + doty(DOT_TEL) - y) < 8) {
			return n;
		}
	}

	// clan teleports
	for (n = 0; n < 8; n++) {
		if (sv_ver == 30 && !may_teleport[n + 64 + clan_offset]) {
			continue;
		}

		if (abs(dotx(DOT_TEL) + 337 - x) < 8 && abs(doty(DOT_TEL) + 24 + n * 12 - y) < 8) {
			return n + 64;
		}
	}
	for (n = 0; n < 8; n++) {
		if (sv_ver == 30 && 8 + clan_offset + n == 31) {
			continue;
		}
		if (sv_ver == 35 && 8 + clan_offset + n >= 60) {
			continue;
		}

		if (sv_ver == 30 && !may_teleport[n + 64 + 8 + clan_offset]) {
			continue;
		}

		if (abs(dotx(DOT_TEL) + 389 - x) < 8 && abs(doty(DOT_TEL) + 24 + n * 12 - y) < 8) {
			return n + 64 + 8;
		}
	}

	// mirror selector
	for (n = 0; n < MAXMIRROR; n++) {
		if (abs(mirror_pos[n].x + dotx(DOT_TEL) - x) < 8 && abs(mirror_pos[n].y + doty(DOT_TEL) - y) < 8) {
			if (sv_ver == 35) {
				return n + 201;
			} else {
				return n + 101;
			}
		}
	}

	if (abs(389 + dotx(DOT_TEL) - x) < 8 && abs(24 + 8 * 12 + doty(DOT_TEL) - y) < 8) {
		return 1042;
	}

	return -1;
}

void display_teleport(void)
{
	int n;

	if (!teleporter) {
		return;
	}

	if (sv_ver == 35) {
		render_sprite(53539, dotx(DOT_TEL) + 520 / 2, doty(DOT_TEL) + 320 / 2, 14, 0);
		if (clan_offset < 16)
			;
		else if (clan_offset < 32) {
			render_sprite(53521, dotx(DOT_TEL) + 102 / 2 + 341, doty(DOT_TEL) + 95 / 2 + 17, 14, 0);
		} else if (clan_offset < 48) {
			render_sprite(53522, dotx(DOT_TEL) + 102 / 2 + 341, doty(DOT_TEL) + 95 / 2 + 17, 14, 0);
		} else {
			render_sprite(53523, dotx(DOT_TEL) + 102 / 2 + 341, doty(DOT_TEL) + 95 / 2 + 17, 14, 0);
		}
	} else {
		if (!clan_offset) {
			render_sprite(53519, dotx(DOT_TEL) + 520 / 2, doty(DOT_TEL) + 320 / 2, 14, 0);
		} else {
			render_sprite(53520, dotx(DOT_TEL) + 520 / 2, doty(DOT_TEL) + 320 / 2, 14, 0);
		}
	}

	for (n = 0; n < MAXTELE; n++) {
		if (!tele[n].x) {
			break;
		}
		if (tele[n].x == -1) {
			continue;
		}

		if (!may_teleport[n]) {
			dx_copysprite_emerald(tele[n].x + dotx(DOT_TEL), tele[n].y + doty(DOT_TEL), 2, 0);
		} else if (telsel == n) {
			dx_copysprite_emerald(tele[n].x + dotx(DOT_TEL), tele[n].y + doty(DOT_TEL), 2, 2);
		} else {
			dx_copysprite_emerald(tele[n].x + dotx(DOT_TEL), tele[n].y + doty(DOT_TEL), 2, 1);
		}
	}

	for (n = 0; n < 8; n++) {
		if (sv_ver == 30 && !may_teleport[n + 64 + clan_offset]) {
			dx_copysprite_emerald(337 + dotx(DOT_TEL), 24 + n * 12 + doty(DOT_TEL), 3, 0);
		} else if (telsel == n + 64) {
			dx_copysprite_emerald(337 + dotx(DOT_TEL), 24 + n * 12 + doty(DOT_TEL), 3, 2);
		} else {
			dx_copysprite_emerald(337 + dotx(DOT_TEL), 24 + n * 12 + doty(DOT_TEL), 3, 1);
		}
	}

	for (n = 0; n < 8; n++) {
		if (sv_ver == 30 && 8 + clan_offset + n == 31) {
			continue;
		}
		if (sv_ver == 35 && 8 + clan_offset + n >= 60) {
			continue;
		}
		if (sv_ver == 30 && !may_teleport[n + 64 + 8 + clan_offset]) {
			dx_copysprite_emerald(389 + dotx(DOT_TEL), 24 + n * 12 + doty(DOT_TEL), 3, 0);
		} else if (telsel == n + 64 + 8) {
			dx_copysprite_emerald(389 + dotx(DOT_TEL), 24 + n * 12 + doty(DOT_TEL), 3, 2);
		} else {
			dx_copysprite_emerald(389 + dotx(DOT_TEL), 24 + n * 12 + doty(DOT_TEL), 3, 1);
		}
	}

	for (n = 0; n < MAXMIRROR; n++) {
		if ((sv_ver == 30 && telsel == n + 101) || (sv_ver == 35 && telsel == n + 201)) {
			dx_copysprite_emerald(mirror_pos[n].x + dotx(DOT_TEL), mirror_pos[n].y + doty(DOT_TEL), 1, 2);
		} else if (newmirror == (unsigned int)(n + 1)) {
			dx_copysprite_emerald(mirror_pos[n].x + dotx(DOT_TEL), mirror_pos[n].y + doty(DOT_TEL), 1, 1);
		} else {
			dx_copysprite_emerald(mirror_pos[n].x + dotx(DOT_TEL), mirror_pos[n].y + doty(DOT_TEL), 1, 0);
		}
	}

	if (telsel == 1042) {
		dx_copysprite_emerald(389 + dotx(DOT_TEL), 24 + 8 * 12 + doty(DOT_TEL), 2, 2);
	} else {
		dx_copysprite_emerald(389 + dotx(DOT_TEL), 24 + 8 * 12 + doty(DOT_TEL), 2, 1);
	}
}
