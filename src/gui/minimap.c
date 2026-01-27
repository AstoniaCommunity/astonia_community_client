/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * Minimap
 *
 *
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL3/SDL.h>

#include "astonia.h"
#include "gui/gui.h"
#include "gui/gui_private.h"
#include "client/client.h"
#include "game/game.h"
#include "sdl/sdl.h"
#include "lib/cjson/cJSON.h"

#define MINIMAP           40
#define MAXMAP            256
#define IRGBA(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 0))

#define MAPPIX_UNKNOWN 0
#define MAPPIX_BLOCK   1
#define MAPPIX_FSPRITE 2
#define MAPPIX_CHAR    3
#define MAPPIX_EMPTY   4
#define MAPPIX_USE     5

static int sx, sy, visible, mx, my, update1, update2, update3, orx, ory, rewrite_cnt;

static unsigned char _mmap[MAXMAP * MAXMAP];
static unsigned short map_poi_idx[MAXMAP * MAXMAP];

static uint32_t mapix1[MAXMAP * MAXMAP];
static uint32_t mapix2[MINIMAP * MINIMAP * 4];

#define MAXSAVEMAP 100
static int mapnr = -1;

static int map_managed = 0; // map managed 0 = we're guessing. map managed 1 = the server will send us area changes.
static int map_area = 0;
static int map_server = 0;

static int map_poi_load(void);
static void map_update_poi(void);
static uint32_t map_poi_col(int x, int y);

SDL_Texture *maptex1 = NULL, *maptex2 = NULL;

void minimap_init(void)
{
	if (game_options & GO_NOMAP) {
		return;
	}

	sx = dotx(DOT_MBR) - MAXMAP - 6;
	sy = doty(DOT_MTL) + 6;

	mx = dotx(DOT_MBR) - MINIMAP * 2 - 6;
	my = doty(DOT_MTL) + 6;

	memset(_mmap, 0, sizeof(_mmap));
	visible = 1;
	update1 = update2 = update3 = 1;

	maptex1 = sdl_create_texture(MAXMAP, MAXMAP);
	maptex2 = sdl_create_texture(MINIMAP * 2, MINIMAP * 2);

	SDL_SetTextureBlendMode(maptex1, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(maptex2, SDL_BLENDMODE_BLEND);

	SDL_SetTextureScaleMode(maptex1, SDL_SCALEMODE_NEAREST);
	SDL_SetTextureScaleMode(maptex2, SDL_SCALEMODE_NEAREST);
}

static void set_pix(int x, int y, unsigned char val)
{
	unsigned char val2;

	if ((val2 = _mmap[x + y * MAXMAP]) != val) {
		// count how much of the map has changed permanently (not counting characters
		// and formerly unknown tiles or swapping between sightblocks and fsprites)
		if (val2 != MAPPIX_UNKNOWN && val2 != MAPPIX_CHAR && val != MAPPIX_CHAR &&
		    !((val == MAPPIX_BLOCK && val2 == MAPPIX_FSPRITE) || (val == MAPPIX_FSPRITE && val2 == MAPPIX_BLOCK))) {
			// note("changed: %d to %d (%d,%d)",_mmap[x+y*MAXMAP],val,x,y);
			rewrite_cnt++;
		}

		_mmap[x + y * MAXMAP] = val;
		update1 = update2 = update3 = 1;
	}
}

static void map_save(void);
static int map_load(void);

void minimap_update(void)
{
	int x, y, xs, xe, ox, oy;

	if (game_options & GO_NOMAP) {
		return;
	}

	ox = (int)originx - (int)DIST;
	oy = (int)originy - (int)DIST;

	rewrite_cnt = 0;
	for (y = 1; y < (int)DIST * 2; y++) {
		if (y + oy < 0) {
			continue;
		}
		if (y + oy >= MAXMAP) {
			continue;
		}

		if (y < (int)DIST) {
			xs = (int)DIST - y;
			xe = (int)DIST + y;
		} else {
			xs = y - (int)DIST;
			xe = (int)DIST * 3 - y;
		}

		for (x = xs + 1; x < xe; x++) {
			if (x + ox < 0) {
				continue;
			}
			if (x + ox >= MAXMAP) {
				continue;
			}
			map_index_t mn = mapmn((unsigned int)x, (unsigned int)y);
			if (!(map[mn].flags & CMF_VISIBLE)) {
				continue;
			}

			if (map[mn].mmf & MMF_SIGHTBLOCK) {
				if (map[mn].flags & CMF_USE) {
					set_pix(ox + x, oy + y, MAPPIX_USE);
				} else {
					set_pix(ox + x, oy + y, MAPPIX_BLOCK);
				}
			} else if (map[mn].fsprite) {
				set_pix(ox + x, oy + y, MAPPIX_FSPRITE);
			} else if (map[mn].csprite && mn != (unsigned int)plrmn) {
				set_pix(ox + x, oy + y, MAPPIX_CHAR);
			} else {
				set_pix(ox + x, oy + y, MAPPIX_EMPTY);
			}
		}
	}
	if (rewrite_cnt > 8 && !map_managed) {
		memset(_mmap, 0, sizeof(_mmap));
		update1 = update2 = 1;
		note("MAP CHANGED: %d", rewrite_cnt);
	}
	if (mapnr == -1 && update3) {
		update3 = 0;
		if (!map_managed && (game_options & GO_MAPSAVE)) {
			mapnr = map_load();
		}
	}
}

static uint32_t pix_col(int x, int y)
{
	uint32_t c;

	switch (_mmap[x + y * MAXMAP]) {
	case MAPPIX_BLOCK:
		return IRGBA(180, 180, 180, 255);
	case MAPPIX_FSPRITE:
		return IRGBA(140, 140, 220, 255);
	case MAPPIX_CHAR:
		return IRGBA(60, 220, 60, 255);
	case MAPPIX_EMPTY:
		if ((c = map_poi_col(x, y))) {
			return c;
		}
		return IRGBA(60, 60, 60, 255);
	case MAPPIX_USE:
		return IRGBA(120, 80, 80, 255);
	case MAPPIX_UNKNOWN:
	default:
		if ((c = map_poi_col(x, y))) {
			return c;
		}
		return IRGBA(25, 25, 25, 255);
	}
}

static void draw_center(int x, int y)
{
	render_pixel(x, y, IRGB(31, 8, 8));
	render_pixel(x + 1, y, IRGB(31, 8, 8));
	render_pixel(x, y + 1, IRGB(31, 8, 8));
	render_pixel(x - 1, y, IRGB(31, 8, 8));
	render_pixel(x, y - 1, IRGB(31, 8, 8));
}

static void draw_center2(int x, int y)
{
	int i;

	render_pixel(x, y, IRGB(31, 8, 8));

	for (i = 0; i < 3; i++) {
		render_pixel(x + i, y, IRGB(31, 8, 8));
		render_pixel(x, y + i, IRGB(31, 8, 8));
		render_pixel(x - i, y, IRGB(31, 8, 8));
		render_pixel(x, y - i, IRGB(31, 8, 8));
	}
}

void display_minimap(void)
{
	int x, y, ix, iy, i;
	float dist;
	SDL_FRect dr, sr;

	if (game_options & GO_NOMAP) {
		return;
	}

	if (visible & 2) { // display big map
		if (update1) {
			for (y = 0; y < MAXMAP; y++) {
				for (x = 0; x < MAXMAP; x++) {
					mapix1[x + y * MAXMAP] = pix_col(x, y);
				}
			}
			SDL_UpdateTexture(maptex1, NULL, mapix1, MAXMAP * sizeof(uint32_t));
			update1 = 0;
		}

		dr.x = (float)((sx + x_offset) * sdl_scale);
		dr.w = (float)(MAXMAP * sdl_scale);
		dr.y = (float)((sy + y_offset) * sdl_scale);
		dr.h = (float)(MAXMAP * sdl_scale);

		if (visible & 1) {
			sr.x = 0.0f;
			sr.w = (float)MAXMAP;
			sr.y = 0.0f;
			sr.h = (float)MAXMAP;
			sdl_render_copy(maptex1, &sr, &dr);
			draw_center(sx + originx, sy + originy);
		} else {
			x = originx - MAXMAP / 6;
			y = originy - MAXMAP / 6;
			if (x < 0) {
				x = 0;
			}
			if (x > MAXMAP - MAXMAP / 3) {
				x = MAXMAP - MAXMAP / 3;
			}
			if (y < 0) {
				y = 0;
			}
			if (y > MAXMAP - MAXMAP / 3) {
				y = MAXMAP - MAXMAP / 3;
			}

			sr.x = (float)x;
			sr.w = (float)(MAXMAP / 3);
			sr.y = (float)y;
			sr.h = (float)(MAXMAP / 3);

			sdl_render_copy(maptex1, &sr, &dr);
			draw_center2(sx + (originx - x) * 3 + 2, sy + (originy - y) * 3 + 2);
		}

		render_line(sx, sy, sx, sy + MAXMAP, 0xffff);
		render_line(sx, sy + MAXMAP, sx + MAXMAP, sy + MAXMAP, 0xffff);
		render_line(sx + MAXMAP, sy + MAXMAP, sx + MAXMAP, sy, 0xffff);
		render_line(sx + MAXMAP, sy, sx, sy, 0xffff);

		render_text(sx + 6, sy + 6, 0xffff, 0, "N");
	}

	if (orx != originx || ory != originy) {
		update2 = 1;
		orx = originx;
		ory = originy;
	}

	if (visible == 1) {
		if (update2) {
			bzero(mapix2, sizeof(mapix2));
			for (iy = -MINIMAP; iy < MINIMAP; iy++) {
				for (ix = -MINIMAP; ix < MINIMAP; ix++) {
					dist = sqrtf((float)(ix * ix + iy * iy));
					if (dist > MINIMAP) {
						continue;
					}

					x = originx + ix;
					y = originy + iy;

					if (x < 0 || x >= MAXMAP || y < 0 || y >= MAXMAP) {
						mapix2[MINIMAP + ix + iy * MINIMAP * 2 + MINIMAP * MINIMAP * 2] = IRGBA(25, 25, 25, 255);
					} else {
						mapix2[MINIMAP + ix + iy * MINIMAP * 2 + MINIMAP * MINIMAP * 2] = pix_col(x, y);
					}
				}
			}
			SDL_UpdateTexture(maptex2, NULL, mapix2, MINIMAP * 2 * sizeof(uint32_t));
			update2 = 0;
		}

		dr.x = (float)((mx + x_offset) * sdl_scale);
		dr.w = (float)(MINIMAP * 2 * sdl_scale);
		dr.y = (float)((my + y_offset) * sdl_scale);
		dr.h = (float)(MINIMAP * 2 * sdl_scale);

		sr.x = 0.0f;
		sr.w = (float)(MINIMAP * 2);
		sr.y = 0.0f;
		sr.h = (float)(MINIMAP * 2);

		sdl_render_copy_ex(maptex2, &sr, &dr, 45.0);
		draw_center(mx + MINIMAP, my + MINIMAP);

		for (i = 0; i < sdl_scale; i++) {
			sdl_render_circle((mx + MINIMAP + x_offset) * sdl_scale, (my + MINIMAP + y_offset) * sdl_scale,
			    (MINIMAP)*sdl_scale + i, 0xffffffff);
		}
		render_text(mx + MINIMAP, my + 4, 0xffff, 0, "N");
	}
}

static void minimap_clearonly(void)
{
	memset(_mmap, 0, sizeof(_mmap));
	update1 = update2 = update3 = 1;
}

void minimap_clear(void)
{
	if (game_options & GO_MAPSAVE) {
		map_save();
	}
	mapnr = -1;
	map_area = 0;
	memset(_mmap, 0, sizeof(_mmap));
	update1 = update2 = update3 = 1;
}

void minimap_toggle(void)
{
	visible = (visible + 1) % 4;
}

void minimap_hide(void)
{
	if (visible) {
		visible = 1;
	}
}

static char *mapname(int i)
{
	static char filename[MAX_PATH];

	if (map_managed) {
		if (localdata) {
			sprintf(filename, "%smMap%d_%d.dat", localdata, map_server, map_area);
		} else {
			sprintf(filename, "bin/data/mMap%d_%d.dat", map_server, map_area);
		}
	} else {
		if (localdata) {
			sprintf(filename, "%smap%03d.dat", localdata, i);
		} else {
			sprintf(filename, "bin/data/map%03d.dat", i);
		}
	}

	return filename;
}

static void map_save_unmanaged(void)
{
	FILE *fp;
	int i, cnt;
	char *filename;

	for (i = cnt = 0; i < MAXMAP * MAXMAP; i++) {
		if (_mmap[i]) {
			cnt++;
		}
	}
	if (cnt < 250) {
		return;
	}

	// check if another client wrote the same map
	// in the meantime
	mapnr = map_load();

	// new map, find a save-slot
	if (mapnr == -1) {
		for (i = 0; i < MAXSAVEMAP; i++) {
			filename = mapname(i);
			fp = fopen(filename, "rb");
			if (!fp) {
				break;
			}
			fclose(fp);
		}
		if (i == MAXSAVEMAP) {
			warn("Area map storage full! Please use /compactmap to merge duplicate maps.");
			return;
		}
		mapnr = i;
	}

	filename = mapname(mapnr);
	// note("saving area map to %s",filename);
	fp = fopen(filename, "wb");
	if (fp) {
		fwrite(_mmap, sizeof(_mmap), 1, fp);
		fclose(fp);
	}
}

static void map_save_managed(void)
{
	FILE *fp;
	char *filename;

	if (!map_area) {
		return;
	}

	filename = mapname(42);
	note("saving area map to %s", filename);
	fp = fopen(filename, "wb");
	if (fp) {
		fwrite(_mmap, sizeof(_mmap), 1, fp);
		fclose(fp);
	}
}

static void map_save(void)
{
	if (map_managed) {
		map_save_managed();
	} else {
		map_save_unmanaged();
	}
}

static int map_compare(const unsigned char *tmap, const unsigned char *xmap)
{
	int i, hit, miss;

	for (i = hit = miss = 0; i < MAXMAP * MAXMAP; i++) {
		// sightblock, fsprite or usable sightblock
		if (tmap[i] == 1 || tmap[i] == 2 || tmap[i] == 5) {
			if (xmap[i] == 1 || xmap[i] == 2 || xmap[i] == 5) {
				hit++;
			} else if (xmap[i] != 0) {
				miss++;
			}
		}
		// empty or csprite
		if (tmap[i] == 3 || tmap[i] == 4) {
			if (xmap[i] == 3 || xmap[i] == 4) {
				hit++;
			} else if (xmap[i] != 0) {
				miss++;
			}
		}
	}
	if (hit < 200) {
		return 0;
	}
	if (miss > hit / 100) {
		return 0;
	}

	return hit;
}

static void map_merge(unsigned char *xmap, const unsigned char *tmap)
{
	int i;

	// only overwrite empty parts of the map with loaded data.
	for (i = 0; i < MAXMAP * MAXMAP; i++) {
		if (!xmap[i]) {
			if (tmap[i] == 3) {
				xmap[i] = 4; // do not load csprites, they move too much
			} else {
				xmap[i] = tmap[i];
			}
		}
	}
}

static int map_load_unmanaged(void)
{
	FILE *fp;
	int i, hit, besti = -1, besthit = 0;
	unsigned char tmap[MAXMAP * MAXMAP];
	char *filename;

	for (i = 0; i < MAXSAVEMAP; i++) {
		filename = mapname(i);
		fp = fopen(filename, "rb");
		if (!fp) {
			continue;
		}
		fread(tmap, sizeof(tmap), 1, fp);
		fclose(fp);

		if (!(hit = map_compare(tmap, _mmap))) {
			continue;
		}

		if (hit > besthit) {
			besti = i;
			besthit = hit;
		}
	}
	if (besti != -1) {
		filename = mapname(besti);
		// note("loading area map from %s (%d hits)",filename,besthit);
		fp = fopen(filename, "rb");
		if (!fp) {
			return -1;
		}
		fread(tmap, sizeof(tmap), 1, fp);
		fclose(fp);

		map_merge(_mmap, tmap);

		return besti;
	}

	return -1;
}

static int map_load_managed(void)
{
	FILE *fp;
	char *filename;

	if (!map_area) {
		return 0;
	}

	filename = mapname(42);
	note("loading area map from %s", filename);

	fp = fopen(filename, "rb");
	if (!fp) {
		return 1;
	}
	fread(_mmap, sizeof(_mmap), 1, fp);
	fclose(fp);

	return 1;
}

static int map_load(void)
{
	if (map_managed) {
		return map_load_managed();
	} else {
		return map_load_unmanaged();
	}
}

void minimap_compact(void)
{
	FILE *fp;
	int i, j;
	char *filename;
	unsigned char tmap[MAXMAP * MAXMAP], xmap[MAXMAP * MAXMAP];

	if (game_options & GO_NOMAP) {
		return;
	}

	for (i = 0; i < MAXSAVEMAP; i++) {
		filename = mapname(i);
		fp = fopen(filename, "rb");
		if (!fp) {
			continue;
		}
		fread(tmap, sizeof(tmap), 1, fp);
		fclose(fp);

		for (j = i + 1; j < MAXSAVEMAP; j++) {
			filename = mapname(j);
			fp = fopen(filename, "rb");
			if (!fp) {
				continue;
			}
			fread(xmap, sizeof(xmap), 1, fp);
			fclose(fp);

			if (map_compare(tmap, xmap)) {
				map_merge(tmap, xmap);
				filename = mapname(i);
				fp = fopen(filename, "wb");
				if (!fp) {
					continue;
				}
				fwrite(tmap, sizeof(tmap), 1, fp);
				fclose(fp);

				filename = mapname(j);
				remove(filename);
				note("merged map %d into map %d", j, i);
			}
		}
	}
}

void minimap_areainfo(int cmd, int areaID, int server_key)
{
	int cnt;

	map_managed = 1;

	if (!cmd) {
		if (map_area) {
			map_save();
		}

		map_area = areaID;
		map_server = server_key;

		map_load();
		cnt = map_poi_load();
		bzero(map_poi_idx, sizeof(map_poi_idx));
		if (cnt) {
			map_update_poi();
		}
	} else {
		minimap_clearonly();
	}
}

typedef struct {
	int x, y;
	int type;
	char *desc;
} MAP_POI;

MAP_POI *map_poi = NULL;
int map_poi_cnt = 0, map_poi_max = 0;

static int map_poi_parse(const char *json_str, const char *source_name)
{
	cJSON *root = cJSON_Parse(json_str);
	if (!root) {
		warn("map_poi: Failed to parse %s: %s", source_name, cJSON_GetErrorPtr());
		return -1;
	}

	cJSON *coords_arr = cJSON_GetObjectItem(root, "coords");
	if (!coords_arr || !cJSON_IsArray(coords_arr)) {
		warn("map_poi: Missing coords array in %s", source_name);
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

		cJSON *x = cJSON_GetObjectItem(item, "x");
		cJSON *y = cJSON_GetObjectItem(item, "y");
		cJSON *type = cJSON_GetObjectItem(item, "type");
		cJSON *desc = cJSON_GetObjectItem(item, "desc");
		if (!x || !y || !type || !desc || !cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(type) ||
		    !cJSON_IsString(desc)) {
			continue;
		}

		if (map_poi_cnt >= map_poi_max) {
			map_poi_max += 16;
			map_poi = xrealloc(map_poi, sizeof(MAP_POI) * (size_t)map_poi_max, MEM_GUI);
			if (!map_poi) {
				return -1;
			}
		}

		map_poi[map_poi_cnt].x = max(0, min(255, x->valueint));
		map_poi[map_poi_cnt].y = max(0, min(255, y->valueint));
		map_poi[map_poi_cnt].type = type->valueint;
		map_poi[map_poi_cnt].desc = xstrdup(desc->valuestring, MEM_GUI);
		map_poi_cnt++;

		if (map_poi_cnt > 60000) {
			break; // yeah. no one's gonna do that. right? right??
		}
	}

	cJSON_Delete(root);
	return map_poi_cnt;
}

static char *poimapname(void)
{
	static char filename[MAX_PATH];

	sprintf(filename, "res/config/map_poi%d_%d.json", map_server, map_area);

	return filename;
}

static int map_poi_load(void)
{
	char *path = poimapname();
	int loaded;

	for (int i = 1; i < map_poi_cnt; i++) {
		xfree(map_poi[i].desc);
	}
	map_poi_cnt = 1;

	if (!map_managed) {
		return 0;
	}

	char *json = load_ascii_file(path, MEM_TEMP);
	if (!json) {
		return 0;
	}

	loaded = map_poi_parse(json, "map_poi.json");
	note("loaded %d map POIs", loaded);

	return loaded;
}

static void map_update_poi(void)
{
	int i;
	int x, y, xoff, yoff;

	for (i = 1; i < map_poi_cnt; i++) {
		for (yoff = -2; yoff < 3; yoff++) {
			y = yoff + map_poi[i].y;
			if (y < 0 || y >= MAXMAP) {
				continue;
			}

			for (xoff = -2; xoff < 3; xoff++) {
				if (xoff == -2 && yoff == -2) {
					continue;
				}
				if (xoff == 2 && yoff == 2) {
					continue;
				}
				if (xoff == 2 && yoff == -2) {
					continue;
				}
				if (xoff == -2 && yoff == 2) {
					continue;
				}

				x = xoff + map_poi[i].x;
				if (x < 0 || x >= MAXMAP) {
					continue;
				}

				map_poi_idx[x + y * MAXMAP] = (unsigned short)i;
			}
		}
	}
}

static uint32_t map_poi_col(int x, int y)
{
	int i;

	if (x < 0 || y < 0 || x >= MAXMAP || y >= MAXMAP) {
		return 0;
	}

	if (!(i = map_poi_idx[x + y * MAXMAP])) {
		return 0;
	}

	if (map_poi[i].type == 2) {
		if (_mmap[map_poi[i].x + map_poi[i].y * MAXMAP] == MAPPIX_UNKNOWN) {
			return 0;
		}
	}

	if (_mmap[x + y * MAXMAP] != MAPPIX_UNKNOWN) {
		return IRGBA(64, 192, 64, 255);
	} else {
		return IRGBA(64, 128, 64, 255);
	}
}

void minimap_display_hover(int hx, int hy)
{
	int x, y, i;

	if (visible == 1) { // small, round map
		double sq = 0.70710678118654752440, dist;
		int tmp;

		x = hx - (mx + MINIMAP);
		y = hy - (my + MINIMAP);

		dist = sqrt((double)(x * x + y * y));
		if (dist > MINIMAP) {
			return;
		}

		// Apply clockwise rotation matrix
		tmp = (int)(round(x * sq + y * sq));
		y = (int)(round(-x * sq + y * sq));
		x = tmp;

		x += originx;
		y += originy;
	} else if (visible == 2) { // big scaled up map
		int ox, oy;

		ox = originx - MAXMAP / 6;
		oy = originy - MAXMAP / 6;
		if (ox < 0) {
			ox = 0;
		}
		if (ox > MAXMAP - MAXMAP / 3) {
			ox = MAXMAP - MAXMAP / 3;
		}
		if (oy < 0) {
			oy = 0;
		}
		if (oy > MAXMAP - MAXMAP / 3) {
			oy = MAXMAP - MAXMAP / 3;
		}

		x = hx - sx;
		y = hy - sy;

		x = x / 3 + ox;
		y = y / 3 + oy;

	} else if (visible == 3) { // big full map
		x = hx - sx;
		y = hy - sy;
	} else {
		return;
	}

	if (x < 0 || x >= MAXMAP) {
		return;
	}
	if (y < 0 || y >= MAXMAP) {
		return;
	}

	for (i = 1; i < map_poi_cnt; i++) {
		if (abs(map_poi[i].x - x) < 5 && abs(map_poi[i].y - y) < 5) {
			if (map_poi[i].type == 2) {
				if (_mmap[map_poi[i].x + map_poi[i].y * MAXMAP] == MAPPIX_UNKNOWN) {
					continue;
				}
			}
			int width = 100;
			int height;

			width = render_text_length(0, map_poi[i].desc);
			if (width > 100) {
				width = 100;
				height = render_text_break_length(0, 0, width, 0xffff, 0, map_poi[i].desc) + 8;
			} else {
				height = 18;
			}

			if (hx + width >= dotx(DOT_BR) - 4) {
				hx = dotx(DOT_BR) - width - 4;
				hy += 8;
			}

			render_shaded_rect(hx, hy, hx + width + 8, hy + height, 0x0000, 150);
			render_text_break(hx + 4, hy + 4, hx + width + 4, 0xffff, 0, map_poi[i].desc);
			break;
		}
	}
}
