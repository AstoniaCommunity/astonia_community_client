/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * Startup And Command Line
 *
 * Contains the startup stuff and the parsing of the command line. Plus a
 * bunch of generic helper for memory allocation and error display.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_keycode.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "astonia.h"
#include "game/game.h"
#include "game/game_private.h"
#include "sdl/sdl.h"
#include "gui/gui.h"
#include "client/client.h"
#include "modder/modder.h"

// Forward declarations
void xlog(FILE *logfp, char *format, ...) __attribute__((format(printf, 2, 3)));
void addlinesep(void);
int rread(FILE *fp, void *ptr, size_t size);
char *load_ascii_file(char *filename, uint8_t ID);
void rrandomize(void);
void display_usage(void);
int parse_args(int argc, char *argv[]);
void load_options(void);
void init_logging(void);
void determine_resolution(void);

int quit = 0;
int sv_ver = 30;

char *localdata;

static int panic_reached = 0;
int xmemcheck_failed = 0;
SDL_Keycode user_keys[10] = {'Q', 'W', 'E', 'A', 'S', 'D', 'Z', 'X', 'C', 'V'};

DLL_EXPORT uint64_t game_options = GO_NOTSET;

static char memcheck_failed_str[] = {"memcheck failed"};
static char panic_reached_str[] = {"panic failure"};

FILE *errorfp;

// note, warn, fail, paranoia, addline

DLL_EXPORT int note(const char *format, ...)
{
	va_list va;
	char buf[1024];


	va_start(va, format);
	vsprintf(buf, format, va);
	va_end(va);

	printf("NOTE: %s\n", buf);
	fflush(stdout);
#ifdef DEVELOPER
	addline("NOTE: %s\n", buf);
#endif

	return 0;
}

DLL_EXPORT int warn(const char *format, ...)
{
	va_list va;
	char buf[1024];


	va_start(va, format);
	vsprintf(buf, format, va);
	va_end(va);

	printf("WARN: %s\n", buf);
	fflush(stdout);
	addline("WARN: %s\n", buf);

	return 0;
}

DLL_EXPORT int fail(const char *format, ...)
{
	va_list va;
	char buf[1024];


	va_start(va, format);
	vsprintf(buf, format, va);
	va_end(va);

	fprintf(errorfp, "FAIL: %s\n", buf);
	fflush(errorfp);
	printf("FAIL: %s\n", buf);
	fflush(stdout);
	addline("FAIL: %s\n", buf);

	return -1;
}

DLL_EXPORT void paranoia(const char *format, ...)
{
	va_list va;

	fprintf(errorfp, "PARANOIA EXIT in ");

	va_start(va, format);
	vfprintf(errorfp, format, va);
	va_end(va);

	fprintf(errorfp, "\n");
	fflush(errorfp);

	exit(-1);
}

void xlog(FILE *logfp, char *format, ...)
{
	va_list args;
	char buf[1024];
	struct tm *tm;
	time_t time_now;
	time(&time_now);

	va_start(args, format);
	vsnprintf(buf, 1024, format, args);
	va_end(args);

	tm = localtime(&time_now);
	if (tm) {
		fprintf(logfp, "%02d.%02d.%02d %02d:%02d:%02d: %s\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year - 100,
		    tm->tm_hour, tm->tm_min, tm->tm_sec, buf);
	} else {
		fprintf(logfp, "%s\n", buf);
	}
	fflush(logfp);
}

static int _addlinesep = 0;

void addlinesep(void)
{
	_addlinesep = 1;
}

DLL_EXPORT void addline(const char *format, ...)
{
	va_list va;
	char buf[1024];

	if (_addlinesep) {
		_addlinesep = 0;
		addline("-------------");
	}

	va_start(va, format);
	vsnprintf(buf, sizeof(buf) - 1, format, va);
	buf[sizeof(buf) - 1] = 0;
	va_end(va);

	if (render_text_init_done()) {
		render_add_text(buf);
	}
}

// io

int rread(FILE *fp, void *ptr, size_t size)
{
	size_t n;

	while (size > 0) {
		n = fread(ptr, 1, size, fp);
		if (n == 0) {
			return 1;
		}
		size -= n;
		ptr = ((unsigned char *)(ptr)) + n;
	}
	return 0;
}

char *load_ascii_file(char *filename, uint8_t ID)
{
	FILE *fp;
	size_t size;
	char *ptr;

	if (!(fp = fopen(filename, "rb"))) {
		return NULL;
	}
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return NULL;
	}
	{
		long file_size = ftell(fp);
		if (file_size < 0) {
			fclose(fp);
			return NULL;
		}
		size = (size_t)file_size;
	}
	if (fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return NULL;
	}
	ptr = xmalloc(size + 1, ID);
	if (rread(fp, ptr, size)) {
		xfree(ptr);
		fclose(fp);
		return NULL;
	}
	ptr[size] = 0;
	fclose(fp);

	return ptr;
}

// rrandom

void rrandomize(void)
{
	srand((unsigned int)time(NULL));
}

int rrand(int range)
{
	return rand() % range;
}

// parsing command line

void display_messagebox(char *title, char *text)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, NULL);
}

void display_usage(void)
{
	const char *help =
	    "The Astonia Client can only be started from the command line or with a specially created shortcut.\n\n"
	    "Usage: moac -u playername -p password -d url\n ... [-w width] [-h height]\n"
	    " ... [-m threads] [-o options]\n ... [-k framespersecond]\n\n"
	    "url being, for example, \"server.astonia.com\" or \"192.168.77.132\" (without the quotes).\n\n"
	    "width and height are the desired window size. If this matches the desktop size the client "
	    "will start in windowed borderless pseudo-fullscreen mode.\n\n"
	    "threads is the number of background threads the game should use. Use 0 to disable. Default is 4.\n\n"
	    "options is a bitfield.\nBit 0 (value of 1) enables the Dark GUI by Tegra.\n"
	    "Bit 1 enables the context menu.\nBit 2 the new keybindings.\nBit 3 the smaller bottom GUI.\n"
	    "Bit 4 the sliding away of the top GUI.\nBit 5 enables the bigger health/mana bars.\n"
	    "Bit 6 enables sound.\nBit 7 the large font.\nBit 8 true full screen mode.\nBit 9 enables the "
	    "legacy mouse wheel logic.\n"
	    "Bit 10 enables out-of-order execution (read: faster) of inventory access and command feedback.\n"
	    "Bit 11 reduces the animation buffer for faster reactions and more stutter.\n"
	    "Bit 12 writes application files to %appdata% instead of the current folder.\n"
	    "Bit 13 enables the loading and saving of minimaps.\n"
	    "Bit 14 and 15 increase gamma.\n"
	    "Bit 16 makes the sliding top bar less sensitive.\n"
	    "Bit 17 reduces lighting effects (more performance, less pretty).\n"
	    "Bit 18 disables the minimap.\n"
	    "Default depends on screen height.\n\n"
	    "framespersecond will set the display rate in frames per second.\n\n";

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Usage", help, NULL);
	printf("%s", help);
}

DLL_EXPORT char server_url[256];
DLL_EXPORT int server_port = 0;
DLL_EXPORT int want_width = 0;
DLL_EXPORT int want_height = 0;
DLL_EXPORT int want_monitor = 0; // Monitor number for multi-monitor support (0=default)

int parse_args(int argc, char *argv[])
{
	int i;
	char *end;

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (arg[0] != '-') {
			continue;
		}

		char opt = (char)tolower(arg[1]);
		char *val = NULL;

		if (arg[2] != '\0') {
			val = &arg[2];
		} else if (i + 1 < argc) {
			val = argv[i + 1];
			// We only consume the next arg if we use it.
			// However, in the loop, we need to be careful.
			// If we define that flags taking args MUST have them, we increment i.
		}

		switch (opt) {
		case 'u':
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				snprintf(username, sizeof(username), "%s", val);
			}
			break;
		case 'p':
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				snprintf(password, sizeof(password), "%s", val);
			}
			break;
		case 'd':
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				snprintf(server_url, sizeof(server_url), "%s", val);
			}
			break;
		case 'h':
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				long h = strtol(val, &end, 10);
				if (h < INT_MIN || h > INT_MAX) {
					want_height = 0;
				} else {
					want_height = (int)h;
				}
			}
			break;
		case 'w':
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				long w = strtol(val, &end, 10);
				if (w < INT_MIN || w > INT_MAX) {
					want_width = 0;
				} else {
					want_width = (int)w;
				}
			}
			break;
		case 'm':
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				long m = strtol(val, &end, 10);
				if (m < INT_MIN || m > INT_MAX) {
					sdl_multi = 0;
				} else {
					sdl_multi = (int)m;
				}
			}
			break;
		case 'o':
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				game_options = strtoull(val, &end, 10);
			}
			break;
		case 'c':
			// Legacy flag: cache size is now statically allocated (MAX_TEXCACHE = 8000).
			// Accept but ignore this flag for backward compatibility.
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			// Silently ignored - cache is compile-time fixed
			break;
		case 'k':
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				long f = strtol(val, &end, 10);
				if (f < INT_MIN || f > INT_MAX) {
					frames_per_second = 0;
				} else {
					frames_per_second = (int)f;
				}
			}
			break;
		case 't':
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				long p = strtol(val, &end, 10);
				if (p < INT_MIN || p > INT_MAX) {
					server_port = 0;
				} else {
					server_port = (int)p;
				}
			}
			break;
		case 'v':
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				long v = strtol(val, &end, 10);
				if (v < INT_MIN || v > INT_MAX) {
					sv_ver = 30;
				} else {
					sv_ver = (int)v;
				}
			}
			break;
		case 'n': // -n monitor number for multi-monitor support
			if (!val && i + 1 < argc) {
				val = argv[++i];
			}
			if (val) {
				long n = strtol(val, &end, 10);
				if (n < INT_MIN || n > INT_MAX) {
					want_monitor = 0;
				} else {
					want_monitor = (int)n;
				}
			}
			break;
		default:
			// Unknown option, ignore or warn?
			break;
		}
	}
	return 0;
}

void save_options(void)
{
	FILE *fp;
	char filename[MAX_PATH];

	if (localdata) {
		if (sv_ver == 35) {
			sprintf(filename, "%s%s", localdata, "moac35.dat");
		} else {
			sprintf(filename, "%s%s", localdata, "moac.dat");
		}
	} else {
		if (sv_ver == 35) {
			sprintf(filename, "%s", "bin/data/moac35.dat");
		} else {
			sprintf(filename, "%s", "bin/data/moac.dat");
		}
	}

	fp = fopen(filename, "wb");
	if (!fp) {
		return;
	}

	fwrite(&user_keys, sizeof(user_keys), 1, fp);
	if (sv_ver == 35) {
		fwrite(&v35_action_row, sizeof(v35_action_row), 1, fp);
	} else {
		fwrite(&v3_action_row, sizeof(v3_action_row), 1, fp);
	}
	fwrite(&action_enabled, sizeof(action_enabled), 1, fp);
	fwrite(&gear_lock, sizeof(gear_lock), 1, fp);
	fclose(fp);
}

void load_options(void)
{
	FILE *fp;
	char filename[MAX_PATH];

	if (localdata) {
		if (sv_ver == 35) {
			sprintf(filename, "%s%s", localdata, "moac35.dat");
		} else {
			sprintf(filename, "%s%s", localdata, "moac.dat");
		}
	} else {
		if (sv_ver == 35) {
			sprintf(filename, "%s", "bin/data/moac35.dat");
		} else {
			sprintf(filename, "%s", "bin/data/moac.dat");
		}
	}

	fp = fopen(filename, "rb");
	if (!fp) {
		return;
	}

	fread(&user_keys, sizeof(user_keys), 1, fp);
	if (sv_ver == 35) {
		fread(&v35_action_row, sizeof(v35_action_row), 1, fp);
	} else {
		fread(&v3_action_row, sizeof(v3_action_row), 1, fp);
	}
	fread(&action_enabled, sizeof(action_enabled), 1, fp);
	fread(&gear_lock, sizeof(gear_lock), 1, fp);
	fclose(fp);

	actions_loaded();
}

void init_logging(void)
{
	char filename[MAX_PATH];

	if (game_options & GO_APPDATA) {
		localdata = SDL_GetPrefPath(ORG_NAME, APP_NAME);
		if (localdata) {
			snprintf(filename, sizeof(filename), "%s%s", localdata, "moac.log");
		} else {
			// Fallback if SDL_GetPrefPath fails
			snprintf(filename, sizeof(filename), "moac.log");
		}
	} else {
		snprintf(filename, sizeof(filename), "moac.log");
	}

	errorfp = fopen(filename, "a");
	if (!errorfp) {
		errorfp = stderr;
	}
}

void determine_resolution(void)
{
	if (!want_height) {
		if (want_width == 800) {
			want_height = 600;
		} else if (want_width == 1600) {
			want_height = 1200;
		} else if (want_width == 2400) {
			want_height = 1800;
		} else if (want_width == 3200) {
			want_height = 2400;
		} else if (want_width) {
			want_height = want_width * 9 / 16;
		}
	}
	if (!want_width) {
		if (want_height == 600) {
			want_width = 800;
		} else if (want_height == 1000) {
			want_width = 1600;
		} else if (want_height == 1200) {
			want_width = 1600;
		} else if (want_height == 1800) {
			want_width = 2400;
		} else if (want_height == 2000) {
			want_width = 3200;
		} else if (want_height == 2400) {
			want_width = 3200;
		} else if (want_height) {
			want_width = want_height * 16 / 9;
		}
	}
}

static void set_v35_values(void)
{
	target_port = 27584;
	set_v35_inventory();
	set_v35_keytab();
	set_v35_actions();
	set_v35_skilltab();
}

// main
int main(int argc, char *argv[])
{
#if USE_MIMALLOC
	// Configure SDL to use mimalloc for all its internal allocations
	// This MUST be called before any SDL function, including SDL_GetPrefPath()
	if (!SDL_SetMemoryFunctions(MALLOC, CALLOC, REALLOC, FREE)) {
		// If this fails we should still carry on and just use malloc, but log error.
		SDL_Log("Failed to set memory functions for mimalloc: %s", SDL_GetError());
	}
#endif

	int ret;
	char buf[80];

	if ((ret = parse_args(argc, argv)) != 0) {
		return -1;
	}

	if (sv_ver == 35) {
		set_v35_values();
	}

	init_logging();

#ifdef ENABLE_CRASH_HANDLER
	register_crash_handler();
#endif

	amod_init();
#ifdef ENABLE_SHAREDMEM
	sharedmem_init();
#endif

	load_options();

	// set some stuff
	if (!*username || !*password || !*server_url) {
		display_usage();
		return 0;
	}

	xlog(errorfp, "Client started with -h%d -w%d -o%" PRIu64, want_height, want_width, game_options);

#ifdef _WIN32
	SetProcessDPIAware();
#endif

	target_server = server_url;

	if (server_port) {
		if (server_port < 0 || server_port > UINT16_MAX) {
			target_port = 0;
		} else {
			target_port = (uint16_t)server_port;
		}
	}

	// init random
	rrandomize();

	determine_resolution();

	sprintf(buf, "Astonia 3 v%d.%d.%d", (VERSION >> 16) & 255, (VERSION >> 8) & 255, (VERSION) & 255);
	if (!sdl_init(want_width, want_height, buf, want_monitor)) {
		render_exit();
		return -1;
	}

	render_init();
	init_sound();

	if (game_options & GO_LARGE) {
		namesize = 0;
		render_set_textfont(1);
	}

	main_init();
	update_user_keys();

	main_loop();

#ifdef ENABLE_SHAREDMEM
	sharedmem_exit();
#endif
	amod_exit();
	main_exit();
	sound_exit();
	render_exit();
	sdl_exit();

	list_mem();

	if (panic_reached) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "recursion panic", panic_reached_str, NULL);
	}
	if (xmemcheck_failed) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "memory panic", memcheck_failed_str, NULL);
	}

	if (localdata) {
		SDL_free(localdata);
	}

	xlog(errorfp, "Clean client shutdown. Thank you for playing!");
	if (errorfp != stderr) {
		fclose(errorfp);
	}
	return 0;
}
