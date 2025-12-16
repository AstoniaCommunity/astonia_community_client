/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * SDL - Core Module
 *
 * Initialization, lifecycle management, window management, cursor handling,
 * event loop, and background prefetching system.
 */

#include <SDL2/SDL_timer.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <zip.h>

#include "dll.h"
#include "astonia.h"
#include "sdl/sdl.h"
#include "sdl/sdl_private.h"
#include "gui/gui.h"

// SDL window and renderer
SDL_Window *sdlwnd = NULL;
SDL_Renderer *sdlren = NULL;

// Cursors
static SDL_Cursor *curs[20];

// Zip archives for graphics
zip_t *sdl_zip1 = NULL;
zip_t *sdl_zip2 = NULL;
zip_t *sdl_zip1p = NULL;
zip_t *sdl_zip2p = NULL;
zip_t *sdl_zip1m = NULL;
zip_t *sdl_zip2m = NULL;

// Prefetch threading (shared with sdl_texture.c)
SDL_sem *prework = NULL;
SDL_mutex *premutex = NULL;

// Scale and resolution settings
DLL_EXPORT int sdl_scale = 1;
DLL_EXPORT int sdl_frames = 0;
DLL_EXPORT int sdl_multi = 4;
DLL_EXPORT int sdl_cache_size = 8000;
DLL_EXPORT int __yres = YRES0;

// Worker thread management

struct zip_handles;
struct zip_handles *worker_zips = NULL;
SDL_atomic_t worker_quit;
SDL_Thread **worker_threads = NULL;

// Image loading state machine (shared with sdl_image.c)
static int sdli_state_storage[MAXSPRITE];
int *sdli_state = sdli_state_storage;

void sdl_dump(FILE *fp)
{
	fprintf(fp, "SDL datadump:\n");

	fprintf(fp, "XRES: %d\n", XRES);
	fprintf(fp, "YRES: %d\n", YRES);

	fprintf(fp, "sdl_scale: %d\n", sdl_scale);
	fprintf(fp, "sdl_frames: %d\n", sdl_frames);
	fprintf(fp, "sdl_multi: %d\n", sdl_multi);
	fprintf(fp, "sdl_cache_size: %d (max=%d)\n", sdl_cache_size, MAX_TEXCACHE);

	fprintf(fp, "mem_png: %lld\n", (long long)__atomic_load_n(&mem_png, __ATOMIC_RELAXED));
	fprintf(fp, "mem_tex: %lld\n", (long long)__atomic_load_n(&mem_tex, __ATOMIC_RELAXED));
	fprintf(fp, "texc_hit: %lld\n", texc_hit);
	fprintf(fp, "texc_miss: %lld\n", texc_miss);
	fprintf(fp, "texc_pre: %lld\n", texc_pre);

	fprintf(fp, "\n");
}

#define GO_DEFAULTS (GO_CONTEXT | GO_ACTION | GO_BIGBAR | GO_PREDICT | GO_SHORT | GO_MAPSAVE)

// #define GO_DEFAULTS (GO_CONTEXT|GO_ACTION|GO_BIGBAR|GO_PREDICT|GO_SHORT|GO_MAPSAVE|GO_NOMAP)

int sdl_init(int width, int height, char *title)
{
	int i;
	SDL_DisplayMode DM;

	if (SDL_Init(SDL_INIT_VIDEO | ((game_options & GO_SOUND) ? SDL_INIT_AUDIO : 0)) != 0) {
		fail("SDL_Init Error: %s", SDL_GetError());
		return 0;
	}

	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
	SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, "1");

	SDL_GetCurrentDisplayMode(0, &DM);

	if (!width || !height) {
		width = DM.w;
		height = DM.h;
	}

	sdlwnd = SDL_CreateWindow(title, DM.w / 2 - width / 2, DM.h / 2 - height / 2, width, height, SDL_WINDOW_SHOWN);
	if (!sdlwnd) {
		fail("SDL_Init Error: %s", SDL_GetError());
		SDL_Quit();
		return 0;
	}

	if (game_options & GO_FULL) {
		SDL_SetWindowFullscreen(sdlwnd, SDL_WINDOW_FULLSCREEN); // true full screen
	} else if (DM.w == width && DM.h == height) {
		SDL_SetWindowFullscreen(sdlwnd, SDL_WINDOW_FULLSCREEN_DESKTOP); // borderless windowed
	}

	sdlren = SDL_CreateRenderer(sdlwnd, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!sdlren) {
		SDL_DestroyWindow(sdlwnd);
		fail("SDL_Init Error: %s", SDL_GetError());
		SDL_Quit();
		return 0;
	}

	// Initialize hash table (statically allocated)
	for (i = 0; i < MAX_TEXHASH; i++) {
		sdlt_cache[i] = STX_NONE;
	}

	// Initialize texture cache (statically allocated)
	for (i = 0; i < MAX_TEXCACHE; i++) {
		// Initialize flags atomically
		uint16_t *flags_ptr = (uint16_t *)&sdlt[i].flags;
		__atomic_store_n(flags_ptr, 0, __ATOMIC_RELAXED);
		sdlt[i].prev = i - 1;
		sdlt[i].next = i + 1;
		sdlt[i].hnext = STX_NONE;
		sdlt[i].hprev = STX_NONE;
		// Initialize new fields:
		// Generation starts at 1 (0 is reserved for "never valid for jobs")
		sdlt[i].generation = 1;
		sdlt[i].work_state = TX_WORK_IDLE;
	}
	sdlt[0].prev = STX_NONE;
	sdlt[MAX_TEXCACHE - 1].next = STX_NONE;
	sdlt_best = 0;
	sdlt_last = MAX_TEXCACHE - 1;

	// Initialize the new texture job queue
	tex_jobs_init();

	SDL_RaiseWindow(sdlwnd);

	// We want SDL to translate scan codes to ASCII / Unicode
	// but we don't really want the SDL line editing stuff.
	// I hope just keeping it enabled all the time doesn't break
	// anything.
	SDL_StartTextInput();

	// decide on screen format
	if (width != XRES || height != YRES) {
		int tmp_scale = 1, off = 0;

		if (width / XRES >= 4 && height / YRES0 >= 4) {
			sdl_scale = 4;
		} else if (width / XRES >= 3 && height / YRES0 >= 3) {
			sdl_scale = 3;
		} else if (width / XRES >= 2 && height / YRES0 >= 2) {
			sdl_scale = 2;
		}

		if (width / XRES >= 4 && height / YRES2 >= 4) {
			tmp_scale = 4;
		} else if (width / XRES >= 3 && height / YRES2 >= 3) {
			tmp_scale = 3;
		} else if (width / XRES >= 2 && height / YRES2 >= 2) {
			tmp_scale = 2;
		}

		if (tmp_scale > sdl_scale || height < YRES0) {
			sdl_scale = tmp_scale;
			YRES = height / sdl_scale;
		}

		YRES = height / sdl_scale;

		if (game_options & GO_SMALLTOP) {
			off += 40;
		}
		if (game_options & GO_SMALLBOT) {
			off += 40;
		}

		if (YRES > YRES1 - off) {
			YRES = YRES1 - off;
		}

		render_set_offset((width / sdl_scale - XRES) / 2, (height / sdl_scale - YRES) / 2);
	}
	if (game_options & GO_NOTSET) {
		if (YRES >= 620) {
			game_options = GO_DEFAULTS;
		} else if (YRES >= 580) {
			game_options = GO_DEFAULTS | GO_SMALLBOT;
		} else {
			game_options = GO_DEFAULTS | GO_SMALLBOT | GO_SMALLTOP;
		}
	}
	note("SDL using %dx%d scale %d, options=%" PRIu64, XRES, YRES, sdl_scale, game_options);

	sdl_create_cursors();

	sdl_zip1 = zip_open("res/gx1.zip", ZIP_RDONLY, NULL);
	sdl_zip1p = zip_open("res/gx1_patch.zip", ZIP_RDONLY, NULL);
	sdl_zip1m = zip_open("res/gx1_mod.zip", ZIP_RDONLY, NULL);

	switch (sdl_scale) {
	case 2:
		sdl_zip2 = zip_open("res/gx2.zip", ZIP_RDONLY, NULL);
		sdl_zip2p = zip_open("res/gx2_patch.zip", ZIP_RDONLY, NULL);
		sdl_zip2m = zip_open("res/gx2_mod.zip", ZIP_RDONLY, NULL);
		break;
	case 3:
		sdl_zip2 = zip_open("res/gx3.zip", ZIP_RDONLY, NULL);
		sdl_zip2p = zip_open("res/gx3_patch.zip", ZIP_RDONLY, NULL);
		sdl_zip2m = zip_open("res/gx3_mod.zip", ZIP_RDONLY, NULL);
		break;
	case 4:
		sdl_zip2 = zip_open("res/gx4.zip", ZIP_RDONLY, NULL);
		sdl_zip2p = zip_open("res/gx4_patch.zip", ZIP_RDONLY, NULL);
		sdl_zip2m = zip_open("res/gx4_mod.zip", ZIP_RDONLY, NULL);
		break;
	default:
		break;
	}

	if ((game_options & GO_SOUND) && Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
		warn("initializing audio failed");
		game_options &= ~GO_SOUND;
	}

	if (game_options & GO_SOUND) {
		int number_of_sound_channels = Mix_AllocateChannels(MAX_SOUND_CHANNELS);
		note("Allocated %d sound channels", number_of_sound_channels);
	}

	// Initialize mutex unconditionally (needed for job queue even in single-threaded mode)
	premutex = SDL_CreateMutex();
	if (!premutex) {
		fail("Failed to create premutex");
		return 0;
	}

	// Initialize semaphore for waking background workers (starts at 0)
	prework = SDL_CreateSemaphore(0);
	if (!prework) {
		fail("Failed to create prework semaphore");
		return 0;
	}

	SDL_AtomicSet(&worker_quit, 0);

	if (sdl_multi) {
		char buf[80];
		int n;

		// Allocate worker zip handles
		worker_zips = xmalloc((size_t)sdl_multi * sizeof(struct zip_handles), MEM_SDL_BASE);
		if (!worker_zips) {
			fail("Out of memory for worker zip handles");
			sdl_multi = 0;
		} else {
			// Open zip handles for each worker
			for (n = 0; n < sdl_multi; n++) {
				worker_zips[n].zip1 = zip_open("res/gx1.zip", ZIP_RDONLY, NULL);
				worker_zips[n].zip1p = zip_open("res/gx1_patch.zip", ZIP_RDONLY, NULL);
				worker_zips[n].zip1m = zip_open("res/gx1_mod.zip", ZIP_RDONLY, NULL);

				switch (sdl_scale) {
				case 2:
					worker_zips[n].zip2 = zip_open("res/gx2.zip", ZIP_RDONLY, NULL);
					worker_zips[n].zip2p = zip_open("res/gx2_patch.zip", ZIP_RDONLY, NULL);
					worker_zips[n].zip2m = zip_open("res/gx2_mod.zip", ZIP_RDONLY, NULL);
					break;
				case 3:
					worker_zips[n].zip2 = zip_open("res/gx3.zip", ZIP_RDONLY, NULL);
					worker_zips[n].zip2p = zip_open("res/gx3_patch.zip", ZIP_RDONLY, NULL);
					worker_zips[n].zip2m = zip_open("res/gx3_mod.zip", ZIP_RDONLY, NULL);
					break;
				case 4:
					worker_zips[n].zip2 = zip_open("res/gx4.zip", ZIP_RDONLY, NULL);
					worker_zips[n].zip2p = zip_open("res/gx4_patch.zip", ZIP_RDONLY, NULL);
					worker_zips[n].zip2m = zip_open("res/gx4_mod.zip", ZIP_RDONLY, NULL);
					break;
				default:
					break;
				}

				if (!worker_zips[n].zip1) {
					warn("Worker %d: Failed to open res/gx1.zip - aborting initialization", n);
					// Clean up already opened zips
					for (int i = 0; i < n; i++) {
						if (worker_zips[i].zip1) {
							zip_close(worker_zips[i].zip1);
						}
						if (worker_zips[i].zip1p) {
							zip_close(worker_zips[i].zip1p);
						}
						if (worker_zips[i].zip1m) {
							zip_close(worker_zips[i].zip1m);
						}
						if (worker_zips[i].zip2) {
							zip_close(worker_zips[i].zip2);
						}
						if (worker_zips[i].zip2p) {
							zip_close(worker_zips[i].zip2p);
						}
						if (worker_zips[i].zip2m) {
							zip_close(worker_zips[i].zip2m);
						}
					}
					xfree(worker_zips);
					worker_zips = NULL;
					sdl_multi = 0;
					break;
				}
			}

			// Only create threads if all zip handles succeeded
			if (sdl_multi > 0) {
				worker_threads = xmalloc((size_t)sdl_multi * sizeof(SDL_Thread *), MEM_SDL_BASE);
				if (!worker_threads) {
					fail("Out of memory for thread handles");
					// Clean up zip handles
					for (n = 0; n < sdl_multi; n++) {
						if (worker_zips[n].zip1) {
							zip_close(worker_zips[n].zip1);
						}
						if (worker_zips[n].zip1p) {
							zip_close(worker_zips[n].zip1p);
						}
						if (worker_zips[n].zip1m) {
							zip_close(worker_zips[n].zip1m);
						}
						if (worker_zips[n].zip2) {
							zip_close(worker_zips[n].zip2);
						}
						if (worker_zips[n].zip2p) {
							zip_close(worker_zips[n].zip2p);
						}
						if (worker_zips[n].zip2m) {
							zip_close(worker_zips[n].zip2m);
						}
					}
					xfree(worker_zips);
					worker_zips = NULL;
					sdl_multi = 0;
				} else {
					// Create all threads
					for (n = 0; n < sdl_multi; n++) {
						sprintf(buf, "moac background worker %d", n);
						worker_threads[n] = SDL_CreateThread(sdl_pre_backgnd, buf, (void *)(long long)n);
						if (!worker_threads[n]) {
							warn("Failed to create worker thread %d", n);
							// Signal quit and join already created threads
							SDL_AtomicSet(&worker_quit, 1);
							for (int i = 0; i < n; i++) {
								if (worker_threads[i]) {
									SDL_WaitThread(worker_threads[i], NULL);
								}
							}
							// Clean up
							xfree(worker_threads);
							worker_threads = NULL;
							for (int i = 0; i < sdl_multi; i++) {
								if (worker_zips[i].zip1) {
									zip_close(worker_zips[i].zip1);
								}
								if (worker_zips[i].zip1p) {
									zip_close(worker_zips[i].zip1p);
								}
								if (worker_zips[i].zip1m) {
									zip_close(worker_zips[i].zip1m);
								}
								if (worker_zips[i].zip2) {
									zip_close(worker_zips[i].zip2);
								}
								if (worker_zips[i].zip2p) {
									zip_close(worker_zips[i].zip2p);
								}
								if (worker_zips[i].zip2m) {
									zip_close(worker_zips[i].zip2m);
								}
							}
							xfree(worker_zips);
							worker_zips = NULL;
							sdl_multi = 0;
							break;
						}
					}
				}
			}
		}
	}

	return 1;
}

int sdl_clear(void)
{
	// SDL_SetRenderDrawColor(sdlren,255,63,63,255);     // clear with bright red to spot broken sprites
	SDL_SetRenderDrawColor(sdlren, 0, 0, 0, 255);
	SDL_RenderClear(sdlren);
	// note("mem: %.2fM PNG, %.2fM Tex, Hit: %ld, Miss: %ld, Max:
	// %d\n",mem_png/(1024.0*1024.0),mem_tex/(1024.0*1024.0),texc_hit,texc_miss,maxpanic);
	maxpanic = 0;
	return 1;
}

int sdl_render(void)
{
	SDL_RenderPresent(sdlren);
	sdl_frames++;
	return 1;
}

void sdl_exit(void)
{
	// Signal workers to quit and join them
	if (sdl_multi && worker_threads) {
		SDL_AtomicSet(&worker_quit, 1);

		// Wake all workers so they can see the quit signal
		// (they're blocked on SDL_SemWait)
		if (prework) {
			for (int n = 0; n < sdl_multi; n++) {
				SDL_SemPost(prework);
			}
		}

		for (int n = 0; n < sdl_multi; n++) {
			if (worker_threads[n]) {
				SDL_WaitThread(worker_threads[n], NULL);
			}
		}
		xfree(worker_threads);
		worker_threads = NULL;
	}

	// Close worker zip handles
	if (worker_zips) {
		for (int n = 0; n < sdl_multi; n++) {
			if (worker_zips[n].zip1) {
				zip_close(worker_zips[n].zip1);
			}
			if (worker_zips[n].zip1p) {
				zip_close(worker_zips[n].zip1p);
			}
			if (worker_zips[n].zip1m) {
				zip_close(worker_zips[n].zip1m);
			}
			if (worker_zips[n].zip2) {
				zip_close(worker_zips[n].zip2);
			}
			if (worker_zips[n].zip2p) {
				zip_close(worker_zips[n].zip2p);
			}
			if (worker_zips[n].zip2m) {
				zip_close(worker_zips[n].zip2m);
			}
		}
		xfree(worker_zips);
		worker_zips = NULL;
	}

	if (sdl_zip1) {
		zip_close(sdl_zip1);
	}
	if (sdl_zip1m) {
		zip_close(sdl_zip1m);
	}
	if (sdl_zip1p) {
		zip_close(sdl_zip1p);
	}
	if (sdl_zip2) {
		zip_close(sdl_zip2);
	}
	if (sdl_zip2m) {
		zip_close(sdl_zip2m);
	}
	if (sdl_zip2p) {
		zip_close(sdl_zip2p);
	}

	if (prework) {
		SDL_DestroySemaphore(prework);
		prework = NULL;
	}

	if (premutex) {
		SDL_DestroyMutex(premutex);
		premutex = NULL;
	}

	// Shutdown the new texture job queue
	tex_jobs_shutdown();

	if (game_options & GO_SOUND) {
		Mix_Quit();
	}
#ifdef DEVELOPER
	sdl_dump_spritecache();
#endif
}

void cmd_proc(int key);
void context_keyup(int key);

void sdl_loop(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			quit = 1;
			break;
		case SDL_KEYDOWN:
			gui_sdl_keyproc(event.key.keysym.sym);
			break;
		case SDL_KEYUP:
			context_keyup(event.key.keysym.sym);
			break;
		case SDL_TEXTINPUT:
			cmd_proc(event.text.text[0]);
			break;
		case SDL_MOUSEMOTION:
			gui_sdl_mouseproc(event.motion.x, event.motion.y, SDL_MOUM_NONE);
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (event.button.button == SDL_BUTTON_LEFT) {
				gui_sdl_mouseproc(event.motion.x, event.motion.y, SDL_MOUM_LDOWN);
			}
			if (event.button.button == SDL_BUTTON_MIDDLE) {
				gui_sdl_mouseproc(event.motion.x, event.motion.y, SDL_MOUM_MDOWN);
			}
			if (event.button.button == SDL_BUTTON_RIGHT) {
				gui_sdl_mouseproc(event.motion.x, event.motion.y, SDL_MOUM_RDOWN);
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if (event.button.button == SDL_BUTTON_LEFT) {
				gui_sdl_mouseproc(event.motion.x, event.motion.y, SDL_MOUM_LUP);
			}
			if (event.button.button == SDL_BUTTON_MIDDLE) {
				gui_sdl_mouseproc(event.motion.x, event.motion.y, SDL_MOUM_MUP);
			}
			if (event.button.button == SDL_BUTTON_RIGHT) {
				gui_sdl_mouseproc(event.motion.x, event.motion.y, SDL_MOUM_RUP);
			}
			break;
		case SDL_MOUSEWHEEL:
			gui_sdl_mouseproc(event.wheel.x, event.wheel.y, SDL_MOUM_WHEEL);
			break;
		case SDL_WINDOWEVENT:
#ifdef ENABLE_DRAGHACK
			if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
				int x, y;
				Uint32 mouseState = SDL_GetMouseState(&x, &y);
				if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) {
					gui_sdl_draghack();
				}
			}
#endif
			break;
		default:
			break;
		}
	}
}

void sdl_set_cursor_pos(int x, int y)
{
	SDL_WarpMouseInWindow(sdlwnd, x, y);
}

void sdl_show_cursor(int flag)
{
	SDL_ShowCursor(flag ? SDL_ENABLE : SDL_DISABLE);
}

void sdl_capture_mouse(int flag)
{
	SDL_CaptureMouse(flag ? SDL_TRUE : SDL_FALSE);
}

/* This function is a hack. It can only load one specific type of
   Windows cursor file: 32x32 pixels with 1 bit depth. */

SDL_Cursor *sdl_create_cursor(char *filename)
{
	FILE *fp;
	unsigned char mask[128], data[128], buf[326];
	unsigned char mask2[128 * 16] = {0}, data2[128 * 16] = {0};

	fp = fopen(filename, "rb");
	if (!fp) {
		warn("SDL Error: Could not open cursor file %s.\n", filename);
		return NULL;
	}

	if (fread(buf, 1, 326, fp) != 326) {
		warn("SDL Error: Read cursor file failed.\n");
		fclose(fp);
		return NULL;
	}
	fclose(fp);

	// translate .cur
	for (int i = 0; i < 32; i++) {
		for (int j = 0; j < 4; j++) {
			data[i * 4 + j] = (unsigned char)((~buf[322 - i * 4 + j]) & (~buf[194 - i * 4 + j]));
			mask[i * 4 + j] = buf[194 - i * 4 + j];
		}
	}

	// scale up if needed and add frame to cross
	static char cross[11][11] = {{0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0},
	    {0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0}, {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},
	    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1}, {0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0},
	    {0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0}};
	int dst, src, i1, b1, i2, b2, y1, y2;

	for (y2 = 0; y2 < 32 * sdl_scale; y2++) {
		y1 = y2 / sdl_scale;

		for (dst = 0; dst < 32 * sdl_scale; dst++) {
			src = dst / sdl_scale;

			i1 = src / 8 + y1 * 4;
			b1 = 128 >> (src & 7);

			i2 = dst / 8 + y2 * 4 * sdl_scale;
			b2 = 128 >> (dst & 7);

			if (src < 11 && y1 < 11 && cross[y1][src]) {
				data2[i2] |= b2;
				mask2[i2] |= b2;
			} else {
				if (data[i1] & b1) {
					data2[i2] |= b2;
				} else {
					data2[i2] &= ~b2;
				}
				if (mask[i1] & b1) {
					mask2[i2] |= b2;
				} else {
					mask2[i2] &= ~b2;
				}
			}
		}
	}
	return SDL_CreateCursor(data2, mask2, 32 * sdl_scale, 32 * sdl_scale, 6 * sdl_scale, 6 * sdl_scale);
}

int sdl_create_cursors(void)
{
	int error = 0;

	curs[SDL_CUR_c_only] = sdl_create_cursor("res/cursor/c_only.cur");
	if (!curs[SDL_CUR_c_only]) {
		error = 1;
	}

	curs[SDL_CUR_c_take] = sdl_create_cursor("res/cursor/c_take.cur");
	if (!curs[SDL_CUR_c_take]) {
		error = 1;
	}

	curs[SDL_CUR_c_drop] = sdl_create_cursor("res/cursor/c_drop.cur");
	if (!curs[SDL_CUR_c_drop]) {
		error = 1;
	}

	curs[SDL_CUR_c_attack] = sdl_create_cursor("res/cursor/c_atta.cur");
	if (!curs[SDL_CUR_c_attack]) {
		error = 1;
	}

	curs[SDL_CUR_c_raise] = sdl_create_cursor("res/cursor/c_rais.cur");
	if (!curs[SDL_CUR_c_raise]) {
		error = 1;
	}

	curs[SDL_CUR_c_give] = sdl_create_cursor("res/cursor/c_give.cur");
	if (!curs[SDL_CUR_c_give]) {
		error = 1;
	}

	curs[SDL_CUR_c_use] = sdl_create_cursor("res/cursor/c_use.cur");
	if (!curs[SDL_CUR_c_use]) {
		error = 1;
	}

	curs[SDL_CUR_c_usewith] = sdl_create_cursor("res/cursor/c_usew.cur");
	if (!curs[SDL_CUR_c_usewith]) {
		error = 1;
	}

	curs[SDL_CUR_c_swap] = sdl_create_cursor("res/cursor/c_swap.cur");
	if (!curs[SDL_CUR_c_swap]) {
		error = 1;
	}

	curs[SDL_CUR_c_sell] = sdl_create_cursor("res/cursor/c_sell.cur");
	if (!curs[SDL_CUR_c_sell]) {
		error = 1;
	}

	curs[SDL_CUR_c_buy] = sdl_create_cursor("res/cursor/c_buy.cur");
	if (!curs[SDL_CUR_c_buy]) {
		error = 1;
	}

	curs[SDL_CUR_c_look] = sdl_create_cursor("res/cursor/c_look.cur");
	if (!curs[SDL_CUR_c_look]) {
		error = 1;
	}

	curs[SDL_CUR_c_set] = sdl_create_cursor("res/cursor/c_set.cur");
	if (!curs[SDL_CUR_c_set]) {
		error = 1;
	}

	curs[SDL_CUR_c_spell] = sdl_create_cursor("res/cursor/c_spell.cur");
	if (!curs[SDL_CUR_c_spell]) {
		error = 1;
	}

	curs[SDL_CUR_c_pix] = sdl_create_cursor("res/cursor/c_pix.cur");
	if (!curs[SDL_CUR_c_pix]) {
		error = 1;
	}

	curs[SDL_CUR_c_say] = sdl_create_cursor("res/cursor/c_say.cur");
	if (!curs[SDL_CUR_c_say]) {
		error = 1;
	}

	curs[SDL_CUR_c_junk] = sdl_create_cursor("res/cursor/c_junk.cur");
	if (!curs[SDL_CUR_c_junk]) {
		error = 1;
	}

	curs[SDL_CUR_c_get] = sdl_create_cursor("res/cursor/c_get.cur");
	if (!curs[SDL_CUR_c_get]) {
		error = 1;
	}

	if (error) {
		fail("Failed to load one or more cursor files");
		return 0;
	}

	return 1;
}

void sdl_set_cursor(int cursor)
{
	if (cursor < SDL_CUR_c_only || cursor > SDL_CUR_c_get) {
		return;
	}
	if (!curs[cursor]) {
		return;
	}
	SDL_SetCursor(curs[cursor]);
}

// Worker function to process jobs from the queue
// Used in single-threaded mode (called from main thread)
// Returns 1 if work was found and processed, 0 if queue was empty or no work
// was done because it was called from a multi-threaded context.
int if_single_thread_process_one_job(void)
{
	// This function is ONLY for single-threaded mode
	// In multi-threaded mode, worker threads run their own loop
	if (sdl_multi) {
		return 0;
	}

	// Pop a job from the queue (non-blocking)
	texture_job_t job;
	if (!tex_jobs_pop(&job, 0)) {
		return 0; // Queue empty
	}

	int cache_index = job.cache_index;
	struct sdl_texture *tex = &sdlt[cache_index];

	// Single-threaded: no races possible, generation check is sufficient
	// If generation changed, the slot was evicted and reused - abandon this work
	if (tex->generation != job.generation) {
		return 0;
	}

	// Mark in-worker (no lock needed in single-threaded mode)
	tex->work_state = TX_WORK_IN_WORKER;

	// Do the actual work: load image and do stages 1+2
	unsigned int sprite = tex->sprite;

	if (sdl_ic_load(sprite, NULL) < 0) {
		// Failed: mark idle and leave DIDMAKE unset
		// Generation can't change under us in single-threaded mode.
		return 0;
	}

	// Stage 1 + 2
	sdl_make(tex, &sdli[sprite], 1);
	sdl_make(tex, &sdli[sprite], 2);

	return 1;
}

void sdl_pre_add(unsigned int sprite, signed char sink, unsigned char freeze, unsigned char scale, char cr, char cg,
    char cb, char light, char sat, int c1, int c2, int c3, int shine, char ml, char ll, char rl, char ul, char dl)
{
	Uint64 start;

	if (sprite >= MAXSPRITE) {
		note("illegal sprite %u wanted in pre_add", sprite);
		return;
	}

	// Ensure there is a cache slot (but don't force full make+tex)
	start = SDL_GetTicks64();
	int cache_index = sdl_tx_load(sprite, sink, freeze, scale, cr, cg, cb, light, sat, c1, c2, c3, shine, ml, ll, rl,
	    ul, dl, NULL, 0, 0, NULL, 0, 1);
	extern long long sdl_time_alloc;
	sdl_time_alloc += (long long)(SDL_GetTicks64() - start);

	if (cache_index == -1) {
		// Already in cache
		return;
	}

	if (cache_index == STX_NONE) {
		// Failed to allocate
		return;
	}

	struct sdl_texture *slot = &sdlt[cache_index];

	// Single-threaded: do the CPU work inline
	if (!sdl_multi) {
		if (!(flags_load(slot) & SF_DIDMAKE)) {
			unsigned int sprite_id = slot->sprite;
			if (sdl_ic_load(sprite_id, NULL) >= 0) {
				sdl_make(slot, &sdli[sprite_id], 1);
				sdl_make(slot, &sdli[sprite_id], 2);
			}
		}
		return;
	}

	// Multi-threaded: enqueue background job
	SDL_LockMutex(g_tex_jobs.mutex);

	// Check if already queued or in-progress
	if (slot->work_state != TX_WORK_IDLE) {
		SDL_UnlockMutex(g_tex_jobs.mutex);
		return;
	}

	// Check if queue is full
	if (g_tex_jobs.count >= TEX_JOB_CAPACITY) {
#ifdef DEVELOPER
		static uint64_t last_log_time = 0;
		uint64_t now = SDL_GetTicks64();
		if (now - last_log_time > 1000) {
			warn("Texture job queue full: capacity=%d, dropping preload for sprite %u", TEX_JOB_CAPACITY, sprite);
			last_log_time = now;
		}
#endif
		SDL_UnlockMutex(g_tex_jobs.mutex);
		return;
	}

	// Add job to queue
	texture_job_t *job = &g_tex_jobs.jobs[g_tex_jobs.tail];
	job->cache_index = cache_index;
	job->generation = slot->generation;
	job->kind = TEXTURE_JOB_MAKE_STAGES_1_2;

	g_tex_jobs.tail = (g_tex_jobs.tail + 1) % TEX_JOB_CAPACITY;
	g_tex_jobs.count++;

	// Mark as queued
	slot->work_state = TX_WORK_QUEUED;

	SDL_CondSignal(g_tex_jobs.cond);
	SDL_UnlockMutex(g_tex_jobs.mutex);

	// Wake a worker
	SDL_SemPost(prework);
}

long long sdl_time_mutex = 0;

void sdl_lock(void *a)
{
	Uint64 start = SDL_GetTicks64();
	SDL_LockMutex(a);
	sdl_time_mutex += (long long)(SDL_GetTicks64() - start);
}

#define SDL_LockMutex(a) sdl_lock(a)

int sdl_pre_do(void)
{
	Uint64 start;
	int uploads = 0;

	start = SDL_GetTicks64();

	// Single-threaded: process jobs from queue (will no-op if called from multi)
	if_single_thread_process_one_job();

	// Main thread: upload any textures where CPU work is done (SF_DIDMAKE) but
	// GPU upload hasn't happened (!SF_DIDTEX)
	// This is stage 3: creating the actual SDL_Texture
	const int max_uploads_per_call = 64; // Safety bound to avoid stalling frame

	for (int i = 0; i < MAX_TEXCACHE && uploads < max_uploads_per_call; i++) {
		struct sdl_texture *slot = &sdlt[i];

		uint16_t flags = flags_load(slot);
		if (!(flags & SF_SPRITE)) {
			continue;
		}

		// Worker did stage 1+2, GPU upload hasn't happened yet
		if ((flags & SF_DIDMAKE) && !(flags & SF_DIDTEX)) {
			unsigned int sprite = slot->sprite;
			sdl_make(slot, &sdli[sprite], 3);
			uploads++;
		}
	}

	extern long long sdl_time_pre2;
	sdl_time_pre2 += (long long)(SDL_GetTicks64() - start);

	return uploads;
}

uint64_t sdl_backgnd_wait = 0, sdl_backgnd_work = 0, sdl_backgnd_jobs = 0;

int sdl_pre_backgnd(void *ptr)
{
	int worker_id = (int)(long long)ptr;
	struct zip_handles *zips = worker_zips ? &worker_zips[worker_id] : NULL;
	uint64_t wait_start, work_start;

	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);

	for (;;) {
		// Wait for work to be available (blocks until signaled)
		wait_start = SDL_GetTicks64();
		int sem_result = SDL_SemWait(prework);
		sdl_backgnd_wait += SDL_GetTicks64() - wait_start;

		if (sem_result != 0) {
			SDL_Log("sdl_pre_backgnd: SDL_SemWait failed: %s - exiting worker thread", SDL_GetError());
			return -1;
		}

		// Check for shutdown before each job attempt
		if (quit || SDL_AtomicGet(&worker_quit)) {
			return 0;
		}

		work_start = SDL_GetTicks64();

		// Pop a job from the queue
		texture_job_t job;
		if (!tex_jobs_pop(&job, 0)) {
			// No work found
			continue;
		}

		int cache_index = job.cache_index;
		struct sdl_texture *tex = &sdlt[cache_index];

		// Check generation: if stale, skip
		if (tex->generation != job.generation) {
			continue;
		}

		// Mark in-worker
		SDL_LockMutex(g_tex_jobs.mutex);
		if (tex->generation != job.generation) {
			SDL_UnlockMutex(g_tex_jobs.mutex);
			continue;
		}
		tex->work_state = TX_WORK_IN_WORKER;
		SDL_UnlockMutex(g_tex_jobs.mutex);

		// Do the actual work: load image and do stages 1+2
		unsigned int sprite = tex->sprite;

		if (sdl_ic_load(sprite, zips) < 0) {
			// Failed: leave DIDMAKE unset, allow main thread to handle fallback
			SDL_LockMutex(g_tex_jobs.mutex);
			if (tex->generation == job.generation) {
				tex->work_state = TX_WORK_IDLE;
			}
			SDL_UnlockMutex(g_tex_jobs.mutex);
			continue;
		}

		// Stage 1 + 2
		sdl_make(tex, &sdli[sprite], 1);
		sdl_make(tex, &sdli[sprite], 2);

		SDL_LockMutex(g_tex_jobs.mutex);
		if (tex->generation == job.generation) {
			// CPU work done; GPU creation is main-thread
			tex->work_state = TX_WORK_IDLE;
		}
		SDL_UnlockMutex(g_tex_jobs.mutex);

		sdl_backgnd_work += SDL_GetTicks64() - work_start;
		sdl_backgnd_jobs++;
	}

	return 0;
}

int sdl_is_shown(void)
{
	uint32_t flags;

	flags = SDL_GetWindowFlags(sdlwnd);

	if (flags & SDL_WINDOW_HIDDEN) {
		return 0;
	}
	if (flags & SDL_WINDOW_MINIMIZED) {
		return 0;
	}

	return 1;
}

int sdl_has_focus(void)
{
	uint32_t flags;

	flags = SDL_GetWindowFlags(sdlwnd);

	if (flags & SDL_WINDOW_MOUSE_FOCUS) {
		return 1;
	}

	return 0;
}

void sdl_set_title(char *title)
{
	SDL_SetWindowTitle(sdlwnd, title);
}

void *sdl_create_texture(int width, int height)
{
	return SDL_CreateTexture(sdlren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
}

void sdl_render_copy(void *tex, void *sr, void *dr)
{
	SDL_RenderCopy(sdlren, tex, sr, dr);
}

void sdl_render_copy_ex(void *tex, void *sr, void *dr, double angle)
{
	SDL_RenderCopyEx(sdlren, tex, sr, dr, angle, 0, SDL_FLIP_NONE);
}

void sdl_flush_textinput(void)
{
	SDL_FlushEvent(SDL_TEXTINPUT);
}

int sdl_check_mouse(void)
{
	int x, y, x2, y2, x3, y3, top;
	SDL_GetGlobalMouseState(&x, &y);

	SDL_GetWindowPosition(sdlwnd, &x2, &y2);
	SDL_GetWindowSize(sdlwnd, &x3, &y3);
	SDL_GetWindowBordersSize(sdlwnd, &top, NULL, NULL, NULL);

	if (x < x2 || x > x2 + x3 || y > y2 + y3) {
		return 1;
	}

	if (game_options & GO_TINYTOP) {
		if (y2 - y > top) {
			return 1;
		}
	} else {
		if (y2 - y > 100 * sdl_scale) {
			return 1;
		}
	}

	if (y < y2) {
		return -1;
	}

	return 0;
}
