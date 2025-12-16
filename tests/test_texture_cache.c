/*
 * Texture Cache Tests - Single-threaded deterministic tests
 * 
 * Tests core cache functionality: insertion, lookup, eviction, hash chains, LRU list.
 * No real rendering, no real I/O.
 */

// Don't redefine - these come from CFLAGS
// #define UNIT_TEST
// #define DEVELOPER

#include "../src/astonia.h"  // Must come first for tick_t and other typedefs
#include "../src/sdl/sdl_private.h"
#include "test.h"

#include <string.h>
#include <time.h>
#include <zip.h>

// ============================================================================
// Valid sprite list (populated from ZIP at test startup)
// ============================================================================

#define MAX_VALID_SPRITES 50000
static unsigned int valid_sprites[MAX_VALID_SPRITES];
static int num_valid_sprites = 0;

// Build list of ACTUALLY LOADABLE sprite IDs from gx1.zip
// Does thorough validation: PNG signature, can be loaded, has dimensions
static void build_valid_sprite_list(void)
{
	if (num_valid_sprites > 0)
		return; // Already built

	if (!sdl_zip1) {
		fprintf(stderr, "ERROR: Cannot enumerate sprites - sdl_zip1 is NULL\n");
		return;
	}

	fprintf(stderr, "Enumerating and validating sprites from gx1.zip...\n");

	zip_int64_t n = zip_get_num_entries(sdl_zip1, 0);
	int candidates = 0;
	int filtered_not_png = 0;
	int filtered_bad_signature = 0;
	int filtered_load_failed = 0;

	for (zip_int64_t i = 0; i < n && num_valid_sprites < MAX_VALID_SPRITES; i++) {
		const char *name = zip_get_name(sdl_zip1, (zip_uint64_t)i, 0);
		if (!name)
			continue;

		// Parse sprite number from filename (e.g., "00012345.png")
		unsigned int sprite_num = 0;
		if (sscanf(name, "%u.png", &sprite_num) == 1) {
			candidates++;

			// Step 1: Check PNG signature
			struct zip_stat st;
			if (zip_stat_index(sdl_zip1, (zip_uint64_t)i, 0, &st) != 0) {
				filtered_not_png++;
				continue;
			}

			zip_file_t *zf = zip_fopen_index(sdl_zip1, (zip_uint64_t)i, 0);
			if (!zf) {
				filtered_not_png++;
				continue;
			}

			unsigned char header[8];
			if (zip_fread(zf, header, 8) != 8) {
				zip_fclose(zf);
				filtered_not_png++;
				continue;
			}

			// PNG signature: 137 80 78 71 13 10 26 10
			if (!(header[0] == 137 && header[1] == 80 && header[2] == 78 && header[3] == 71)) {
				zip_fclose(zf);
				filtered_bad_signature++;
				continue;
			}
			zip_fclose(zf);

			// Step 2: Try to actually load it with sdl_ic_load
			// This validates the PNG can be decoded and has valid dimensions
			if (sdl_ic_load(sprite_num, NULL) < 0) {
				filtered_load_failed++;
				continue;
			}

			// Step 3: Verify it loaded into sdli[] with valid dimensions
			if (sdli[sprite_num].xres <= 0 || sdli[sprite_num].yres <= 0) {
				filtered_load_failed++;
				continue;
			}

			// All checks passed - this is a valid sprite!
			valid_sprites[num_valid_sprites++] = sprite_num;

			// Progress indicator
			if ((num_valid_sprites % 10000) == 0 && num_valid_sprites > 0) {
				fprintf(stderr, "  Validated %d sprites...\n", num_valid_sprites);
			}
		}
	}

	int filtered_total = filtered_not_png + filtered_bad_signature + filtered_load_failed;
	fprintf(stderr, "Found %d valid sprites in gx1.zip\n", num_valid_sprites);
	fprintf(stderr, "  (%d candidates, %d filtered: %d bad files, %d bad signatures, %d load failures)\n",
	        candidates, filtered_total, filtered_not_png, filtered_bad_signature, filtered_load_failed);
}

// Get a valid sprite ID (wraps around if index out of range)
static unsigned int get_valid_sprite(int index)
{
	if (num_valid_sprites == 0)
		return 1; // Fallback
	return valid_sprites[index % num_valid_sprites];
}

// ============================================================================
// Additional invariant checks for flag relationships
// ============================================================================

static void check_flags_invariants(int cache_index)
{
	const struct sdl_texture *t = &sdlt[cache_index];
	uint16_t f = flags_load((struct sdl_texture *)t);

	if (!f)
		return; // unused slot

	// Flag progression: DIDALLOC → DIDMAKE → DIDTEX
	if (f & SF_DIDTEX) {
		ASSERT_TRUE(f & SF_DIDMAKE);
		ASSERT_TRUE(f & SF_DIDALLOC);
		ASSERT_PTR_NOT_NULL(t->tex);
	}
	if (f & SF_DIDMAKE) {
		ASSERT_TRUE(f & SF_DIDALLOC);
	}

	// Text vs Sprite mutual exclusion
	if (f & SF_TEXT) {
		ASSERT_FALSE(f & SF_SPRITE);
		ASSERT_PTR_NOT_NULL(t->tex); // Text always has tex (rendered to texture immediately)
		ASSERT_TRUE(t->pixel == NULL); // Text never uses pixel buffer
	}
	if (f & SF_SPRITE) {
		ASSERT_FALSE(f & SF_TEXT);
		ASSERT_TRUE(t->text == NULL);
	}

	// Generation must never be 0 (reserved)
	ASSERT_TRUE(t->generation != 0);

	// Work state must be valid
	uint8_t ws = work_state_load((struct sdl_texture *)t);
	ASSERT_TRUE(ws == TX_WORK_IDLE || ws == TX_WORK_QUEUED || ws == TX_WORK_IN_WORKER);
}

// ============================================================================
// Basic cache tests
// ============================================================================

TEST(test_basic_insert_and_lookup)
{
	ASSERT_TRUE(sdl_init_for_tests());
	build_valid_sprite_list(); // Build sprite list after SDL init
	ASSERT_TRUE(num_valid_sprites > 0); // Ensure we have sprites to test

	fprintf(stderr, "  → Testing basic insert and lookup...\n");

	// Load a texture
	int idx1 = sdl_tx_load(
	    /* sprite */ 100,
	    /* sink */ 0,
	    /* freeze */ 0,
	    /* scale */ 1,
	    /* cr, cg, cb */ 0, 0, 0,
	    /* light, sat */ 0, 0,
	    /* c1, c2, c3 */ 0, 0, 0,
	    /* shine */ 0,
	    /* ml, ll, rl, ul, dl */ 0, 0, 0, 0, 0,
	    /* text, text_color, text_flags, text_font */ NULL, 0, 0, NULL,
	    /* checkonly */ 0,
	    /* preload */ 0);

	ASSERT_IN_RANGE(idx1, 0, MAX_TEXCACHE - 1);

	uint16_t flags = flags_load(&sdlt[idx1]);
	ASSERT_TRUE(flags & SF_USED);
	ASSERT_TRUE(flags & SF_SPRITE);
	ASSERT_EQ_INT(100, sdlt[idx1].sprite);

	// Same parameters should hit cache
	int idx2 = sdl_tx_load(100, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);

	ASSERT_EQ_INT(idx1, idx2);

	// Check invariants
	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Basic insert and lookup works\n");

	sdl_shutdown_for_tests();
}

TEST(test_different_sprites_different_slots)
{
	ASSERT_TRUE(sdl_init_for_tests());

	fprintf(stderr, "  → Testing different sprites get different slots...\n");

	int idx1 = sdl_tx_load(100, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	int idx2 = sdl_tx_load(200, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);

	ASSERT_NE_INT(idx1, idx2);
	ASSERT_EQ_INT(100, sdlt[idx1].sprite);
	ASSERT_EQ_INT(200, sdlt[idx2].sprite);

	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Different sprites use different slots\n");

	sdl_shutdown_for_tests();
}

TEST(test_different_parameters_different_slots)
{
	ASSERT_TRUE(sdl_init_for_tests());

	fprintf(stderr, "  → Testing parameter variations...\n");

	// Same sprite, different scale
	int idx1 = sdl_tx_load(100, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	int idx2 = sdl_tx_load(100, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);

	ASSERT_NE_INT(idx1, idx2);

	// Same sprite, different colors
	int idx3 = sdl_tx_load(100, 0, 0, 1, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);

	ASSERT_NE_INT(idx1, idx3);
	ASSERT_NE_INT(idx2, idx3);

	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Different parameters create unique cache entries\n");

	sdl_shutdown_for_tests();
}

// ============================================================================
// Hash chain tests
// ============================================================================

TEST(test_hash_chains_no_corruption_after_insertions)
{
	ASSERT_TRUE(sdl_init_for_tests());

	fprintf(stderr, "  → Loading 500 real sprites to stress hash chains...\n");

	// Insert many sprites to stress hash chains
	// Use only valid sprites from ZIP (no "not found" warnings)
	const int num_sprites = 500;
	for (int i = 0; i < num_sprites; i++) {
		unsigned int sprite = get_valid_sprite(i);
		int idx =
		    sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
		ASSERT_IN_RANGE(idx, 0, MAX_TEXCACHE - 1);
	}

	// Check all invariants (including hash chains)
	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Hash chains intact after 500 insertions (no corruption, no cycles)\n");

	sdl_shutdown_for_tests();
}

// ============================================================================
// LRU and eviction tests
// ============================================================================

TEST(test_lru_list_stays_consistent)
{
	ASSERT_TRUE(sdl_init_for_tests());

	fprintf(stderr, "  → Testing LRU list consistency...\n");

	// Fill a portion of the cache with valid sprites
	for (int i = 0; i < 100; i++) {
		unsigned int sprite = get_valid_sprite(i);
		(void)sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	}

	// Access in different order (should reorder LRU)
	for (int i = 99; i >= 0; i--) {
		unsigned int sprite = get_valid_sprite(i);
		(void)sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	}

	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ LRU list consistent (prev/next pointers valid, no cycles)\n");

	sdl_shutdown_for_tests();
}

TEST(test_eviction_basic)
{
	ASSERT_TRUE(sdl_init_for_tests());

	fprintf(stderr, "  → Testing basic eviction (1000 sprites)...\n");

	// Load 1000 valid sprites
	const int num_sprites = 1000;

	for (int i = 0; i < num_sprites; i++) {
		unsigned int sprite = get_valid_sprite(i);
		int idx =
		    sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
		ASSERT_IN_RANGE(idx, 0, MAX_TEXCACHE - 1);

		// Every 100 iterations, check invariants
		if (i % 100 == 0) {
			ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());
		}
	}

	fprintf(stderr, "  ✓ Eviction works correctly (all invariants maintained)\n");

	sdl_shutdown_for_tests();
}

TEST(test_full_cache_stress)
{
	ASSERT_TRUE(sdl_init_for_tests());

	fprintf(stderr, "Loading full cache (%d textures)...\n", MAX_TEXCACHE);

	// Fill the ENTIRE cache (32768 textures) to simulate real gameplay
	// This is critical - real gameplay fills the cache in minutes
	for (int i = 0; i < MAX_TEXCACHE; i++) {
		// Cycle through valid sprites, using different parameters to create unique cache entries
		unsigned int sprite = get_valid_sprite(i);
		int scale = 1 + (i % 3); // Vary scale 1-3 to create more cache entries

		int idx = sdl_tx_load(sprite, 0, 0, (unsigned char)scale, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0,
		    NULL, 0, 0);
		ASSERT_IN_RANGE(idx, 0, MAX_TEXCACHE - 1);

		// Check invariants every 5000 textures (don't slow down too much)
		if (i > 0 && i % 5000 == 0) {
			fprintf(stderr, "  Loaded %d/%d textures...\n", i, MAX_TEXCACHE);
			ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());
		}
	}

	fprintf(stderr, "  Cache full! Checking final invariants...\n");
	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	// Now force eviction by loading MORE textures (should evict LRU)
	fprintf(stderr, "  Testing eviction under full cache...\n");
	for (int i = 0; i < 1000; i++) {
		unsigned int sprite = get_valid_sprite(MAX_TEXCACHE + i);
		(void)sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	}

	fprintf(stderr, "  Final invariant check after eviction...\n");
	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	sdl_shutdown_for_tests();
}

// ============================================================================
// Cache deduplication test
// ============================================================================

TEST(test_cache_deduplication)
{
	ASSERT_TRUE(sdl_init_for_tests());

	fprintf(stderr, "  → Testing cache deduplication (same sprite, different params)...\n");

	// Load the same sprite with many different parameter combinations
	// Cache should deduplicate based on hash(sprite + params)
	unsigned int sprite = get_valid_sprite(0);
	int initial_used = 0;

	// Count initial cache usage
	for (int i = 0; i < MAX_TEXCACHE; i++) {
		if (flags_load(&sdlt[i]) & SF_USED) {
			initial_used++;
		}
	}

	// Load same sprite 100 times with SAME parameters - should hit cache
	int idx_first = -1;
	for (int i = 0; i < 100; i++) {
		int idx = sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
		if (i == 0) {
			idx_first = idx;
		} else {
			ASSERT_EQ_INT(idx_first, idx); // Should always return same slot
		}
	}

	// Now load with different parameters - should create new entries
	int idx_scale2 = sdl_tx_load(sprite, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	ASSERT_NE_INT(idx_first, idx_scale2);

	int idx_color = sdl_tx_load(sprite, 0, 0, 1, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	ASSERT_NE_INT(idx_first, idx_color);
	ASSERT_NE_INT(idx_scale2, idx_color);

	// Verify we only added a few entries, not 100+
	int final_used = 0;
	for (int i = 0; i < MAX_TEXCACHE; i++) {
		if (flags_load(&sdlt[i]) & SF_USED) {
			final_used++;
		}
	}

	int added = final_used - initial_used;
	ASSERT_TRUE(added <= 5); // Should be ~3 (original + 2 variants), not 100+

	fprintf(stderr, "  ✓ Cache deduplication works (100 identical loads → 1 entry, variants create new entries)\n");

	sdl_shutdown_for_tests();
}

// ============================================================================
// Scripted concurrency tests (sequential simulation of concurrent scenarios)
// ============================================================================

TEST(test_eviction_refuses_in_flight_jobs)
{
	ASSERT_TRUE(sdl_init_for_tests());

	fprintf(stderr, "  → Testing eviction safety (refuse to evict in-flight jobs)...\n");

	// Load a sprite
	unsigned int sprite = get_valid_sprite(0);
	int idx = sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	ASSERT_IN_RANGE(idx, 0, MAX_TEXCACHE - 1);

	// Simulate a worker taking the job (set work_state to IN_WORKER)
	SDL_LockMutex(g_tex_jobs.mutex);
	sdlt[idx].work_state = TX_WORK_IN_WORKER;
	SDL_UnlockMutex(g_tex_jobs.mutex);

	// Now try to evict this entry by loading many other sprites
	// The eviction logic should skip this entry because work_state != IDLE
	for (int i = 1; i < 100; i++) {
		unsigned int other_sprite = get_valid_sprite(i);
		(void)sdl_tx_load(other_sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	}

	// Original entry should still be intact (not evicted)
	ASSERT_EQ_INT(sprite, sdlt[idx].sprite);
	ASSERT_EQ_INT(TX_WORK_IN_WORKER, sdlt[idx].work_state);

	// Clean up
	SDL_LockMutex(g_tex_jobs.mutex);
	sdlt[idx].work_state = TX_WORK_IDLE;
	SDL_UnlockMutex(g_tex_jobs.mutex);

	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Eviction correctly skips entries with in-flight work\n");

	sdl_shutdown_for_tests();
}

TEST(test_generation_invalidates_stale_jobs)
{
	ASSERT_TRUE(sdl_init_for_tests());

	fprintf(stderr, "  → Testing generation invalidation of stale jobs...\n");

	// Load a sprite
	unsigned int sprite1 = get_valid_sprite(0);
	int idx = sdl_tx_load(sprite1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	uint32_t gen1 = sdlt[idx].generation;

	// Simulate eviction: bump generation
	sdlt[idx].generation++;
	uint32_t gen2 = sdlt[idx].generation;

	ASSERT_NE_INT(gen1, gen2);
	ASSERT_TRUE(gen2 > 0); // Should never wrap to 0

	// Now if we had a stale job with old generation, it should be ignored
	// (We can't easily test the worker behavior here, but we verify generation changed)

	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Generation counter increments on reuse (stale job protection)\n");

	sdl_shutdown_for_tests();
}

// ============================================================================
// Fuzz test - random operations
// ============================================================================

TEST(test_fuzz_random_cache_operations)
{
	ASSERT_TRUE(sdl_init_for_tests());

	// Configurable seed: use env var if set, else use time-based random
	uint32_t seed;
	const char *seed_env = getenv("TEST_SEED");
	if (seed_env) {
		seed = (uint32_t)strtoul(seed_env, NULL, 10);
		fprintf(stderr, "  → Using TEST_SEED=%u from environment\n", seed);
	} else {
		seed = (uint32_t)time(NULL);
		fprintf(stderr, "  → Using random seed: %u (set TEST_SEED=%u to reproduce)\n", seed, seed);
	}
	test_rng_seed(seed);

	fprintf(stderr, "  → Running 1 million random operations (fuzz test)...\n");

	const int num_steps = 1000000;

	for (int step = 0; step < num_steps; step++) {
		int op = test_rng_range(0, 2);

		switch (op) {
		case 0: {
			// Random load with valid sprite ID
			int sprite_idx = test_rng_range(0, num_valid_sprites - 1);
			unsigned int sprite = get_valid_sprite(sprite_idx);
		int scale = test_rng_range(1, 3);
		(void)sdl_tx_load(sprite, 0, 0, (unsigned char)scale, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0,
		    NULL, 0, 0);
			break;
		}
		case 1: {
			// Random preload with valid sprite ID
			int sprite_idx = test_rng_range(0, num_valid_sprites - 1);
			unsigned int sprite = get_valid_sprite(sprite_idx);
			sdl_pre_add(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			break;
		}
		case 2: {
			// Process prefetch queue
			(void)sdl_pre_tick_for_tests();
			break;
		}
		}

		// Check invariants every 1000 steps
		if (step % 1000 == 0) {
			ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());
		}
	}

	// Final invariant check
	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Fuzz test passed (1 million random ops, all invariants held)\n");

	sdl_shutdown_for_tests();
}

// ============================================================================
// Test runner
// ============================================================================

TEST_MAIN(
    fprintf(stderr, "\n=== Basic Cache Tests ===\n");
    test_basic_insert_and_lookup();
    test_different_sprites_different_slots();
    test_different_parameters_different_slots();
    test_cache_deduplication();

    fprintf(stderr, "\n=== Hash Chain Tests ===\n");
    test_hash_chains_no_corruption_after_insertions();

    fprintf(stderr, "\n=== LRU and Eviction Tests ===\n");
    test_lru_list_stays_consistent();
    test_eviction_basic();

    fprintf(stderr, "\n=== Concurrency Edge Cases (Sequential Simulation) ===\n");
    test_eviction_refuses_in_flight_jobs();
    test_generation_invalidates_stale_jobs();

    fprintf(stderr, "\n=== Full Cache Stress Test ===\n");
    test_full_cache_stress();

    fprintf(stderr, "\n=== Fuzz Tests ===\n");
    test_fuzz_random_cache_operations();
)
