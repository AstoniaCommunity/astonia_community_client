/*
 * SDL Test Helpers - Test-only initialization and invariant checking
 *
 * Compiled only when UNIT_TEST is defined.
 * Provides lightweight initialization for testing without windows/audio/real I/O.
 */

#ifdef UNIT_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "../astonia.h" // Must come first for tick_t
#include "sdl_private.h"

// Forward declarations for test-exposed functions
extern SDL_atomic_t worker_quit;
extern SDL_Thread **worker_threads;
extern struct zip_handles *worker_zips;
extern int sdl_multi;
extern SDL_sem *prework;

// ============================================================================
// State initialization helpers
// ============================================================================

static void sdl_zero_state_for_tests(void)
{
	int i;

	// Texture cache - reset to clean state
	for (i = 0; i < MAX_TEXCACHE; i++) {
		// Use atomic store for flags
		uint16_t *flags_ptr = (uint16_t *)&sdlt[i].flags;
		__atomic_store_n(flags_ptr, 0, __ATOMIC_RELAXED);

		sdlt[i].tex = NULL;
		sdlt[i].pixel = NULL;
		sdlt[i].hnext = STX_NONE;
		sdlt[i].hprev = STX_NONE;
		sdlt[i].prev = i - 1;
		sdlt[i].next = i + 1;
		sdlt[i].sprite = -1;
		sdlt[i].xres = 0;
		sdlt[i].yres = 0;
		sdlt[i].text = NULL;
		sdlt[i].generation = 1; // Start at 1 (0 is reserved)
		sdlt[i].work_state = TX_WORK_IDLE;
	}

	if (MAX_TEXCACHE > 0) {
		sdlt[0].prev = STX_NONE;
		sdlt[MAX_TEXCACHE - 1].next = STX_NONE;
	}

	sdlt_best = 0;
	sdlt_last = MAX_TEXCACHE - 1;

	// Hash table - clear all heads
	for (i = 0; i < MAX_TEXHASH; i++) {
		sdlt_cache[i] = STX_NONE;
	}

	// Job queue
	tex_jobs_shutdown();
	tex_jobs_init();

	// Reset performance counters
	mem_tex = 0;
	mem_png = 0;
}

// ============================================================================
// Public test initialization functions
// ============================================================================

int sdl_init_for_tests(void)
{
	// Minimal SDL init for timers/threads only
	if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
		fprintf(stderr, "sdl_init_for_tests: SDL_Init failed: %s\n", SDL_GetError());
		return 0;
	}

	// Single-threaded mode
	sdl_multi = 0;

	// Open graphics ZIP files (needed for real I/O)
	sdl_zip1 = zip_open("res/gx1.zip", ZIP_RDONLY, NULL);
	sdl_zip1p = zip_open("res/gx1_patch.zip", ZIP_RDONLY, NULL);
	sdl_zip1m = zip_open("res/gx1_mod.zip", ZIP_RDONLY, NULL);
	sdl_zip2 = zip_open("res/gx2.zip", ZIP_RDONLY, NULL);
	sdl_zip2p = zip_open("res/gx2_patch.zip", ZIP_RDONLY, NULL);
	sdl_zip2m = zip_open("res/gx2_mod.zip", ZIP_RDONLY, NULL);

	if (!sdl_zip1) {
		fprintf(stderr, "sdl_init_for_tests: Failed to open res/gx1.zip\n");
		fprintf(stderr, "Make sure to run tests from repository root!\n");
		return 0;
	}

	// Initialize job queue
	tex_jobs_init();

	// Create mutex for prefetch operations
	premutex = SDL_CreateMutex();
	if (!premutex) {
		fprintf(stderr, "sdl_init_for_tests: SDL_CreateMutex failed: %s\n", SDL_GetError());
		return 0;
	}

	// Create semaphore for worker signaling
	prework = SDL_CreateSemaphore(0);
	if (!prework) {
		fprintf(stderr, "sdl_init_for_tests: SDL_CreateSemaphore failed: %s\n", SDL_GetError());
		SDL_DestroyMutex(premutex);
		return 0;
	}

	SDL_AtomicSet(&worker_quit, 0);
	worker_threads = NULL;

	sdl_zero_state_for_tests();

	return 1;
}

int sdl_init_for_tests_with_workers(int worker_count)
{
	int i;

	if (!sdl_init_for_tests()) {
		return 0;
	}

	if (worker_count <= 0) {
		worker_count = 1;
	}
#ifndef SDL_MAX_WORKERS
#define SDL_MAX_WORKERS 16
#endif
	if (worker_count > SDL_MAX_WORKERS) {
		worker_count = SDL_MAX_WORKERS;
	}

	sdl_multi = worker_count;

	worker_threads = xmalloc(sizeof(SDL_Thread *) * (size_t)worker_count, MEM_SDL_BASE);
	if (!worker_threads) {
		fprintf(stderr, "sdl_init_for_tests_with_workers: out of memory\n");
		return 0;
	}

	// Don't allocate zip handles for tests - workers will use NULL
	worker_zips = NULL;

	SDL_AtomicSet(&worker_quit, 0);

	for (i = 0; i < worker_count; i++) {
		char name[64];
		snprintf(name, sizeof(name), "test_worker_%d", i);
		worker_threads[i] = SDL_CreateThread(sdl_pre_backgnd, name, (void *)(intptr_t)i);
		if (!worker_threads[i]) {
			fprintf(stderr, "sdl_init_for_tests_with_workers: SDL_CreateThread failed: %s\n", SDL_GetError());
			return 0;
		}
	}

	// Give workers a moment to start
	SDL_Delay(10);

	return 1;
}

void sdl_shutdown_for_tests(void)
{
	int i;

	// Stop worker threads
	if (sdl_multi && worker_threads) {
		SDL_AtomicSet(&worker_quit, 1);

		// Also signal the job queue condition variable (for any waiting on cond)
		SDL_LockMutex(g_tex_jobs.mutex);
		SDL_CondBroadcast(g_tex_jobs.cond);
		SDL_UnlockMutex(g_tex_jobs.mutex);

		// Wake up all workers from semaphore wait
		// Each worker needs one semaphore post to wake from SDL_SemWait
		for (i = 0; i < sdl_multi; i++) {
			SDL_SemPost(prework);
		}

		// Wait for all workers to exit
		for (i = 0; i < sdl_multi; i++) {
			if (worker_threads[i]) {
				SDL_WaitThread(worker_threads[i], NULL);
			}
		}
		xfree(worker_threads);
		worker_threads = NULL;
	}

	// Close ZIP files
	if (sdl_zip1) {
		zip_close(sdl_zip1);
	}
	if (sdl_zip1p) {
		zip_close(sdl_zip1p);
	}
	if (sdl_zip1m) {
		zip_close(sdl_zip1m);
	}
	if (sdl_zip2) {
		zip_close(sdl_zip2);
	}
	if (sdl_zip2p) {
		zip_close(sdl_zip2p);
	}
	if (sdl_zip2m) {
		zip_close(sdl_zip2m);
	}

	// Shutdown job queue
	tex_jobs_shutdown();

	if (premutex) {
		SDL_DestroyMutex(premutex);
		premutex = NULL;
	}
	if (prework) {
		SDL_DestroySemaphore(prework);
		prework = NULL;
	}

	SDL_Quit();
}

int sdl_pre_tick_for_tests(void)
{
	return sdl_pre_do();
}

// ============================================================================
// Invariant checking
// ============================================================================

static int sdl_check_texture_entry_invariants(int cache_index)
{
	struct sdl_texture *e = &sdlt[cache_index];
	uint16_t flags = flags_load(e);

	// If entry is unused, it should not have resources
	if (!(flags & SF_USED)) {
		if (e->tex != NULL) {
			fprintf(stderr, "BUG: unused entry %d has tex != NULL\n", cache_index);
			return -1;
		}
		// Note: pixel might still be set temporarily during cleanup, so we don't check it
		return 0;
	}

	// Flag progression invariants: DIDALLOC -> DIDMAKE -> DIDTEX
	if ((flags & SF_DIDTEX) && !(flags & SF_DIDMAKE)) {
		fprintf(stderr, "BUG: entry %d has DIDTEX without DIDMAKE\n", cache_index);
		return -1;
	}

	if ((flags & SF_DIDTEX) && !(flags & SF_DIDALLOC)) {
		fprintf(stderr, "BUG: entry %d has DIDTEX without DIDALLOC\n", cache_index);
		return -1;
	}

	if ((flags & SF_DIDMAKE) && !(flags & SF_DIDALLOC)) {
		fprintf(stderr, "BUG: entry %d has DIDMAKE without DIDALLOC\n", cache_index);
		return -1;
	}

	// Resource consistency: DIDTEX should mean tex != NULL
	if ((flags & SF_DIDTEX) && e->tex == NULL) {
		fprintf(stderr, "BUG: entry %d has DIDTEX but tex == NULL\n", cache_index);
		return -1;
	}

	// Text vs Sprite mutual exclusion
	if ((flags & SF_TEXT) && (flags & SF_SPRITE)) {
		fprintf(stderr, "BUG: entry %d has both SF_TEXT and SF_SPRITE\n", cache_index);
		return -1;
	}

	// Text entries: must have tex, must not have pixel
	if (flags & SF_TEXT) {
		if (e->tex == NULL) {
			fprintf(stderr, "BUG: entry %d is SF_TEXT but tex == NULL\n", cache_index);
			return -1;
		}
		if (e->pixel != NULL) {
			fprintf(stderr, "BUG: entry %d is SF_TEXT but pixel != NULL\n", cache_index);
			return -1;
		}
	}

	// Sprite entries: must not have text
	if ((flags & SF_SPRITE) && e->text != NULL) {
		fprintf(stderr, "BUG: entry %d is SF_SPRITE but text != NULL\n", cache_index);
		return -1;
	}

	// Generation should never be 0 (reserved value)
	if (e->generation == 0) {
		fprintf(stderr, "BUG: entry %d has generation == 0\n", cache_index);
		return -1;
	}

	// Work state must be valid
	uint8_t ws = work_state_load(e);
	if (ws != TX_WORK_IDLE && ws != TX_WORK_QUEUED && ws != TX_WORK_IN_WORKER) {
		fprintf(stderr, "BUG: entry %d has invalid work_state=%u\n", cache_index, (unsigned)ws);
		return -1;
	}

	return 0;
}

static int sdl_check_hash_chain_invariants(void)
{
	int h;

	for (h = 0; h < MAX_TEXHASH; h++) {
		int steps = 0;
		int idx = sdlt_cache[h];

		while (idx != STX_NONE) {
			// Check index is in range
			if (idx < 0 || idx >= MAX_TEXCACHE) {
				fprintf(stderr, "BUG: hash[%d] has out-of-range index %d\n", h, idx);
				return -1;
			}

			// Detect cycles (no chain should be longer than cache size)
			if (steps++ > MAX_TEXCACHE) {
				fprintf(stderr, "BUG: hash[%d] appears to have a cycle (steps=%d)\n", h, steps);
				return -1;
			}

			idx = sdlt[idx].hnext;
		}
	}

	return 0;
}

static int sdl_check_lru_list_invariants(void)
{
	int idx;
	int count = 0;

	// Forward walk from sdlt_best
	idx = sdlt_best;
	while (idx != STX_NONE) {
		if (idx < 0 || idx >= MAX_TEXCACHE) {
			fprintf(stderr, "BUG: LRU forward walk found out-of-range index %d\n", idx);
			return -1;
		}

		if (count++ > MAX_TEXCACHE) {
			fprintf(stderr, "BUG: LRU forward walk detected cycle (count=%d)\n", count);
			return -1;
		}

		// Check prev/next consistency
		int next = sdlt[idx].next;
		if (next != STX_NONE) {
			if (next < 0 || next >= MAX_TEXCACHE) {
				fprintf(stderr, "BUG: LRU entry %d has out-of-range next=%d\n", idx, next);
				return -1;
			}
			if (sdlt[next].prev != idx) {
				fprintf(stderr, "BUG: LRU entry %d points to next=%d, but that entry's prev=%d\n", idx, next,
				    sdlt[next].prev);
				return -1;
			}
		}

		idx = next;
	}

	return 0;
}

static int sdl_check_job_queue_invariants(void)
{
	texture_job_queue_t *q = &g_tex_jobs;

	SDL_LockMutex(q->mutex);

	// Basic queue state
	if (q->count < 0 || q->count > TEX_JOB_CAPACITY) {
		fprintf(stderr, "BUG: job queue count=%d out of range [0, %d]\n", q->count, TEX_JOB_CAPACITY);
		SDL_UnlockMutex(q->mutex);
		return -1;
	}

	if (q->head < 0 || q->head >= TEX_JOB_CAPACITY) {
		fprintf(stderr, "BUG: job queue head=%d out of range [0, %d)\n", q->head, TEX_JOB_CAPACITY);
		SDL_UnlockMutex(q->mutex);
		return -1;
	}

	if (q->tail < 0 || q->tail >= TEX_JOB_CAPACITY) {
		fprintf(stderr, "BUG: job queue tail=%d out of range [0, %d)\n", q->tail, TEX_JOB_CAPACITY);
		SDL_UnlockMutex(q->mutex);
		return -1;
	}

	// Check that queued jobs reference valid cache indices
	int checked = 0;
	int idx = q->head;
	for (int i = 0; i < q->count; i++) {
		texture_job_t *job = &q->jobs[idx];

		if (job->cache_index < 0 || job->cache_index >= MAX_TEXCACHE) {
			fprintf(stderr, "BUG: queued job at slot %d has invalid cache_index=%d\n", idx, job->cache_index);
			SDL_UnlockMutex(q->mutex);
			return -1;
		}

		if (job->generation == 0) {
			fprintf(stderr, "BUG: queued job at slot %d has generation==0\n", idx);
			SDL_UnlockMutex(q->mutex);
			return -1;
		}

		idx = (idx + 1) % TEX_JOB_CAPACITY;
		checked++;

		if (checked > TEX_JOB_CAPACITY) {
			fprintf(stderr, "BUG: job queue appears to have infinite loop\n");
			SDL_UnlockMutex(q->mutex);
			return -1;
		}
	}

	SDL_UnlockMutex(q->mutex);
	return 0;
}

int sdl_check_invariants_for_tests(void)
{
	int i;

	// 1. Check all texture entries
	for (i = 0; i < MAX_TEXCACHE; i++) {
		if (sdl_check_texture_entry_invariants(i) != 0) {
			return -1;
		}
	}

	// 2. Check hash chains
	if (sdl_check_hash_chain_invariants() != 0) {
		return -1;
	}

	// 3. Check LRU list
	if (sdl_check_lru_list_invariants() != 0) {
		return -1;
	}

	// 4. Check job queue
	if (sdl_check_job_queue_invariants() != 0) {
		return -1;
	}

	return 0;
}

// ============================================================================
// GPU Stub Implementations (SDL Texture Operations)
// ============================================================================

// We use REAL I/O (PNG loading, decompression, CPU processing)
// but STUB GPU operations (which require a real renderer/window)

// Fake SDL_Texture object for tests
static int dummy_texture;

// Stub: Create texture (would normally require GPU renderer)
SDL_Texture *SDL_CreateTexture(SDL_Renderer *renderer __attribute__((unused)), Uint32 format __attribute__((unused)),
    int access __attribute__((unused)), int w __attribute__((unused)), int h __attribute__((unused)))
{
	// Return non-NULL pointer (cache code just checks != NULL)
	return (SDL_Texture *)&dummy_texture;
}

// Stub: Update texture with pixel data
int SDL_UpdateTexture(SDL_Texture *texture __attribute__((unused)), const SDL_Rect *rect __attribute__((unused)),
    const void *pixels __attribute__((unused)), int pitch __attribute__((unused)))
{
	// Success - we don't actually upload to GPU
	return 0;
}

// Stub: Destroy texture
void SDL_DestroyTexture(SDL_Texture *texture __attribute__((unused)))
{
	// No-op in tests
}

// Stub: Query texture info
int SDL_QueryTexture(SDL_Texture *texture __attribute__((unused)), Uint32 *format, int *access, int *w, int *h)
{
	// Return fake dimensions if requested
	if (format) {
		*format = SDL_PIXELFORMAT_ARGB8888;
	}
	if (access) {
		*access = SDL_TEXTUREACCESS_STATIC;
	}
	if (w) {
		*w = 64;
	}
	if (h) {
		*h = 64;
	}
	return 0;
}

// ============================================================================
// Test-only introspection helpers
// ============================================================================

// Return flags for a cache entry (read-only, no side effects)
uint16_t sdl_texture_get_flags_for_test(int cache_index)
{
	if (cache_index < 0 || cache_index >= MAX_TEXCACHE) {
		return 0;
	}
	return flags_load(&sdlt[cache_index]);
}

// Return sprite id for a cache entry (read-only, no side effects)
int sdl_texture_get_sprite_for_test(int cache_index)
{
	if (cache_index < 0 || cache_index >= MAX_TEXCACHE) {
		return -1;
	}
	return sdlt[cache_index].sprite;
}

// Return work_state for a cache entry (read-only, no side effects)
uint8_t sdl_texture_get_work_state_for_test(int cache_index)
{
	if (cache_index < 0 || cache_index >= MAX_TEXCACHE) {
		return 0xFF; // Invalid
	}
	return work_state_load(&sdlt[cache_index]);
}

// Return rough job queue depth (read-only, no side effects)
int sdl_get_job_queue_depth_for_test(void)
{
	SDL_LockMutex(g_tex_jobs.mutex);
	int depth = g_tex_jobs.count;
	SDL_UnlockMutex(g_tex_jobs.mutex);
	return depth;
}

#endif /* UNIT_TEST */
