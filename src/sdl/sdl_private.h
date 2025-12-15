/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 */

#ifndef SDL_PRIVATE_H
#define SDL_PRIVATE_H

#include <stdint.h>
#include <zip.h>
#include <png.h>
#include <SDL2/SDL.h>

// Fixed upper bound for the texture cache metadata.
// 32k entries is only a few MB of metadata and comfortably covers typical usage.
// Statically allocated at compile time
#define MAX_TEXCACHE 32768
#define MAX_TEXHASH  32768

#define STX_NONE (-1)

#define IGET_A(c)         ((((uint32_t)(c)) >> 24) & 0xFF)
#define IGET_R(c)         ((((uint32_t)(c)) >> 16) & 0xFF)
#define IGET_G(c)         ((((uint32_t)(c)) >> 8) & 0xFF)
#define IGET_B(c)         ((((uint32_t)(c)) >> 0) & 0xFF)
#define IRGB(r, g, b)     (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 0))
#define IRGBA(r, g, b, a) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 0))

#define SF_USED     (1 << 0)
#define SF_SPRITE   (1 << 1)
#define SF_TEXT     (1 << 2)
#define SF_DIDALLOC (1 << 3)
#define SF_DIDMAKE  (1 << 4)
#define SF_DIDTEX   (1 << 5)

// Texture job work state enum
typedef enum texture_work_state {
	TX_WORK_IDLE = 0, // no job queued, no worker running
	TX_WORK_QUEUED, // job is in queue, not yet taken by a worker
	TX_WORK_IN_WORKER, // worker popped job and is processing
} texture_work_state_t;

struct sdl_texture {
	SDL_Texture *tex;
	uint32_t *pixel;

	int prev, next;
	int hprev, hnext;

	_Atomic(uint16_t) flags; // Atomic for lock-free reads, writes under mutex


	// Versioning and work state for robust job queue
	uint32_t generation; // Incremented each time this slot is reused (eviction only)
	// See texture_work_state_t; MUST be modified under g_tex_jobs.mutex
	_Atomic(uint8_t) work_state;

	// ---------- sprites ------------
	// fx
	uint32_t sprite;
	int8_t sink;
	uint8_t scale;
	int16_t cr, cg, cb, light, sat;
	uint16_t c1, c2, c3, shine;

	uint8_t freeze;

	// light
	int8_t ml, ll, rl, ul, dl; // light in middle, left, right, up, down

	// primary
	uint16_t xres; // x resolution in pixels
	uint16_t yres; // y resolution in pixels
	int16_t xoff; // offset to blit position
	int16_t yoff; // offset to blit position

	// ---------- text --------------
	uint16_t text_flags;
	uint32_t text_color;
	char *text;
	void *text_font;
};

struct sdl_image {
	uint32_t *pixel;

	uint16_t flags;
	uint16_t xres, yres;
	int16_t xoff, yoff;
};

// Texture job queue structures
#define TEX_JOB_CAPACITY 16384 // Large enough to handle all texture cache entries

// Texture job kind - makes the job semantics explicit
typedef enum texture_job_kind {
	// Load from disk, allocate pixels, process effects
	TEXTURE_JOB_MAKE_STAGES_1_2 = 0,
	// Room for future job types (TEXTURE_JOB_FREE, TEXTURE_JOB_RELOAD, etc.)
} texture_job_kind_t;

typedef struct texture_job {
	int cache_index; // index into sdlt[]
	uint32_t generation; // snapshot of sdlt[cache_index].generation
	texture_job_kind_t kind; // what operation to perform
} texture_job_t;

typedef struct texture_job_queue {
	texture_job_t jobs[TEX_JOB_CAPACITY];
	int head; // pop position
	int tail; // push position
	int count; // number of jobs in queue

	SDL_mutex *mutex;
	SDL_cond *cond;
} texture_job_queue_t;

// Lock-free flag operation helpers
// These provide consistent atomic ordering across all SDL modules
// Must be defined after struct sdl_texture is complete
static inline uint16_t flags_load(struct sdl_texture *st)
{
	uint16_t *flags_ptr = (uint16_t *)&st->flags;
	return __atomic_load_n(flags_ptr, __ATOMIC_ACQUIRE);
}

// Work state load helper
// Can be called without mutex for diagnostic reads, but any decision based on
// the value that affects eviction or queue state MUST be done under mutex.
static inline uint8_t work_state_load(struct sdl_texture *st)
{
	uint8_t *state_ptr = (uint8_t *)&st->work_state;
	return __atomic_load_n(state_ptr, __ATOMIC_ACQUIRE);
}

// Work state store helper
// Caller MUST hold g_tex_jobs.mutex.
// Stores are always coordinated with queue state changes under the mutex.
static inline void work_state_store(struct sdl_texture *st, texture_work_state_t new_state)
{
	uint8_t *state_ptr = (uint8_t *)&st->work_state;
	__atomic_store_n(state_ptr, (uint8_t)new_state, __ATOMIC_RELEASE);
}

#ifndef HAVE_DDFONT
#define HAVE_DDFONT

struct renderfont {
	int dim;
	unsigned char *raw;
};
#endif

#define RENDER_TEXT_TERMINATOR '\xB0' // draw text terminator - (zero stays one, too)

struct zip_handles {
	zip_t *zip1;
	zip_t *zip2;
	zip_t *zip1p;
	zip_t *zip2p;
	zip_t *zip1m;
	zip_t *zip2m;
};

int sdl_ic_load(unsigned int sprite, struct zip_handles *zips);
int sdl_pre_backgnd(void *ptr);
int sdl_create_cursors(void);
SDL_Cursor *sdl_create_cursor(char *filename);
void sdl_pre_add(unsigned int sprite, signed char sink, unsigned char freeze, unsigned char scale, char cr, char cg,
    char cb, char light, char sat, int c1, int c2, int c3, int shine, char ml, char ll, char rl, char ul, char dl);
void sdl_lock(void *a);
int sdl_pre_do(void);

#define MAX_SOUND_CHANNELS 32
#define MAXSOUND           100

struct png_helper;
int png_load_helper(struct png_helper *p);
void png_load_helper_exit(struct png_helper *p);

// ============================================================================
// Shared variables from sdl_core.c
// ============================================================================
extern SDL_Window *sdlwnd;
extern SDL_Renderer *sdlren;
extern zip_t *sdl_zip1;
extern zip_t *sdl_zip2;
extern zip_t *sdl_zip1p;
extern zip_t *sdl_zip2p;
extern zip_t *sdl_zip1m;
extern zip_t *sdl_zip2m;
extern SDL_mutex *premutex;
extern int *sdli_state; // Image loading state machine
extern texture_job_queue_t g_tex_jobs; // Texture job queue
extern int sdl_cache_size; // Requested size (for logging / config), not allocation

// ============================================================================
// Shared variables from sdl_texture.c
// ============================================================================
extern struct sdl_texture sdlt[MAX_TEXCACHE];
extern int sdlt_best, sdlt_last;
extern int sdlt_cache[MAX_TEXHASH];
extern struct sdl_image *sdli;

extern int texc_used;
extern long long mem_png, mem_tex;
extern long long texc_hit, texc_miss, texc_pre;

extern long long sdl_time_preload;
extern long long sdl_time_make;
extern long long sdl_time_make_main;
extern long long sdl_time_load;
extern long long sdl_time_alloc;
extern long long sdl_time_tex;
extern long long sdl_time_tex_main;
extern long long sdl_time_text;
extern long long sdl_time_blit;
extern long long sdl_time_pre1;
extern long long sdl_time_pre2;
extern long long sdl_time_pre3;

extern int maxpanic;

// ============================================================================
// Internal functions from sdl_texture.c
// ============================================================================
void sdl_tx_best(int cache_index);
int sdl_tx_load(unsigned int sprite, signed char sink, unsigned char freeze, unsigned char scale, char cr, char cg,
    char cb, char light, char sat, int c1, int c2, int c3, int shine, char ml, char ll, char rl, char ul, char dl,
    const char *text, int text_color, int text_flags, void *text_font, int checkonly, int preload);
void tex_jobs_init(void);
void tex_jobs_shutdown(void);
int tex_jobs_pop(texture_job_t *out_job, int should_block);

#ifdef DEVELOPER
void sdl_dump_spritecache(void);
#endif

// ============================================================================
// Internal functions from sdl_image.c
// ============================================================================
uint32_t mix_argb(uint32_t c1, uint32_t c2, float w1, float w2);
void sdl_smoothify(uint32_t *pixel, int xres, int yres, int scale);
void sdl_premulti(uint32_t *pixel, int xres, int yres, int scale);
void png_helper_read(png_structp ps, png_bytep buf, png_size_t len);
int sdl_load_image_png_(struct sdl_image *si, char *filename, zip_t *zip);
int sdl_load_image_png(struct sdl_image *si, char *filename, zip_t *zip, int smoothify);
int do_smoothify(int sprite);
int sdl_load_image(struct sdl_image *si, int sprite, struct zip_handles *zips);
int sdl_ic_load(unsigned int sprite, struct zip_handles *zips);
void sdl_make(struct sdl_texture *st, struct sdl_image *si, int preload);

// ============================================================================
// Internal functions from sdl_effects.c
// ============================================================================
uint32_t sdl_light(int light, uint32_t irgb);
uint32_t sdl_freeze(int freeze, uint32_t irgb);
uint32_t sdl_shine_pix(uint32_t irgb, unsigned short shine);
uint32_t sdl_colorize_pix(uint32_t irgb, unsigned short c1v, unsigned short c2v, unsigned short c3v);
uint32_t sdl_colorize_pix2(uint32_t irgb, unsigned short c1v, unsigned short c2v, unsigned short c3v, int x, int y,
    int xres, int yres, uint32_t *pixel, int sprite);
uint32_t sdl_colorbalance(uint32_t irgb, char cr, char cg, char cb, char light, char sat);

// ============================================================================
// Internal functions from sdl_draw.c
// ============================================================================
SDL_Texture *sdl_maketext(const char *text, struct renderfont *font, uint32_t color, int flags);

// ============================================================================
// Internal functions from sdl_core.c
// ============================================================================
int sdl_pre_worker(void);

// ============================================================================
// Test-only functions (compiled only when UNIT_TEST is defined)
// ============================================================================

#ifdef UNIT_TEST

// Initialize SDL subsystems for testing without window/audio/real I/O
int sdl_init_for_tests(int requested_cache_size);

// Initialize with background worker threads enabled
int sdl_init_for_tests_with_workers(int requested_cache_size, int worker_count);

// Tear down test state (stop workers, free resources)
void sdl_shutdown_for_tests(void);

// Pump the prefetch pipeline without full game loop
int sdl_pre_tick_for_tests(void);

// Check all cache/queue/LRU invariants (returns 0 on success, -1 on bug)
int sdl_check_invariants_for_tests(void);

// Test-only introspection helpers (read-only, no side effects)
uint16_t sdl_texture_get_flags_for_test(int cache_index);
int sdl_texture_get_sprite_for_test(int cache_index);
uint8_t sdl_texture_get_work_state_for_test(int cache_index);
int sdl_get_job_queue_depth_for_test(void);

#endif /* UNIT_TEST */

#endif // SDL_PRIVATE_H
