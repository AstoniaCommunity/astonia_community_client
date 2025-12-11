/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * Graphical User Interface - Map coordinate functions
 *
 */

#include <inttypes.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "astonia.h"
#include "gui/gui.h"
#include "gui/gui_private.h"
#include "client/client.h"
#include "game/game.h"
#include "sdl/sdl.h"
#include "modder/modder.h"

void set_mapoff(int cx, int cy, int mdx, int mdy)
{
	mapoffx = (cx - (mdx / 2 - mdy / 2) * (FDX / 2));
	mapoffy = (cy - (mdx / 2 + mdy / 2) * (FDY / 2));
}

void set_mapadd(int addx, int addy)
{
	mapaddx = addx;
	mapaddy = addy;
}

void mtos(unsigned int mapx, unsigned int mapy, int *scrx, int *scry)
{
	*scrx = (mapoffx + mapaddx) + ((int)mapx - (int)mapy) * (FDX / 2);
	*scry = (mapoffy + mapaddy) + ((int)mapx + (int)mapy) * (FDY / 2);
}

int stom(int scrx, int scry, int *mapx, int *mapy)
{
	if (scrx < dotx(DOT_MTL) || scrx >= dotx(DOT_MBR) || scry < doty(DOT_MTL) || scry >= doty(DOT_MBR)) {
		return 0;
	}

	scrx -= stom_off_x;
	scry -= stom_off_y;
	scrx -= (mapoffx + mapaddx);
	scry -= (mapoffy + mapaddy) - 10;
	*mapy = (40 * scry - 20 * scrx - 1) / (20 * 40); // ??? -1 ???
	*mapx = (40 * scry + 20 * scrx) / (20 * 40);

	return 1;
}

DLL_EXPORT size_t get_near_ground(int x, int y)
{
	int mapx, mapy;
	unsigned int ux, uy;

	if (!stom(x, y, &mapx, &mapy)) {
		return MAXMN;
	}

	if (mapx < 0 || mapy < 0 || (unsigned int)mapx >= MAPDX || (unsigned int)mapy >= MAPDY) {
		return MAXMN;
	}

	ux = (unsigned int)mapx;
	uy = (unsigned int)mapy;
	return mapmn(ux, uy);
}

DLL_EXPORT map_index_t get_near_item(int x, int y, unsigned int flag, unsigned int looksize)
{
	int mapx, mapy, scrx, scry;
	unsigned int ux, uy, sx, sy, ex, ey, mapx_u, mapy_u;
	map_index_t mn, nearest = MAXMN;
	double dist, nearestdist = 100000000;

	if (!stom(mousex, mousey, &mapx, &mapy)) {
		return MAXMN;
	}

	if (mapx < 0 || mapy < 0 || mapx >= (int)MAPDX || mapy >= (int)MAPDY) {
		return MAXMN;
	}

	ux = (unsigned int)mapx;
	uy = (unsigned int)mapy;

	sx = (ux > looksize) ? (ux - looksize) : 0U;
	sy = (uy > looksize) ? (uy - looksize) : 0U;
	ex = min(MAPDX - 1, ux + looksize);
	ey = min(MAPDY - 1, uy + looksize);

	for (mapy_u = sy; mapy_u <= ey; mapy_u++) {
		for (mapx_u = sx; mapx_u <= ex; mapx_u++) {
			mn = mapmn(mapx_u, mapy_u);

			if (!(map[mn].rlight)) {
				continue;
			}
			if (!(map[mn].flags & flag)) {
				continue;
			}
			if (!(map[mn].isprite)) {
				continue;
			}

			mtos(mapx_u, mapy_u, &scrx, &scry);

			dist = (x - scrx) * (x - scrx) + (y - scry) * (y - scry);

			if (dist < nearestdist) {
				nearestdist = dist;
				nearest = mn;
			}
		}
	}

	return nearest;
}

DLL_EXPORT map_index_t get_near_char(int x, int y, unsigned int looksize)
{
	int mapx, mapy, scrx, scry;
	unsigned int ux, uy, sx, sy, ex, ey, mapx_u, mapy_u;
	map_index_t mn, nearest = MAXMN;
	double dist, nearestdist = 100000000;

	if (!stom(mousex, mousey, &mapx, &mapy)) {
		return MAXMN;
	}

	if (mapx < 0 || mapy < 0 || mapx >= (int)MAPDX || mapy >= (int)MAPDY) {
		return MAXMN;
	}

	ux = (unsigned int)mapx;
	uy = (unsigned int)mapy;

	mn = mapmn(ux, uy);
	if (mn == MAPDX * MAPDY / 2) {
		return mn;
	}

	sx = (ux > looksize) ? (ux - looksize) : 0U;
	sy = (uy > looksize) ? (uy - looksize) : 0U;
	ex = min(MAPDX - 1, ux + looksize);
	ey = min(MAPDY - 1, uy + looksize);

	for (mapy_u = sy; mapy_u <= ey; mapy_u++) {
		for (mapx_u = sx; mapx_u <= ex; mapx_u++) {
			mn = mapmn(mapx_u, mapy_u);

			if (context_key_enabled() && mn == MAPDX * MAPDY / 2) {
				continue;
			}

			if (!(map[mn].rlight)) {
				continue;
			}
			if (!(map[mn].csprite)) {
				continue;
			}

			mtos(mapx_u, mapy_u, &scrx, &scry);

			dist = (x - scrx) * (x - scrx) + (y - scry) * (y - scry);

			if (dist < nearestdist) {
				nearestdist = dist;
				nearest = mn;
			}
		}
	}

	return nearest;
}
