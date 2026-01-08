/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * Sprites
 *
 * Various lists dealing with sprites. Defining attributes and changing
 * behaviour.
 *
 */

#include <stdint.h>

#include "astonia.h"
#include "game/game.h"
#include "game/game_private.h"
#include "game/sprite_config.h"
#include "gui/gui.h"
#include "client/client.h"
#include "modder/modder.h"

// is_..._sprite - now using JSON config lookups from sprite_config.c
int (*is_cut_sprite)(unsigned int sprite) = sprite_config_is_cut_sprite;
int (*is_mov_sprite)(unsigned int sprite, int itemhint) = sprite_config_is_mov_sprite;
int (*is_door_sprite)(unsigned int sprite) = sprite_config_is_door_sprite;
int (*is_yadd_sprite)(unsigned int sprite) = sprite_config_is_yadd_sprite;

int (*get_chr_height)(unsigned int csprite) = _get_chr_height;

DLL_EXPORT int _get_chr_height(unsigned int csprite)
{
	switch (csprite) {
	case 20:
		return -35;
	case 21:
		return -20;

	case 247:
		return -54;
	case 248:
		return -58;
	case 249:
		return -62;
	case 250:
		return -66;

	case 255:
		return -54;
	case 256:
		return -58;
	case 257:
		return -62;
	case 258:
		return -66;

	case 304:
		return -70;

	default:
		return -50;
	}
}

// charno to scale / colors
int (*trans_charno)(int csprite, int *pscale, int *pcr, int *pcg, int *pcb, int *plight, int *psat, int *pc1, int *pc2,
    int *pc3, int *pshine, int attick) = _trans_charno;

DLL_EXPORT int _trans_charno(int csprite, int *pscale, int *pcr, int *pcg, int *pcb, int *plight, int *psat, int *pc1,
    int *pc2, int *pc3, int *pshine, int attick)
{
	int scale, cr, cg, cb, light, sat, c1, c2, c3, shine;

	/* Look up variant in hash table */
	const CharacterVariant *v = sprite_config_lookup_character(csprite);

	/* Apply variant (returns csprite unchanged if not found) */
	int result =
	    sprite_config_apply_character(v, csprite, &scale, &cr, &cg, &cb, &light, &sat, &c1, &c2, &c3, &shine, attick);

	/* Assign to output pointers (NULL pointers are allowed) */
	if (pscale) {
		*pscale = scale;
	}
	if (pcr) {
		*pcr = cr;
	}
	if (pcg) {
		*pcg = cg;
	}
	if (pcb) {
		*pcb = cb;
	}
	if (plight) {
		*plight = light;
	}
	if (psat) {
		*psat = sat;
	}
	if (pc1) {
		*pc1 = c1;
	}
	if (pc2) {
		*pc2 = c2;
	}
	if (pc3) {
		*pc3 = c3;
	}
	if (pshine) {
		*pshine = shine;
	}

	return result;
}

// asprite
unsigned int (*trans_asprite)(map_index_t mn, unsigned int sprite, tick_t attick, unsigned char *pscale,
    unsigned char *pcr, unsigned char *pcg, unsigned char *pcb, unsigned char *plight, unsigned char *psat,
    unsigned short *pc1, unsigned short *pc2, unsigned short *pc3, unsigned short *pshine) = _trans_asprite;

DLL_EXPORT unsigned int _trans_asprite(map_index_t mn, unsigned int sprite, tick_t attick, unsigned char *pscale,
    unsigned char *pcr, unsigned char *pcg, unsigned char *pcb, unsigned char *plight, unsigned char *psat,
    unsigned short *pc1, unsigned short *pc2, unsigned short *pc3, unsigned short *pshine)
{
	unsigned char scale, cr, cg, cb, light, sat;
	unsigned short c1, c2, c3, shine;

	/* Look up variant in hash table */
	const AnimatedVariant *v = sprite_config_lookup_animated(sprite);

	/* Apply variant (returns sprite unchanged if not found) */
	unsigned int result =
	    sprite_config_apply_animated(v, mn, sprite, attick, &scale, &cr, &cg, &cb, &light, &sat, &c1, &c2, &c3, &shine);

	/* Assign to output pointers (NULL pointers are allowed) */
	if (pscale) {
		*pscale = scale;
	}
	if (pcr) {
		*pcr = cr;
	}
	if (pcg) {
		*pcg = cg;
	}
	if (pcb) {
		*pcb = cb;
	}
	if (plight) {
		*plight = light;
	}
	if (psat) {
		*psat = sat;
	}
	if (pc1) {
		*pc1 = c1;
	}
	if (pc2) {
		*pc2 = c2;
	}
	if (pc3) {
		*pc3 = c3;
	}
	if (pshine) {
		*pshine = shine;
	}

	return result;
}

int (*get_player_sprite)(int nr, int zdir, int action, int step, int duration, int attick) = _get_player_sprite;

DLL_EXPORT int _get_player_sprite(int nr, int zdir, int action, int step, int duration, int attick)
{
	int base;

	// if (nr>100) nr=trans_charno(nr,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,attick);

	base = 100000 + nr * 1000;

	// translate idle action 0 to 60, this runs the idle animation instead of showing the single idle image
	if (action == 0) {
		switch (nr) {
		case 45: // !!!
		case 63:
		case 64:
		case 68:
		case 69:
		case 73:
		case 74:
		case 78:
		case 79:
		case 83:
		case 84:
		case 88:
		case 89:
		case 93:
		case 94:
		case 98:
		case 99:
		case 103:
		case 104:
		case 108:
		case 109:
		case 113:
		case 114:
		case 118:
		case 119:
		case 360:
			action = 60;
			step = (attick % 16);
			duration = 16;
			break;

		case 120:
		case 121:
		case 122:
			action = 60;
			step = (attick % 32);
			duration = 32;
			break;

		default:
			break;
		}
	}

	if (nr == 21) { // spiders action override
		if (action == 2 || action == 3 || (action >= 6 && action <= 49) || action > 60) {
			action = 4;
		}
	}

	// Note: fireball2, lightning ball2, bless2 and heal 2 are really the second half of the same animation
	// and used by the server to be able to create the fireball effect (eg) in mid-cast
	// attack1...3 are actually being used (randomly, just for variety)

	switch (action) {
	case 0:
		return base + 0 + zdir * 1; // idle
	case 1:
		return base + 8 + zdir / 1 * 8 + step * 8 / duration; // walk
	case 2:
		return base + 104 + zdir / 2 * 8 + step * 8 / duration; // take
	case 3:
		return base + 104 + zdir / 2 * 8 + step * 8 / duration; // drop
	case 4:
		return base + 136 + zdir / 2 * 8 + step * 8 / duration; // attack1
	case 5:
		return base + 168 + zdir / 2 * 8 + step * 8 / duration; // attack2
	case 6:
		return base + 200 + zdir / 2 * 8 + step * 8 / duration; // attack3
	case 7:
		return base + 72 + zdir / 2 * 8 + step * 8 / duration; // use

	case 10:
		return base + 232 + zdir / 1 * 8 + step * 4 / duration; // fireball 1
	case 11:
		return base + 236 + zdir / 1 * 8 + step * 4 / duration; // fireball 2
	case 12:
		return base + 232 + zdir / 1 * 8 + step * 4 / duration; // lightning ball 1
	case 13:
		return base + 236 + zdir / 1 * 8 + step * 4 / duration; // lightning ball 2


	case 14:
		return base + 296 + zdir / 2 * 8 + step * 8 / duration; // magic shield
	case 15:
		return base + 296 + zdir / 2 * 8 + step * 8 / duration; // flash
	case 16:
		return base + 296 + zdir / 2 * 8 + step * 8 / duration; // bless - self
	case 17:
		return base + 232 + zdir / 1 * 8 + step * 4 / duration; // bless 1
	case 18:
		return base + 236 + zdir / 1 * 8 + step * 4 / duration; // bless 2
	case 19:
		return base + 296 + zdir / 2 * 8 + step * 8 / duration; // heal - self
	case 20:
		return base + 232 + zdir / 1 * 8 + step * 4 / duration; // heal 1
	case 21:
		return base + 236 + zdir / 1 * 8 + step * 4 / duration; // heal 2
	case 22:
		return base + 296 + zdir / 2 * 8 + step * 8 / duration; // freeze
	case 23:
		return base + 296 + zdir / 2 * 8 + step * 8 / duration; // warcry
	case 24:
		return base + 72 + zdir / 2 * 8 + step * 8 / duration; // give
	case 25:
		return base + 296 + zdir / 2 * 8 + step * 8 / duration; // earth command
	case 26:
		return base + 296 + zdir / 2 * 8 + step * 8 / duration; // earth mud
	case 27:
		return base + 296 + zdir / 2 * 8 + step * 8 / duration; // pulse
	case 28:
		return base + 296 + zdir / 2 * 8 + step * 8 / duration; // pulse


	case 50:
		return base + 328 + zdir / 2 * 8 + step * 8 / duration; // die
	case 60:
		return base + 800 + zdir / 1 * 8 + step * 8 / duration; // idle animated
	}
	return base;
}

void (*trans_csprite)(map_index_t mn, struct map *cmap, tick_t attick) = _trans_csprite;

DLL_EXPORT void _trans_csprite(map_index_t mn, struct map *cmap, tick_t attick)
{
	int dirxadd[8] = {+1, 0, -1, -2, -1, 0, +1, +2};
	int diryadd[8] = {+1, +2, +1, 0, -1, -2, -1, 0};
	unsigned int csprite;
	int scale, cr, cg, cb, light, sat, c1, c2, c3, shine;

	if (playersprite_override && mn == mapmn(MAPDX / 2, MAPDY / 2)) {
		csprite = (unsigned int)playersprite_override;
	} else {
		csprite = cmap[mn].csprite;
	}

	csprite = (unsigned int)trans_charno(
	    (int)csprite, &scale, &cr, &cg, &cb, &light, &sat, &c1, &c2, &c3, &shine, (int)attick);

	cmap[mn].rc.sprite = (unsigned int)get_player_sprite(
	    (int)csprite, cmap[mn].dir - 1, cmap[mn].action, cmap[mn].step, cmap[mn].duration, (int)attick);
	cmap[mn].rc.scale = (unsigned char)scale;

	cmap[mn].rc.shine = (unsigned short)shine;
	cmap[mn].rc.cr = (unsigned char)cr;
	cmap[mn].rc.cg = (unsigned char)cg;
	cmap[mn].rc.cb = (unsigned char)cb;
	cmap[mn].rc.light = (unsigned char)light;
	cmap[mn].rc.sat = (unsigned char)sat;

	if (cmap[mn].csprite < 120 || amod_is_playersprite((int)cmap[mn].csprite)) {
		cmap[mn].rc.c1 = player[cmap[mn].cn].c1;
		cmap[mn].rc.c2 = player[cmap[mn].cn].c2;
		cmap[mn].rc.c3 = player[cmap[mn].cn].c3;
	} else {
		cmap[mn].rc.c1 = (unsigned short)c1;
		cmap[mn].rc.c2 = (unsigned short)c2;
		cmap[mn].rc.c3 = (unsigned short)c3;
	}

	if (cmap[mn].duration && cmap[mn].action == 1) {
		cmap[mn].xadd = (char)(20 * (cmap[mn].step) * dirxadd[cmap[mn].dir - 1] / cmap[mn].duration);
		cmap[mn].yadd = (char)(10 * (cmap[mn].step) * diryadd[cmap[mn].dir - 1] / cmap[mn].duration);
	} else {
		cmap[mn].xadd = 0;
		cmap[mn].yadd = 0;
	}
}

int (*get_lay_sprite)(int sprite, int lay) = sprite_config_get_lay_sprite;

int (*get_offset_sprite)(int sprite, int *px, int *py) = sprite_config_get_offset_sprite;

int (*additional_sprite)(unsigned int sprite, int attick) = _additional_sprite;

DLL_EXPORT int _additional_sprite(unsigned int sprite, int attick)
{
	switch (sprite) {
	case 50495:
	case 50496:
	case 50497:
	case 50498:
		return 50500 + (attick % 6);

	default:
		return 0;
	}
}

unsigned int (*opt_sprite)(unsigned int sprite) = _opt_sprite;

DLL_EXPORT unsigned int _opt_sprite(unsigned int sprite)
{
	switch (sprite) {
	case 13:
		if (game_options & GO_DARK) {
			return 300;
		}
		break;
	case 14:
		if (game_options & GO_DARK) {
			return 301;
		}
		break;
	case 35:
		if (game_options & GO_DARK) {
			return 302;
		}
		break;
	case 991:
		if (game_options & GO_DARK) {
			return 308;
		}
		break;
	case 994:
		if (game_options & GO_DARK) {
			return 303;
		}
		break;
	case 995:
		if (game_options & GO_DARK) {
			return 304;
		}
		break;
	case 998:
		if (game_options & GO_DARK) {
			return 305;
		}
		break;
	case 999:
		if (game_options & GO_DARK) {
			return 306;
		}
		break;
	}
	return sprite;
}

// Return true if the sprite should not get shaded lighting.
// The client will use uniform light instead. Should return
// true for anything that is not a basic wall or floor.
int (*no_lighting_sprite)(unsigned int sprite) = sprite_config_no_lighting_sprite;

/*
 * DLL_EXPORT wrappers for mod compatibility.
 * Mods may call these _*_sprite functions directly.
 * These delegate to the JSON-config-based sprite_config_* functions.
 */
DLL_EXPORT int _is_cut_sprite(unsigned int sprite)
{
	return sprite_config_is_cut_sprite(sprite);
}

DLL_EXPORT int _is_mov_sprite(unsigned int sprite, int itemhint)
{
	return sprite_config_is_mov_sprite(sprite, itemhint);
}

DLL_EXPORT int _is_door_sprite(unsigned int sprite)
{
	return sprite_config_is_door_sprite(sprite);
}

DLL_EXPORT int _is_yadd_sprite(unsigned int sprite)
{
	return sprite_config_is_yadd_sprite(sprite);
}

DLL_EXPORT int _get_lay_sprite(int sprite, int lay)
{
	return sprite_config_get_lay_sprite(sprite, lay);
}

DLL_EXPORT int _get_offset_sprite(int sprite, int *px, int *py)
{
	return sprite_config_get_offset_sprite(sprite, px, py);
}

DLL_EXPORT int _no_lighting_sprite(unsigned int sprite)
{
	return sprite_config_no_lighting_sprite(sprite);
}
