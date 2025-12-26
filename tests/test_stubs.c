/*
 * Test Stubs - Minimal implementations of game functions for unit testing
 * 
 * These stubs allow SDL code to link without the full game engine.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <SDL2/SDL.h>

// ============================================================================
// Logging stubs
// ============================================================================

void note(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
}

char *fail(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	fprintf(stderr, "FAIL: ");
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	return "test failure";
}

void paranoia(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	fprintf(stderr, "PARANOIA: ");
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	abort();  // Fail fast on paranoia checks
}

void warn(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	fprintf(stderr, "WARN: ");
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
}

// ============================================================================
// Game state stubs
// ============================================================================

int quit = 0;
uint64_t game_options = 0;
char *localdata = NULL;
int xmemcheck_failed = 0;

// SDL worker thread globals (defined in sdl_core.c, not here)
// extern SDL_AtomicInt worker_quit;
// extern SDL_Thread **worker_threads;
// extern struct zip_handles *worker_zips;

// ============================================================================
// Render stubs
// ============================================================================

void render_set_offset(int x __attribute__((unused)), int y __attribute__((unused)))
{
	// No-op in tests
}

// ============================================================================
// GUI stubs
// ============================================================================

void gui_sdl_mouseproc(int x __attribute__((unused)), int y __attribute__((unused)), int b __attribute__((unused)))
{
	// No-op in tests
}

void gui_sdl_keyproc(int key __attribute__((unused)), int state __attribute__((unused)))
{
	// No-op in tests
}

void context_keyup(int key __attribute__((unused)))
{
	// No-op in tests
}

void cmd_proc(const char *cmd __attribute__((unused)))
{
	// No-op in tests
}

void display_messagebox(const char *title __attribute__((unused)), const char *msg __attribute__((unused)))
{
	fprintf(stderr, "MessageBox: %s - %s\n", title ? title : "(no title)", msg ? msg : "(no message)");
}

// ============================================================================
// Audio stubs (SDL_mixer)
// ============================================================================

int Mix_OpenAudio(int frequency __attribute__((unused)), uint16_t format __attribute__((unused)),
    int channels __attribute__((unused)), int chunksize __attribute__((unused)))
{
	return 0; // Success
}

int Mix_AllocateChannels(int numchans __attribute__((unused)))
{
	return 0;
}

void Mix_Quit(void)
{
	// No-op
}

// ============================================================================
// Random number stub
// ============================================================================

int rrand(int min, int max)
{
	if (max <= min)
		return min;
	return min + (rand() % (max - min + 1));
}

// ============================================================================
// Additional SDL stubs for render operations
// ============================================================================

// These are called by sdl_draw.c but we don't actually render in tests
int SDL_SetRenderDrawBlendMode(SDL_Renderer *renderer __attribute__((unused)),
    SDL_BlendMode blendMode __attribute__((unused)))
{
	return 0;
}

int SDL_SetTextureBlendMode(SDL_Texture *texture __attribute__((unused)), SDL_BlendMode blendMode __attribute__((unused)))
{
	return 0;
}

int SDL_SetTextureAlphaMod(SDL_Texture *texture __attribute__((unused)), Uint8 alpha __attribute__((unused)))
{
	return 0;
}
