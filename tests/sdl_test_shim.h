/*
 * Headless SDL Shim for Tests
 *
 * Minimal stand-in for SDL_Texture and related functions when UNIT_TEST is defined.
 * No GPU handles, just dimensions + metadata in memory.
 */

#ifndef SDL_TEST_SHIM_H
#define SDL_TEST_SHIM_H

#ifdef UNIT_TEST

#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL_blendmode.h>

// Minimal stand-in for SDL_Texture; no GPU handle, just dimensions + metadata.
typedef struct SDL_Texture {
	int w;
	int h;
	uint32_t format;
	int access;
} SDL_Texture;

static inline SDL_Texture *SDL_CreateTexture(void *renderer_unused, uint32_t format, int access, int w, int h)
{
	(void)renderer_unused;
	SDL_Texture *tex = malloc(sizeof(SDL_Texture));
	if (!tex)
		return NULL;
	tex->w = w;
	tex->h = h;
	tex->format = format;
	tex->access = access;
	return tex;
}

static inline void SDL_DestroyTexture(SDL_Texture *tex)
{
	free(tex);
}

static inline int SDL_QueryTexture(SDL_Texture *tex, uint32_t *format, int *access, int *w, int *h)
{
	if (!tex)
		return -1;
	if (format)
		*format = tex->format;
	if (access)
		*access = tex->access;
	if (w)
		*w = tex->w;
	if (h)
		*h = tex->h;
	return 0;
}

static inline int SDL_UpdateTexture(SDL_Texture *tex, const void *r, const void *pixels, int pitch)
{
	(void)tex;
	(void)r;
	(void)pixels;
	(void)pitch;
	return 0;
}

static inline int SDL_SetTextureBlendMode(SDL_Texture *tex, SDL_BlendMode mode)
{
	(void)tex;
	(void)mode;
	return 0;
}

#endif // UNIT_TEST

#endif // SDL_TEST_SHIM_H
