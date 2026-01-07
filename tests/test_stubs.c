/*
 * Test Stubs - Minimal implementations of game functions for unit testing
 * 
 * These stubs allow SDL code to link without the full game engine.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

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
	abort(); // Fail fast on paranoia checks
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

void gui_sdl_mouseproc(float x __attribute__((unused)), float y __attribute__((unused)), int b __attribute__((unused)))
{
	// No-op in tests
}

void gui_sdl_keyproc(SDL_Keycode key __attribute__((unused)))
{
	// No-op in tests
}

void context_keyup(SDL_Keycode key __attribute__((unused)))
{
	// No-op in tests
}

void cmd_proc(int key __attribute__((unused)))
{
	// No-op in tests
}

void display_messagebox(const char *title __attribute__((unused)), const char *msg __attribute__((unused)))
{
	fprintf(stderr, "MessageBox: %s - %s\n", title ? title : "(no title)", msg ? msg : "(no message)");
}

// ============================================================================
// Audio stubs (SDL3_mixer)
// ============================================================================

// Prevent audio initialization in tests (game_options has GO_SOUND disabled)
// These stubs are here in case the linker needs them

// ============================================================================
// Random number stub
// ============================================================================

int rrand(int min, int max)
{
	if (max <= min) {
		return min;
	}
	return min + (rand() % (max - min + 1));
}

// ============================================================================
// Additional SDL stubs for render operations
// ============================================================================

// Note: SDL_SetRenderDrawBlendMode and other render stubs are now in sdl_test.c
// with render call counters for test verification.

bool SDL_SetTextureBlendMode(
    SDL_Texture *texture __attribute__((unused)), SDL_BlendMode blendMode __attribute__((unused)))
{
	return true;
}

bool SDL_SetTextureAlphaMod(SDL_Texture *texture __attribute__((unused)), uint8_t alpha __attribute__((unused)))
{
	return true;
}
