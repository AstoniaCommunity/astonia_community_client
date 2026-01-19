/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * Rendering System
 *
 * SDL2-based rendering abstraction layer providing system-independent graphics functions
 *
 */

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <SDL3/SDL.h>

#include "dll.h"
#include "astonia.h"
#include "game/game.h"
#include "game/game_private.h"
#include "client/client.h"
#include "sdl/sdl.h"

static RenderFont fonta_shaded_storage[128];
RenderFont *fonta_shaded = fonta_shaded_storage;
static RenderFont fonta_framed_storage[128];
RenderFont *fonta_framed = fonta_framed_storage;

static RenderFont fontb_shaded_storage[128];
RenderFont *fontb_shaded = fontb_shaded_storage;
static RenderFont fontb_framed_storage[128];
RenderFont *fontb_framed = fontb_framed_storage;

static RenderFont fontc_shaded_storage[128];
RenderFont *fontc_shaded = fontc_shaded_storage;
static RenderFont fontc_framed_storage[128];
RenderFont *fontc_framed = fontc_framed_storage;

// Global rendering offset for window scaling and positioning
int x_offset, y_offset;

// Clipping rectangle bounds (screen coordinates)
static int clipsx, clipsy, clipex, clipey;

// Clipping stack for nested rendering regions (max depth: 32)
static int clipstore[32][4], clippos = 0;

/**
 * Dump rendering system state to file for debugging.
 * Used by the crash handler to report rendering state when errors occur.
 *
 */
void render_dump(FILE *fp)
{
	fprintf(fp, "Rendering System Data Dump:\n");

	fprintf(fp, "x_offset: %d\n", x_offset);
	fprintf(fp, "y_offset: %d\n", y_offset);

	fprintf(fp, "clipsx: %d\n", clipsx);
	fprintf(fp, "clipsy: %d\n", clipsy);
	fprintf(fp, "clipex: %d\n", clipex);
	fprintf(fp, "clipey: %d\n", clipey);

	fprintf(fp, "\n");
}

/**
 * Push current clipping rectangle onto the stack.
 * Allows nested UI components to temporarily restrict rendering regions.
 * Maximum stack depth is 32 levels.
 *
 * Must be balanced with render_pop_clip() calls.
 */
DLL_EXPORT void render_push_clip(void)
{
	if (clippos >= 32) {
		return;
	}

	clipstore[clippos][0] = clipsx;
	clipstore[clippos][1] = clipsy;
	clipstore[clippos][2] = clipex;
	clipstore[clippos][3] = clipey;
	clippos++;
}

/**
 * Pop clipping rectangle from the stack.
 * Restores the previous clipping region after rendering a nested UI component.
 *
 * Must be balanced with render_push_clip() calls.
 */
DLL_EXPORT void render_pop_clip(void)
{
	if (clippos == 0) {
		return;
	}

	clippos--;
	clipsx = clipstore[clippos][0];
	clipsy = clipstore[clippos][1];
	clipex = clipstore[clippos][2];
	clipey = clipstore[clippos][3];
}

/**
 * Restrict the clipping rectangle further by intersecting with new bounds.
 * The resulting clip rect is the intersection of the current rect and the new rect.
 *
 */
DLL_EXPORT void render_more_clip(int sx, int sy, int ex, int ey)
{
	if (sx > clipsx) {
		clipsx = sx;
	}
	if (sy > clipsy) {
		clipsy = sy;
	}
	if (ex < clipex) {
		clipex = ex;
	}
	if (ey < clipey) {
		clipey = ey;
	}
}

/**
 * Set the clipping rectangle to specific bounds.
 * All rendering operations will be clipped to this rectangle.
 *
 */
DLL_EXPORT void render_set_clip(int sx, int sy, int ex, int ey)
{
	clipsx = sx;
	clipsy = sy;
	clipex = ex;
	clipey = ey;
}

/**
 * Clear the clipping rectangle, resetting to full screen.
 * After calling this, rendering will not be clipped.
 */
DLL_EXPORT void render_clear_clip(void)
{
	clipsx = 0;
	clipsy = 0;
	clipex = XRES;
	clipey = YRES;
}

/**
 * Get the current clipping rectangle bounds.
 */
DLL_EXPORT void render_get_clip(int *out_start_x, int *out_start_y, int *out_end_x, int *out_end_y)
{
	if (out_start_x) {
		*out_start_x = clipsx;
	}
	if (out_start_y) {
		*out_start_y = clipsy;
	}
	if (out_end_x) {
		*out_end_x = clipex;
	}
	if (out_end_y) {
		*out_end_y = clipey;
	}
}

/**
 * Initialize the rendering system.
 * Sets up clipping to full screen resolution, creates font textures,
 * and initializes the chat window text system.
 *
 * @return 0 on success
 */
int render_init(void)
{
	// set the clipping to the maximum possible
	clippos = 0;
	clipsx = 0;
	clipsy = 0;
	clipex = XRES;
	clipey = YRES;

	render_create_font();
	render_init_text();

	return 0;
}

/**
 * Cleanup and shutdown the rendering system.
 * Called during application exit.
 *
 * @return 0 on success
 */
int render_exit(void)
{
	return 0;
}

/**
 * Render a sprite with full effects support.
 * This is the primary sprite rendering function supporting lighting, scaling,
 * color manipulation, alpha blending, animation freeze frames, and custom clipping.
 *
 * @return 1 on success, 0 if sprite could not be loaded
 */
DLL_EXPORT int render_sprite_fx(RenderFX *fx, int scrx, int scry)
{
	int stx;

	assert(fx != NULL && "render_sprite_fx: fx=NULL");
	assert(fx->light >= 0 && fx->light <= 16 && "render_sprite_fx: fx->light out of range");
	assert(fx->freeze >= 0 && fx->freeze < RENDERFX_MAX_FREEZE && "render_sprite_fx: fx->freeze out of range");

	stx = sdl_tx_load(fx->sprite, fx->sink, fx->freeze, fx->scale, fx->cr, fx->cg, fx->cb, fx->clight, fx->sat, fx->c1,
	    fx->c2, fx->c3, fx->shine, fx->ml, fx->ll, fx->rl, fx->ul, fx->dl, NULL, 0, 0, NULL, 0, 0);

	if (stx == -1) {
		return 0;
	}

	// shift position according to align
	if (fx->align == RENDER_ALIGN_OFFSET) {
		scrx += sdlt_xoff(stx);
		scry += sdlt_yoff(stx);
	} else if (fx->align == RENDER_ALIGN_CENTER) {
		scrx -= sdlt_xres(stx) / 2;
		scry -= sdlt_yres(stx) / 2;
	}

	// add the additional cliprect
	if (fx->clipsx != fx->clipex || fx->clipsy != fx->clipey) {
		render_push_clip();
		if (fx->clipsx != fx->clipex) {
			render_more_clip(scrx - sdlt_xoff(stx) + fx->clipsx, clipsy, scrx - sdlt_xoff(stx) + fx->clipex, clipey);
		}
		if (fx->clipsy != fx->clipey) {
			render_more_clip(clipsx, scry - sdlt_yoff(stx) + fx->clipsy, clipex, scry - sdlt_yoff(stx) + fx->clipey);
		}
	}

	// blit it
	if (fx->alpha) {
		sdl_tex_alpha(stx, fx->alpha);
	}
	sdl_blit(stx, scrx, scry, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
	if (fx->alpha) {
		sdl_tex_alpha(stx, 255);
	}

	// remove additional cliprect
	if (fx->clipsx != fx->clipex || fx->clipsy != fx->clipey) {
		render_pop_clip();
	}

	return 1;
}

/**
 * Render a sprite with basic effects (internal helper function).
 * Simplified version of render_sprite_fx() for internal use with minimal parameters.
 *
 */
void render_sprite_callfx(unsigned int sprite, int scrx, int scry, char light, char ml, char align)
{
	RenderFX fx;

	bzero(&fx, sizeof(RenderFX));

	fx.sprite = sprite;
	fx.light = light;
	fx.align = align;

	fx.ml = fx.ll = fx.rl = fx.ul = fx.dl = ml;
	fx.sink = 0;
	fx.scale = 100;
	fx.cr = fx.cg = fx.cb = fx.clight = fx.sat = 0;
	fx.c1 = fx.c2 = fx.c3 = fx.shine = 0;

	render_sprite_fx(&fx, scrx, scry);
}

/**
 * Render a sprite with simple lighting.
 * Simplified sprite rendering with basic lighting support.
 * This is the most commonly used sprite rendering function in the codebase.
 *
 */
DLL_EXPORT void render_sprite(unsigned int sprite, int scrx, int scry, char light, char align)
{
	RenderFX fx;

	bzero(&fx, sizeof(RenderFX));

	fx.sprite = sprite;
	fx.light = RENDERFX_NORMAL_LIGHT;
	fx.align = align;

	fx.ml = fx.ll = fx.rl = fx.ul = fx.dl = light;
	fx.scale = 100;

	render_sprite_fx(&fx, scrx, scry);
}

/**
 * Draw a filled rectangle.
 * Renders a solid colored rectangle with clipping support.
 *
 */
DLL_EXPORT void render_rect(int sx, int sy, int ex, int ey, unsigned short int color)
{
	sdl_rect(sx, sy, ex, ey, color, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw a filled rectangle with alpha blending.
 * Renders a semi-transparent colored rectangle.
 *
 */
void render_shaded_rect(int sx, int sy, int ex, int ey, unsigned short color, unsigned short alpha)
{
	sdl_shaded_rect(sx, sy, ex, ey, color, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw a line between two points.
 * Renders a 1-pixel wide line with clipping support.
 *
 */
DLL_EXPORT void render_line(int fx, int fy, int tx, int ty, unsigned short col)
{
	sdl_line(fx, fy, tx, ty, col, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Render a lightning strike effect between two points.
 * Used for spell effects and combat visuals.
 *
 */
void render_display_strike(int fx, int fy, int tx, int ty)
{
	int mx, my;
	int dx, dy, d, l;
	unsigned short col;

	dx = abs(tx - fx);
	dy = abs(ty - fy);

	mx = (fx + tx) / 2 + 15 - rrand(30);
	my = (fy + ty) / 2 + 15 - rrand(30);

	if (dx >= dy) {
		for (d = -4; d < 5; d++) {
			l = (4 - abs(d)) * 4;
			col = (unsigned short)IRGB(l, l, 31);
			render_line(fx, fy, mx, my + d, col);
			render_line(mx, my + d, tx, ty, col);
		}
	} else {
		for (d = -4; d < 5; d++) {
			l = (4 - abs(d)) * 4;
			col = (unsigned short)IRGB(l, l, 31);
			render_line(fx, fy, mx + d, my, col);
			render_line(mx + d, my, tx, ty, col);
		}
	}
}

void render_draw_curve(int cx, int cy, int nr, int size, int col)
{
	int n, x, y;
	unsigned short ucol = (unsigned short)col;

	for (n = nr * 90; n < nr * 90 + 90; n += 4) {
		x = (int)(sin(n / 360.0 * M_PI * 2) * size) + cx;
		y = (int)(cos(n / 360.0 * M_PI * 2) * size * 2 / 3) + cy;

		if (x < clipsx) {
			continue;
		}
		if (y < clipsy) {
			continue;
		}
		if (x >= clipex) {
			continue;
		}
		if (y + 10 >= clipey) {
			continue;
		}

		sdl_pixel(x, y, ucol, x_offset, y_offset);
		sdl_pixel(x, y + 5, ucol, x_offset, y_offset);
		sdl_pixel(x, y + 10, ucol, x_offset, y_offset);
	}
}

/**
 * Render a green pulseback lightning effect between two points.
 * Used for healing/buff spell effects.
 *
 */
void render_display_pulseback(int fx, int fy, int tx, int ty)
{
	int mx, my;
	int dx, dy, d, l;
	unsigned short col;

	dx = abs(tx - fx);
	dy = abs(ty - fy);

	mx = (fx + tx) / 2 + 15 - rrand(30);
	my = (fy + ty) / 2 + 15 - rrand(30);

	if (dx >= dy) {
		for (d = -4; d < 5; d++) {
			l = (4 - abs(d)) * 4;
			col = (unsigned short)IRGB(l, 31, l);
			render_line(fx, fy, mx, my + d, col);
			render_line(mx, my + d, tx, ty, col);
		}
	} else {
		for (d = -4; d < 5; d++) {
			l = (4 - abs(d)) * 4;
			col = (unsigned short)IRGB(l, 31, l);
			render_line(fx, fy, mx + d, my, col);
			render_line(mx + d, my, tx, ty, col);
		}
	}
}

// text

/**
 * Calculate the pixel width of text.
 * Stops at text terminator character.
 *
 * @return Width in pixels
 */
DLL_EXPORT int render_text_length(int flags, const char *text)
{
	RenderFont *font;
	int x;
	const char *c;

	if (flags & RENDER_TEXT_SMALL) {
		font = fontb;
	} else if (flags & RENDER_TEXT_BIG) {
		font = fontc;
	} else {
		font = fonta;
	}

	for (x = 0, c = text; *c && *c != RENDER_TEXT_TERMINATOR; c++) {
		x += font[(unsigned char)*c].dim;
	}

	return x;
}

int render_text_len(int flags, const char *text, int n)
{
	RenderFont *font;
	int x;
	const char *c;

	if (n < 0) {
		return render_text_length(flags, text);
	}

	if (flags & RENDER_TEXT_SMALL) {
		font = fontb;
	} else if (flags & RENDER_TEXT_BIG) {
		font = fontc;
	} else {
		font = fonta;
	}

	for (x = 0, c = text; *c && *c != RENDER_TEXT_TERMINATOR && n; c++, n--) {
		x += font[(unsigned char)*c].dim;
	}

	return x;
}

/**
 * Render text at specified position.
 * Supports multiple fonts, alignment, shadows, and outlines.
 *
 * @return Final X coordinate after rendering
 */
DLL_EXPORT int render_text(int sx, int sy, unsigned short int color, int flags, const char *text)
{
	RenderFont *font;

	if (flags & RENDER__SHADED_FONT) {
		if (flags & RENDER_TEXT_SMALL) {
			font = fontb_shaded;
		} else if (flags & RENDER_TEXT_BIG) {
			font = fontc_shaded;
		} else {
			font = fonta_shaded;
		}
	} else if (flags & RENDER__FRAMED_FONT) {
		if (flags & RENDER_TEXT_SMALL) {
			font = fontb_framed;
		} else if (flags & RENDER_TEXT_BIG) {
			font = fontc_framed;
		} else {
			font = fonta_framed;
		}
	} else {
		if (flags & RENDER_TEXT_SMALL) {
			font = fontb;
		} else if (flags & RENDER_TEXT_BIG) {
			font = fontc;
		} else {
			font = fonta;
		}
	}
	if (!font) {
		return 42;
	}

	if (flags & RENDER_TEXT_SHADED) {
		render_text(sx - 1, sy - 1, IRGB(0, 0, 0),
		    RENDER_TEXT_LEFT |
		        (flags & (RENDER_TEXT_SMALL | RENDER_TEXT_BIG | RENDER_ALIGN_CENTER | RENDER_TEXT_RIGHT |
		                     RENDER_TEXT_NOCACHE)) |
		        RENDER__SHADED_FONT,
		    text);
	} else if (flags & RENDER_TEXT_FRAMED) {
		if (flags & 512) {
			render_text(sx - 1, sy - 1, IRGB(31, 31, 31),
			    RENDER_TEXT_LEFT |
			        (flags & (RENDER_TEXT_SMALL | RENDER_TEXT_BIG | RENDER_ALIGN_CENTER | RENDER_TEXT_RIGHT |
			                     RENDER_TEXT_NOCACHE)) |
			        RENDER__FRAMED_FONT,
			    text);
		} else {
			render_text(sx - 1, sy - 1, IRGB(0, 0, 0),
			    RENDER_TEXT_LEFT |
			        (flags & (RENDER_TEXT_SMALL | RENDER_TEXT_BIG | RENDER_ALIGN_CENTER | RENDER_TEXT_RIGHT |
			                     RENDER_TEXT_NOCACHE)) |
			        RENDER__FRAMED_FONT,
			    text);
		}
	}

	sx = sdl_drawtext(sx, sy, color, flags, text, font, clipsx, clipsy, clipex, clipey, x_offset, y_offset);

	return sx;
}

/**
 * Render text with word wrapping.
 * Automatically wraps text at word boundaries when reaching breakx.
 *
 * @return Final Y coordinate after rendering
 */
DLL_EXPORT int render_text_break(int x, int y, int breakx, unsigned short color, int flags, const char *ptr)
{
	char buf[256];
	int xp, n;
	int size;

	xp = x;

	while (*ptr) {
		while (*ptr == ' ') {
			ptr++;
		}

		for (n = 0; n < 256 && *ptr && *ptr != ' '; buf[n++] = *ptr++)
			;
		buf[n] = 0;

		size = render_text_length(flags, buf);
		if (xp + size > breakx) {
			xp = x;
			y += 10;
		}
		render_text(xp, y, color, flags, buf);
		xp += size + 4;
	}
	return y + 10;
}

DLL_EXPORT int render_text_nl(int x, int y, int unsigned short color, int flags, const char *ptr)
{
	char buf[256];
	int n;

	while (*ptr) {
		while (*ptr == '\n') {
			ptr++;
		}

		for (n = 0; n < 256 && *ptr && *ptr != '\n'; buf[n++] = *ptr++)
			;
		buf[n] = 0;
		render_text(x, y, color, flags, buf);
		if (flags & RENDER_TEXT_BIG) {
			y += 12;
		} else {
			y += 10;
		}
	}
	return y + 10;
}

DLL_EXPORT int render_text_break_length(int x, int y, int breakx, unsigned short color, int flags, const char *ptr)
{
	(void)color; // unused parameter
	char buf[256];
	int xp, n;
	int size;

	xp = x;

	while (*ptr) {
		while (*ptr == ' ') {
			ptr++;
		}

		for (n = 0; n < 256 && *ptr && *ptr != ' '; buf[n++] = *ptr++)
			;
		buf[n] = 0;

		size = render_text_length(flags, buf);
		if (xp + size > breakx) {
			xp = x;
			y += 10;
		}
		xp += size + 4;
	}
	return y + 10;
}

/**
 * Draw a single pixel.
 */
DLL_EXPORT void render_pixel(int x, int y, unsigned short color)
{
	sdl_pixel(x, y, color, x_offset, y_offset);
}

/**
 * Draw a single pixel with alpha blending.
 *
 */
DLL_EXPORT void render_pixel_alpha(int x, int y, unsigned short col, unsigned char alpha)
{
	sdl_pixel_alpha(x, y, col, alpha, x_offset, y_offset);
}

/**
 * Draw a filled rectangle with alpha blending.
 * Exported version of render_shaded_rect for modders.
 *
 */
DLL_EXPORT void render_rect_alpha(int sx, int sy, int ex, int ey, unsigned short color, unsigned char alpha)
{
	sdl_shaded_rect(sx, sy, ex, ey, color, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw a line with alpha blending.
 *
 */
DLL_EXPORT void render_line_alpha(int fx, int fy, int tx, int ty, unsigned short col, unsigned char alpha)
{
	sdl_line_alpha(fx, fy, tx, ty, col, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

// ============================================================================
// Extended Rendering Primitives for Modders
// ============================================================================

/**
 * Draw a circle outline with alpha blending.
 *
 */
DLL_EXPORT void render_circle_alpha(int cx, int cy, int radius, unsigned short color, unsigned char alpha)
{
	sdl_circle_alpha(cx, cy, radius, color, alpha, x_offset, y_offset);
}

/**
 * Draw a filled circle with alpha blending.
 *
 */
DLL_EXPORT void render_circle_filled_alpha(int cx, int cy, int radius, unsigned short color, unsigned char alpha)
{
	sdl_circle_filled_alpha(cx, cy, radius, color, alpha, x_offset, y_offset);
}

/**
 * Draw an ellipse outline with alpha blending.
 *
 */
DLL_EXPORT void render_ellipse_alpha(int cx, int cy, int rx, int ry, unsigned short color, unsigned char alpha)
{
	sdl_ellipse_alpha(cx, cy, rx, ry, color, alpha, x_offset, y_offset);
}

/**
 * Draw a filled ellipse with alpha blending.
 *
 */
DLL_EXPORT void render_ellipse_filled_alpha(int cx, int cy, int rx, int ry, unsigned short color, unsigned char alpha)
{
	sdl_ellipse_filled_alpha(cx, cy, rx, ry, color, alpha, x_offset, y_offset);
}

/**
 * Draw a rectangle outline (hollow) with alpha blending.
 *
 */
DLL_EXPORT void render_rect_outline_alpha(int sx, int sy, int ex, int ey, unsigned short color, unsigned char alpha)
{
	sdl_rect_outline_alpha(sx, sy, ex, ey, color, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw a rounded rectangle outline with alpha blending.
 *
 */
DLL_EXPORT void render_rounded_rect_alpha(
    int sx, int sy, int ex, int ey, int radius, unsigned short color, unsigned char alpha)
{
	sdl_rounded_rect_alpha(sx, sy, ex, ey, radius, color, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw a filled rounded rectangle with alpha blending.
 *
 */
DLL_EXPORT void render_rounded_rect_filled_alpha(
    int sx, int sy, int ex, int ey, int radius, unsigned short color, unsigned char alpha)
{
	sdl_rounded_rect_filled_alpha(
	    sx, sy, ex, ey, radius, color, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw a triangle outline with alpha blending.
 *
 */
DLL_EXPORT void render_triangle_alpha(
    int x1, int y1, int x2, int y2, int x3, int y3, unsigned short color, unsigned char alpha)
{
	sdl_triangle_alpha(x1, y1, x2, y2, x3, y3, color, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw a filled triangle with alpha blending.
 *
 */
DLL_EXPORT void render_triangle_filled_alpha(
    int x1, int y1, int x2, int y2, int x3, int y3, unsigned short color, unsigned char alpha)
{
	sdl_triangle_filled_alpha(x1, y1, x2, y2, x3, y3, color, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw a thick line with alpha blending.
 * Unlike render_line_alpha which is 1 pixel wide, this allows configurable thickness.
 *
 */
DLL_EXPORT void render_thick_line_alpha(
    int fx, int fy, int tx, int ty, int thickness, unsigned short color, unsigned char alpha)
{
	sdl_thick_line_alpha(fx, fy, tx, ty, thickness, color, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw an arc (partial circle outline) with alpha blending.
 * Angles are in degrees, 0 = right (3 o'clock), increasing clockwise.
 *
 */
DLL_EXPORT void render_arc_alpha(
    int cx, int cy, int radius, int start_angle, int end_angle, unsigned short color, unsigned char alpha)
{
	sdl_arc_alpha(cx, cy, radius, start_angle, end_angle, color, alpha, x_offset, y_offset);
}

/**
 * Draw a horizontal gradient rectangle with alpha blending.
 * Colors interpolate from color1 (left) to color2 (right).
 *
 */
DLL_EXPORT void render_gradient_rect_h(
    int sx, int sy, int ex, int ey, unsigned short color1, unsigned short color2, unsigned char alpha)
{
	sdl_gradient_rect_h(sx, sy, ex, ey, color1, color2, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw a vertical gradient rectangle with alpha blending.
 * Colors interpolate from color1 (top) to color2 (bottom).
 *
 */
DLL_EXPORT void render_gradient_rect_v(
    int sx, int sy, int ex, int ey, unsigned short color1, unsigned short color2, unsigned char alpha)
{
	sdl_gradient_rect_v(sx, sy, ex, ey, color1, color2, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Draw a quadratic Bezier curve with alpha blending.
 * The curve passes through (x0,y0) and (x2,y2), with (x1,y1) as the control point.
 *
 */
DLL_EXPORT void render_bezier_quadratic_alpha(
    int x0, int y0, int x1, int y1, int x2, int y2, unsigned short color, unsigned char alpha)
{
	sdl_bezier_quadratic_alpha(x0, y0, x1, y1, x2, y2, color, alpha, x_offset, y_offset);
}

/**
 * Draw a cubic Bezier curve with alpha blending.
 * The curve passes through (x0,y0) and (x3,y3), with (x1,y1) and (x2,y2) as control points.
 *
 */
DLL_EXPORT void render_bezier_cubic_alpha(
    int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, unsigned short color, unsigned char alpha)
{
	sdl_bezier_cubic_alpha(x0, y0, x1, y1, x2, y2, x3, y3, color, alpha, x_offset, y_offset);
}

/**
 * Draw a radial gradient circle (glow effect) with alpha blending.
 * Creates a soft glowing effect with bright center fading to transparent edge.
 * Perfect for particles, auras, and light sources.
 *
 */
DLL_EXPORT void render_gradient_circle(
    int cx, int cy, int radius, unsigned short color, unsigned char center_alpha, unsigned char edge_alpha)
{
	sdl_gradient_circle(cx, cy, radius, color, center_alpha, edge_alpha, x_offset, y_offset);
}

/**
 * Draw an anti-aliased line with alpha blending.
 * Uses Xiaolin Wu's algorithm for smooth lines without jaggies.
 * Ideal for lightning effects, beams, and trails.
 *
 */
DLL_EXPORT void render_line_aa(int x0, int y0, int x1, int y1, unsigned short color, unsigned char alpha)
{
	sdl_line_aa(x0, y0, x1, y1, color, alpha, x_offset, y_offset);
}

/**
 * Draw a filled ring (arc with inner and outer radius) with alpha blending.
 * Perfect for shockwaves, expanding rings, circular sweeps.
 * Angles are in degrees, 0 = right (3 o'clock), increasing clockwise.
 *
 */
DLL_EXPORT void render_ring_alpha(int cx, int cy, int inner_radius, int outer_radius, int start_angle, int end_angle,
    unsigned short color, unsigned char alpha)
{
	sdl_ring_alpha(cx, cy, inner_radius, outer_radius, start_angle, end_angle, color, alpha, x_offset, y_offset);
}

/**
 * Set the blend mode for subsequent rendering operations.
 * Affects how colors are blended when drawing with alpha.
 *
 */
DLL_EXPORT void render_set_blend_mode(int mode)
{
	sdl_set_blend_mode(mode);
}

/**
 * Get the current blend mode.
 *
 * @return Current blend mode: BLEND_NORMAL (0), BLEND_ADDITIVE (1), BLEND_MULTIPLY (2)
 */
DLL_EXPORT int render_get_blend_mode(void)
{
	return sdl_get_blend_mode();
}

// ============================================================================
// Custom Texture Loading API for Modders
// ============================================================================

/**
 * Load a texture from a PNG file.
 * The texture can be rendered using render_texture() or render_texture_scaled().
 *
 * @return Texture ID (>= 0) on success, -1 on failure
 */
DLL_EXPORT int render_load_texture(const char *path)
{
	return sdl_load_mod_texture(path);
}

/**
 * Unload a previously loaded texture.
 * Frees the GPU memory associated with the texture.
 *
 */
DLL_EXPORT void render_unload_texture(int tex_id)
{
	sdl_unload_mod_texture(tex_id);
}

/**
 * Render a custom texture at the specified position.
 *
 */
DLL_EXPORT void render_texture(int tex_id, int x, int y, unsigned char alpha)
{
	sdl_render_mod_texture(tex_id, x, y, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Render a custom texture with scaling.
 *
 */
DLL_EXPORT void render_texture_scaled(int tex_id, int x, int y, float scale, unsigned char alpha)
{
	sdl_render_mod_texture_scaled(tex_id, x, y, scale, alpha, clipsx, clipsy, clipex, clipey, x_offset, y_offset);
}

/**
 * Get the width of a loaded texture.
 *
 * @return Width in pixels, or 0 if invalid texture ID
 */
DLL_EXPORT int render_texture_width(int tex_id)
{
	return sdl_get_mod_texture_width(tex_id);
}

/**
 * Get the height of a loaded texture.
 *
 * @return Height in pixels, or 0 if invalid texture ID
 */
DLL_EXPORT int render_texture_height(int tex_id)
{
	return sdl_get_mod_texture_height(tex_id);
}

// ============================================================================
// Screen Effects for Modders
// ============================================================================

/**
 * Apply a color tint to the entire screen.
 * Draws a semi-transparent colored overlay over the current frame.
 * Should be called after all other rendering, typically at the end of on_frame.
 *
 */
DLL_EXPORT void render_screen_tint(unsigned short color, unsigned char intensity)
{
	if (intensity == 0) {
		return;
	}
	sdl_shaded_rect(0, 0, XRES, YRES, color, intensity, 0, 0, XRES, YRES, x_offset, y_offset);
}

/**
 * Apply a vignette effect (darken the edges of the screen).
 * Creates a cinematic fade-to-black effect at the screen borders.
 * Should be called after all other rendering, typically at the end of on_frame.
 *
 */
DLL_EXPORT void render_vignette(unsigned char intensity)
{
	int border_size, i, band_start, band_end, band_count;
	unsigned char alpha;

	if (intensity == 0) {
		return;
	}

	// Calculate border size based on intensity (max ~15% of screen)
	// 1700 = 255 / 0.15 (roughly), where 255 is max intensity and 0.15 is max border ratio
	// This gives: at intensity=255, border_size = min_dimension * 0.15 (15% of screen)
	border_size = (XRES < YRES ? XRES : YRES) * intensity / 1700;
	if (border_size < 1) {
		border_size = 1;
	}

	// Optimization: Use 16 bands maximum instead of per-pixel strips
	// This reduces draw calls from 4*border_size to 4*16 = 64 maximum
	band_count = border_size < 16 ? border_size : 16;

	// Draw gradient borders using larger bands with averaged alpha
	for (i = 0; i < band_count; i++) {
		band_start = (border_size * i) / band_count;
		band_end = (border_size * (i + 1)) / band_count;
		if (band_end <= band_start) {
			band_end = band_start + 1;
		}

		// Use alpha from the center of this band
		alpha = (unsigned char)((intensity * (border_size - (band_start + band_end) / 2)) / border_size);

		// Left edge
		sdl_shaded_rect(band_start, 0, band_end, YRES, 0, alpha, 0, 0, XRES, YRES, x_offset, y_offset);
		// Right edge
		sdl_shaded_rect(XRES - band_end, 0, XRES - band_start, YRES, 0, alpha, 0, 0, XRES, YRES, x_offset, y_offset);
		// Top edge
		sdl_shaded_rect(0, band_start, XRES, band_end, 0, alpha, 0, 0, XRES, YRES, x_offset, y_offset);
		// Bottom edge
		sdl_shaded_rect(0, YRES - band_end, XRES, YRES - band_start, 0, alpha, 0, 0, XRES, YRES, x_offset, y_offset);
	}
}

/**
 * Flash the screen with a color effect.
 * Useful for damage feedback, healing effects, or other visual cues.
 *
 */
DLL_EXPORT void render_screen_flash(unsigned short color, unsigned char intensity)
{
	render_screen_tint(color, intensity);
}

// ============================================================================
// Render Targets for Modders
// ============================================================================

/**
 * Create a render target (offscreen buffer) for rendering.
 * Useful for post-processing effects, motion blur, bloom, etc.
 *
 * @return Target ID (>= 0) on success, -1 on failure
 */
DLL_EXPORT int render_create_target(int width, int height)
{
	return sdl_create_render_target(width, height);
}

/**
 * Destroy a previously created render target.
 * Frees the GPU memory associated with the target.
 *
 */
DLL_EXPORT void render_destroy_target(int target_id)
{
	sdl_destroy_render_target(target_id);
}

/**
 * Set the current render target.
 * All subsequent rendering will go to this target instead of the screen.
 * Pass -1 or RENDER_TARGET_SCREEN to render to the screen again.
 *
 * @return 0 on success, -1 on failure
 */
DLL_EXPORT int render_set_target(int target_id)
{
	return sdl_set_render_target(target_id);
}

/**
 * Render a target to the screen at the specified position.
 *
 */
DLL_EXPORT void render_target_to_screen(int target_id, int x, int y, unsigned char alpha)
{
	sdl_render_target_to_screen(target_id, x, y, alpha);
}

/**
 * Clear a render target to transparent black.
 *
 */
DLL_EXPORT void render_clear_target(int target_id)
{
	sdl_clear_render_target(target_id);
}

/**
 * Render formatted text (printf-style) at specified position.
 * Supports all render_text() features with printf-style formatting.
 *
 * @return Final X coordinate after rendering
 */
DLL_EXPORT int render_text_fmt(int64_t sx, int64_t sy, unsigned short int color, int flags, const char *format, ...)
{
	char buf[1024];
	va_list va;

	va_start(va, format);
	vsprintf(buf, format, va);
	va_end(va);

	return render_text((int)sx, (int)sy, color, flags, buf);
}

DLL_EXPORT int render_text_break_fmt(
    int sx, int sy, int breakx, unsigned short int color, int flags, const char *format, ...)
{
	char buf[1024];
	va_list va;

	va_start(va, format);
	vsprintf(buf, format, va);
	va_end(va);

	return render_text_break(sx, sy, breakx, color, flags, buf);
}

static int bless_init = 0;
static int bless_sin[36];
static int bless_cos[36];
static int bless_hight[200];

static void render_draw_bless_pix(int x, int y, int nr, int color, int front)
{
	int sy;

	sy = bless_sin[nr % 36];
	if (front && sy < 0) {
		return;
	}
	if (!front && sy >= 0) {
		return;
	}

	x += bless_cos[nr % 36];
	y = y + sy + bless_hight[nr % 200];

	if (x < clipsx || x >= clipex || y < clipsy || y >= clipey) {
		return;
	}

	sdl_pixel(x, y, (unsigned short)color, x_offset, y_offset);
}

static void render_draw_heal_pix(int x, int y, int nr, int color, int front)
{
	int sy, val;
	double off;

	val = (nr / 36) % 5;
	off = 0.6 + val * 0.10;
	sy = (int)(bless_sin[nr % 36] * off);
	if (front && sy < 0) {
		return;
	}
	if (!front && sy >= 0) {
		return;
	}

	x += (int)(bless_cos[nr % 36] * off);

	y = y + sy + bless_hight[nr % 200] / 10 - 45;

	if (x < clipsx || x >= clipex || y < clipsy || y >= clipey) {
		return;
	}

	sdl_pixel(x, y, (unsigned short)color, x_offset, y_offset);
}

static void render_draw_rain_pix(int x, int y, int nr, int color, int front)
{
	int sy;

	x += ((nr / 30) % 30) + 15;
	sy = ((nr / 330) % 20) + 10;
	if (front && sy < 0) {
		return;
	}
	if (!front && sy >= 0) {
		return;
	}

	y = y + sy - ((nr * 2) % 60) - 60;

	if (x < clipsx || x >= clipex || y < clipsy || y >= clipey) {
		return;
	}

	sdl_pixel(x, y, (unsigned short)color, x_offset, y_offset);
}

static void init_bless(void)
{
	int nr;
	if (!bless_init) {
		for (nr = 0; nr < 36; nr++) {
			bless_sin[nr] = (int)(sin((nr % 36) / 36.0 * M_PI * 2) * 8);
			bless_cos[nr] = (int)(cos((nr % 36) / 36.0 * M_PI * 2) * 16);
		}
		for (nr = 0; nr < 200; nr++) {
			bless_hight[nr] = -20 + (int)(sin((nr % 200) / 200.0 * M_PI * 2) * 20);
		}
		bless_init = 1;
	}
}

void render_draw_bless(int x, int y, int ticker, int strength, int front)
{
	int step;
	double light;

	init_bless();

	if (ticker > 62) {
		light = 1.0;
	} else {
		light = (ticker) / 62.0;
	}

	for (step = 0; step < strength * 10; step += 17) {
		render_draw_bless_pix(
		    x, y, ticker + step + 0, IRGB((int)(24 * light), (int)(24 * light), (int)(31 * light)), front);
		render_draw_bless_pix(
		    x, y, ticker + step + 1, IRGB((int)(20 * light), (int)(20 * light), (int)(28 * light)), front);
		render_draw_bless_pix(
		    x, y, ticker + step + 2, IRGB((int)(16 * light), (int)(16 * light), (int)(24 * light)), front);
		render_draw_bless_pix(
		    x, y, ticker + step + 3, IRGB((int)(12 * light), (int)(12 * light), (int)(20 * light)), front);
		render_draw_bless_pix(
		    x, y, ticker + step + 4, IRGB((int)(8 * light), (int)(8 * light), (int)(16 * light)), front);
	}
}

void render_draw_heal(int x, int y, int start, int front)
{
	int step, ticker;
	double light;

	init_bless();

	ticker = start + (int)tick;
	if (ticker > 62) {
		light = 1.0;
	} else {
		light = (ticker) / 62.0;
	}

	for (step = 0; step < 12 * 17; step += 17) {
		render_draw_heal_pix(
		    x, y, ticker + step + 0, IRGB((int)(24 * light), (int)(31 * light), (int)(24 * light)), front);
		render_draw_heal_pix(
		    x, y, ticker + step + 1, IRGB((int)(20 * light), (int)(28 * light), (int)(20 * light)), front);
		render_draw_heal_pix(
		    x, y, ticker + step + 2, IRGB((int)(16 * light), (int)(24 * light), (int)(16 * light)), front);
		render_draw_heal_pix(
		    x, y, ticker + step + 3, IRGB((int)(12 * light), (int)(20 * light), (int)(12 * light)), front);
		render_draw_heal_pix(
		    x, y, ticker + step + 4, IRGB((int)(8 * light), (int)(16 * light), (int)(8 * light)), front);
	}
}

void render_draw_potion(int x, int y, int ticker, int strength, int front)
{
	int step, nr;
	double light;

	if (!bless_init) {
		for (nr = 0; nr < 36; nr++) {
			bless_sin[nr] = (int)(sin((nr % 36) / 36.0 * M_PI * 2) * 8);
			bless_cos[nr] = (int)(cos((nr % 36) / 36.0 * M_PI * 2) * 16);
		}
		for (nr = 0; nr < 200; nr++) {
			bless_hight[nr] = -20 + (int)(sin((nr % 200) / 200.0 * M_PI * 2) * 20);
		}
		bless_init = 1;
	}


	if (ticker > 62) {
		light = 1.0;
	} else {
		light = (ticker) / 62.0;
	}

	for (step = 0; step < strength * 10; step += 17) {
		render_draw_bless_pix(
		    x, y, ticker + step + 0, IRGB((int)(31 * light), (int)(24 * light), (int)(24 * light)), front);
		render_draw_bless_pix(
		    x, y, ticker + step + 1, IRGB((int)(28 * light), (int)(20 * light), (int)(20 * light)), front);
		render_draw_bless_pix(
		    x, y, ticker + step + 2, IRGB((int)(24 * light), (int)(16 * light), (int)(16 * light)), front);
		render_draw_bless_pix(
		    x, y, ticker + step + 3, IRGB((int)(20 * light), (int)(12 * light), (int)(12 * light)), front);
		render_draw_bless_pix(
		    x, y, ticker + step + 4, IRGB((int)(16 * light), (int)(8 * light), (int)(8 * light)), front);
	}
}

void render_draw_rain(int x, int y, int ticker, int strength, int front)
{
	int step;

	for (step = -(strength * 100); step < 0; step += 237) {
		render_draw_rain_pix(x, y, -ticker + step + 0, IRGB(31, 24, 16), front);
		render_draw_rain_pix(x, y, -ticker + step + 1, IRGB(24, 16, 8), front);
		render_draw_rain_pix(x, y, -ticker + step + 2, IRGB(16, 8, 0), front);
	}
}

static void render_create_letter(unsigned char *rawrun, int sx, int sy, int val, char letter[64][64])
{
	int x = sx, y = sy;

	while (*rawrun != 255) {
		if (*rawrun == 254) {
			y++;
			x = sx;
			rawrun++;
			continue;
		}

		x += *rawrun++;

		letter[y][x] = (char)val;
	}
}

static unsigned char *render_create_rawrun(char letter[64][64])
{
	char *ptr, *fon, *last;
	int x, y, step;

	last = fon = ptr = xmalloc(8192, MEM_TEMP);

	for (y = sdl_scale * 3; y < 64; y++) {
		step = 0;
		for (x = sdl_scale * 3; x < 64; x++) {
			if (letter[y][x] == 2) {
				*ptr++ = (char)step;
				last = ptr;
				step = 1;
			} else {
				step++;
			}
		}
		*ptr++ = (char)254;
	}
	ptr = last;
	*ptr++ = (char)255;

	fon = xrealloc(fon, (size_t)(ptr - fon), MEM_GLOB);
	return (unsigned char *)fon;
}

static void create_shade_font(RenderFont *src, RenderFont *dst)
{
	char letter[64][64];
	int c, x, y;

	for (c = 0; c < 128; c++) {
		bzero(letter, sizeof(letter));
		// Create shadow in a diagonal pattern without gaps
		for (y = 0; y <= sdl_scale; y++) {
			for (x = 0; x <= sdl_scale; x++) {
				if (x > 0 || y > 0) { // Don't draw at (0,0)
					render_create_letter(src[c].raw, sdl_scale * 4 + x, sdl_scale * 4 + y, 2, letter);
				}
			}
		}
		// Draw the actual character on top
		render_create_letter(src[c].raw, sdl_scale * 4, sdl_scale * 4, 1, letter);
		dst[c].raw = render_create_rawrun(letter);
		dst[c].dim = src[c].dim;
	}
}

static void create_frame_font(RenderFont *src, RenderFont *dst)
{
	char letter[64][64];
	int c, x, y;

	for (c = 0; c < 128; c++) {
		bzero(letter, sizeof(letter));
		for (y = 0; y <= sdl_scale * 2; y += sdl_scale) {
			for (x = 0; x <= sdl_scale * 2; x += sdl_scale) {
				render_create_letter(src[c].raw, sdl_scale * 3 + x, sdl_scale * 3 + y, 2, letter);
			}
		}
		render_create_letter(src[c].raw, sdl_scale * 4, sdl_scale * 4, 1, letter);
		dst[c].raw = render_create_rawrun(letter);
		dst[c].dim = src[c].dim;
	}
}

int render_create_font_png(RenderFont *dst, uint32_t *pixel, int dx, int dy, int yoff, int scale)
{
	int c, x, y, sx, sy;
	char letter[64][64];
	(void)dy; // unused parameter

	for (c = 32; c < 128; c++) {
		if (c < 80) {
			sx = (c - 32) * 10 * scale;
			sy = yoff;
		} else {
			sx = (c - 80) * 10 * scale;
			sy = yoff + 20 * scale;
		}
		bzero(letter, sizeof(letter));

		for (x = 0; x < 10 * scale; x++) {
			for (y = 0; y < 12 * scale; y++) {
				if (pixel[sx + x + (sy + y) * dx] == 0xffffffff) {
					letter[y + sdl_scale * 3][x + sdl_scale * 3] = 2;
				}
			}
		}
		dst[c].raw = render_create_rawrun(letter);
	}
	return 1;
}

/**
 * Create font textures from PNG files.
 * Loads scaled font textures based on sdl_scale setting.
 * Creates normal, shaded, and framed variants for all three font sizes.
 */
void render_create_font(void)
{
	uint32_t *pixel;
	int dx, dy;

	if (fonta_shaded[0].raw) {
		return;
	}

	if (sdl_scale > 1) {
		if (sdl_scale == 2) {
			pixel = sdl_load_png("res/font2x.png", &dx, &dy);
		} else if (sdl_scale == 3) {
			pixel = sdl_load_png("res/font3x.png", &dx, &dy);
		} else if (sdl_scale == 4) {
			pixel = sdl_load_png("res/font4x.png", &dx, &dy);
		} else {
			fail("Scale not supported in render_create_font!");
			pixel = NULL;
		}
		if (!pixel) {
			return;
		}

		render_create_font_png(fonta, pixel, dx, dy, 40 * sdl_scale, sdl_scale);
		render_create_font_png(fontb, pixel, dx, dy, 0, sdl_scale);
		render_create_font_png(fontc, pixel, dx, dy, 80 * sdl_scale, sdl_scale);
#ifdef SDL_FAST_MALLOC
		FREE(pixel);
#else
		xfree(pixel);
#endif
	}

	create_shade_font(fonta, fonta_shaded);
	create_shade_font(fontb, fontb_shaded);
	create_shade_font(fontc, fontc_shaded);

	create_frame_font(fonta, fonta_framed);
	create_frame_font(fontb, fontb_framed);
	create_frame_font(fontc, fontc_framed);
}

RenderFont *textfont = fonta;
int textdisplay_dy = 10;

int render_text_char(int sx, int sy, int c, unsigned short int color)
{
	if (c > 127 || c < 0) {
		return 0;
	}

	return sdl_drawtext(sx, sy, color, 0, (char *)&c, textfont, clipsx, clipsy, clipex, clipey, x_offset, y_offset) -
	       sx;
}

static int render_text_len_internal(const char *text)
{
	int x;
	const char *c;

	for (x = 0, c = text; *c; c++) {
		x += textfont[(unsigned char)*c].dim;
	}

	return (int)((float)x + 0.5f);
}

int render_char_len(char c)
{
	return textfont[(unsigned char)c].dim;
}

// ---------------------> Chat Window <-----------------------------
#define MAXTEXTLINES   256
#define MAXTEXTLETTERS 256

#define TEXTDISPLAY_DY (textdisplay_dy)

#define TEXTDISPLAY_SX 396
#define TEXTDISPLAY_SY (__textdisplay_sy)

#define TEXTDISPLAYLINES (TEXTDISPLAY_SY / TEXTDISPLAY_DY)

int textnextline = 0, textdisplayline = 0, textlines = 0;

struct letter {
	char c;
	unsigned char color;
	unsigned char link;
};

static struct letter text_storage[MAXTEXTLINES * MAXTEXTLETTERS];
struct letter *text = text_storage;

unsigned short palette[256];

/**
 * Initialize chat window text system.
 * Allocates text buffer and sets up color palette for different message types.
 */
void render_init_text(void)
{
	palette[0] = IRGB(31, 31, 31); // normal white text (talk, game messages)
	palette[1] = IRGB(16, 16, 16); // dark gray text (now entering ...)
	palette[2] = IRGB(16, 31, 16); // light green (normal chat)
	palette[3] = IRGB(31, 16, 16); // light red (announce)
	palette[4] = IRGB(16, 16, 31); // light blue (text links)
	palette[5] = IRGB(24, 24, 31); // orange (item desc headings)
	palette[6] = IRGB(31, 31, 16); // yellow (tells)
	palette[7] = IRGB(16, 24, 31); // violet (staff chat)
	palette[8] = IRGB(24, 24, 31); // light violet (god chat)

	palette[9] = IRGB(24, 24, 16); // chat - auction
	palette[10] = IRGB(24, 16, 24); // chat - grats
	palette[11] = IRGB(16, 24, 24); // chat	- mirror
	palette[12] = IRGB(31, 24, 16); // chat - info
	palette[13] = IRGB(31, 16, 24); // chat - area
	palette[14] = IRGB(16, 31, 24); // chat - v2, games
	palette[15] = IRGB(24, 31, 16); // chat - public clan
	palette[16] = IRGB(24, 16, 31); // chat	- internal clan

	palette[17] = IRGB(31, 31, 31); // fake white text (hidden links)
}

void render_set_textfont(int nr)
{
	switch (nr) {
	case 0:
		textfont = fonta;
		textdisplay_dy = 10;
		break;
	case 1:
		textfont = fontc;
		textdisplay_dy = 12;
		break;
	}
	bzero(text, MAXTEXTLINES * MAXTEXTLETTERS * sizeof(struct letter));
	textnextline = textdisplayline = textlines = 0;
}

/**
 * Render the chat window text.
 * Displays visible lines from the circular text buffer with color coding and links.
 */
void render_display_text(void)
{
	int n, m, rn, x, y, pos;
	char buf[256], *bp;
	unsigned short lastcolor = (unsigned short)-1;

	for (n = textdisplayline, y = doty(DOT_TXT); y <= doty(DOT_TXT) + TEXTDISPLAY_SY - TEXTDISPLAY_DY;
	    n++, y += TEXTDISPLAY_DY) {
		rn = n % MAXTEXTLINES;

		x = dotx(DOT_TXT);
		pos = rn * MAXTEXTLETTERS;

		bp = buf;
		for (m = 0; m < MAXTEXTLETTERS; m++, pos++) {
			if (text[pos].c == 0) {
				break;
			}

			if (lastcolor != text[pos].color || text[pos].c < 32) {
				if (bp != buf) {
					*bp = 0;
					if (game_options & GO_LARGE) {
						x = render_text(x, y, palette[lastcolor], RENDER_TEXT_BIG, buf);
					} else {
						x = render_text(x, y, palette[lastcolor], 0, buf);
					}
				}
				bp = buf;
				lastcolor = text[pos].color;
			}

			if (text[pos].c < 32) {
				int i;

				x = ((int)text[pos].c) * 12 + dotx(DOT_TXT);

				// better display for numbers
				for (i = pos + 1; isdigit(text[i].c) || text[i].c == '-'; i++) {
					x -= textfont[(unsigned char)text[i].c].dim;
				}
				continue;
			}

			*bp++ = text[pos].c;
		}

		if (bp != buf) {
			*bp = 0;
			if (game_options & GO_LARGE) {
				render_text(x, y, palette[lastcolor], RENDER_TEXT_BIG, buf);
			} else {
				render_text(x, y, palette[lastcolor], 0, buf);
			}
		}
	}
}

/**
 * Add a line of text to the chat window.
 * Handles word wrapping, color codes, and clickable links.
 *
 */
void render_add_text(char *ptr)
{
	int n, m, pos, color = 0, link = 0;
	int x = 0, tmp;
	char buf[256];

	pos = textnextline * MAXTEXTLETTERS;
	bzero(text + pos, sizeof(struct letter) * MAXTEXTLETTERS);

	while (*ptr) {
		while (*ptr == ' ') {
			ptr++;
		}
		while (*ptr == RENDER_TEXT_TERMINATOR) {
			ptr++;
			switch (*ptr) {
			case 'c':
				tmp = atoi(ptr + 1);
				if (tmp == 18) {
					link = 0;
				} else if (tmp != 17) {
					color = tmp;
					link = 0;
				}
				if (tmp == 4) {
					link = 1;
				} else if (tmp == 17) {
					link = 2;
				}
				ptr++;
				while (isdigit(*ptr)) {
					ptr++;
				}
				break;
			default:
				ptr++;
				break;
			}
		}
		while (*ptr == ' ') {
			ptr++;
		}

		n = 0;
		while (*ptr && *ptr != ' ' && *ptr != RENDER_TEXT_TERMINATOR && n < 49) {
			buf[n++] = *ptr++;
		}
		buf[n] = 0;

		if (x + (tmp = render_text_len_internal(buf)) >= TEXTDISPLAY_SX) {
			if (textdisplayline == (textnextline + (MAXTEXTLINES - TEXTDISPLAYLINES)) % MAXTEXTLINES) {
				textdisplayline = (textdisplayline + 1) % MAXTEXTLINES;
			}
			textnextline = (textnextline + 1) % MAXTEXTLINES;
			pos = textnextline * MAXTEXTLETTERS;
			bzero(text + pos, sizeof(struct letter) * MAXTEXTLETTERS);
			x = tmp;

			for (m = 0; m < 2; m++) {
				text[pos].c = 32;
				x += textfont[32].dim;
				text[pos].color = (unsigned char)color;
				text[pos].link = (unsigned char)link;
				pos++;
			}

		} else {
			x += tmp;
		}

		for (m = 0; m < n; m++, pos++) {
			text[pos].c = buf[m];
			text[pos].color = (unsigned char)color;
			text[pos].link = (unsigned char)link;
		}
		text[pos].c = 32;
		x += textfont[32].dim;
		text[pos].color = (unsigned char)color;
		text[pos].link = (unsigned char)link;

		pos++;
	}
	pos = (pos + MAXTEXTLETTERS * MAXTEXTLINES - 1) % (MAXTEXTLETTERS * MAXTEXTLINES);
	if (text[pos].c == 32) {
		text[pos].c = 0;
		text[pos].color = 0;
		text[pos].link = 0;
	}

	if (textdisplayline == (textnextline + (MAXTEXTLINES - TEXTDISPLAYLINES)) % MAXTEXTLINES) {
		textdisplayline = (textdisplayline + 1) % MAXTEXTLINES;
	}

	textnextline = (textnextline + 1) % MAXTEXTLINES;
	if (textnextline == textdisplayline) {
		textdisplayline = (textdisplayline + 1) % MAXTEXTLINES;
	}
	textlines++;
}

int render_text_init_done(void)
{
	return text != NULL;
}

/**
 * Check if mouse is over clickable text in chat window.
 * Extracts the clicked text link if found.
 *
 * @return Link type (1 or 2) if over link, 0 otherwise
 */
int render_scantext(int x, int y, char *hit)
{
	int n, m, pos, panic = 0, tmp = 0;
	int dx, link;

	if (x < dotx(DOT_TXT) || y < doty(DOT_TXT)) {
		return 0;
	}
	if (x > dotx(DOT_TXT) + TEXTDISPLAY_SX) {
		return 0;
	}
	if (y > doty(DOT_TXT) + TEXTDISPLAY_SY) {
		return 0;
	}

	n = (y - doty(DOT_TXT)) / TEXTDISPLAY_DY;
	n = (n + textdisplayline) % MAXTEXTLINES;

	for (pos = n * MAXTEXTLETTERS, dx = m = 0; m < MAXTEXTLETTERS && text[pos].c; m++, pos++) {
		if (text[pos].c > 0 && text[pos].c < 32) {
			dx = ((int)text[pos].c) * 12;
			for (int i = pos + 1; isdigit(text[i].c) || text[i].c == '-'; i++) {
				dx -= textfont[(unsigned char)text[i].c].dim;
			}
			continue;
		}

		dx += textfont[(unsigned char)text[pos].c].dim;

		if (dx + dotx(DOT_TXT) > x) {
			if ((link = text[pos].link)) { // link palette color
				while ((text[pos].link || text[pos].c == 0) && panic++ < 5000) {
					pos--;
					if (pos < 0) {
						pos = MAXTEXTLETTERS * MAXTEXTLINES - 1;
					}
				}

				pos++;
				if (pos == MAXTEXTLETTERS * MAXTEXTLINES) {
					pos = 0;
				}
				while ((text[pos].link || text[pos].c == 0) && panic++ < 5000 && tmp < 80) {
					if (tmp > 0 && text[pos].c == ' ' && hit[tmp - 1] == ' ')
						;
					else if (text[pos].c) {
						hit[tmp++] = text[pos].c;
					}
					pos++;
					if (pos == MAXTEXTLETTERS * MAXTEXTLINES) {
						pos = 0;
					}
				}
				if (tmp > 0 && hit[tmp - 1] == ' ') {
					hit[tmp - 1] = 0;
				} else {
					hit[tmp] = 0;
				}
				return link;
			}
			return 0;
		}
	}
	return 0;
}

void render_list_text(void)
{
	note("textlines=%d, textdisplayline=%d", textlines, textdisplayline);
}

void render_text_lineup(void)
{
	int oldest_line;

	// Can't scroll up if all lines fit on screen
	if (textlines <= TEXTDISPLAYLINES) {
		return;
	}

	// Calculate the oldest valid line in the circular buffer
	if (textlines >= MAXTEXTLINES) {
		// Buffer is full - oldest line is right after textnextline
		oldest_line = textnextline;
	} else {
		// Buffer not full - oldest line is at index 0
		oldest_line = 0;
	}

	// Don't scroll past the oldest line
	if (textdisplayline == oldest_line) {
		return;
	}

	// Move display one line up (toward older text)
	textdisplayline = (textdisplayline + MAXTEXTLINES - 1) % MAXTEXTLINES;
}

void render_text_linedown(void)
{
	int bottom_edge_line;

	// Can't scroll down if all lines fit on screen
	if (textlines <= TEXTDISPLAYLINES) {
		return;
	}

	// Calculate where the bottom edge of the display window would be
	bottom_edge_line = (textdisplayline + TEXTDISPLAYLINES) % MAXTEXTLINES;

	// Don't scroll past the newest line (textnextline)
	// The bottom of our window should stop at textnextline
	if (bottom_edge_line == textnextline) {
		return;
	}

	// Move display one line down (toward newer text)
	textdisplayline = (textdisplayline + 1) % MAXTEXTLINES;
}

void render_text_pageup(void)
{
	int n;

	for (n = 0; n < TEXTDISPLAYLINES; n++) {
		render_text_lineup();
	}
}

void render_text_pagedown(void)
{
	int n;

	for (n = 0; n < TEXTDISPLAYLINES; n++) {
		render_text_linedown();
	}
}

/**
 * Set global rendering offset.
 * Used for window scaling and positioning.
 *
 */
void render_set_offset(int x, int y)
{
	x_offset = x;
	y_offset = y;
}

/**
 * Get current X rendering offset.
 *
 * @return X offset in pixels
 */
int render_offset_x(void)
{
	return x_offset;
}

/**
 * Get current Y rendering offset.
 *
 * @return Y offset in pixels
 */
int render_offset_y(void)
{
	return y_offset;
}
