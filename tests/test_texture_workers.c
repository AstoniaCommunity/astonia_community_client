/*
 * Worker Concurrency Tests - Test multi-threaded texture loading
 *
 * Tests the worker thread system, prefetch pipeline, and concurrent cache access.
 * All tests use real PNG loading from ZIP files, but fake GPU texture creation.
 */

#include "../src/astonia.h"  // Must come first for tick_t and other typedefs
#include "../src/sdl/sdl_private.h"
#include "test.h"

#include <string.h>
#include <time.h>
#include <zip.h>

// ============================================================================
// Test sprite enumeration
// ============================================================================

static unsigned int valid_sprites[100000];
static int num_valid_sprites = 0;

// Enumerate valid sprites from gx1.zip (called once at startup)
// Does thorough validation: PNG signature, can be opened, has dimensions
static void enumerate_valid_sprites(void)
{
	if (num_valid_sprites > 0)
		return; // Already enumerated

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

	for (zip_int64_t i = 0; i < n && num_valid_sprites < 50145; i++) {
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
// Single-threaded pipeline test
// ============================================================================

TEST(test_single_thread_pipeline)
{
	ASSERT_TRUE(sdl_init_for_tests());
	enumerate_valid_sprites();
	ASSERT_TRUE(num_valid_sprites > 0); // Ensure we have sprites to test

	fprintf(stderr, "  → Testing single-thread pipeline (synchronous load + prefetch)...\n");

	unsigned int sprite = get_valid_sprite(0);

	// Load sprite synchronously
	int cache_idx = sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
	ASSERT_IN_RANGE(cache_idx, 0, MAX_TEXCACHE - 1);

	// In single-threaded mode, texture should be immediately available (or nearly so)
	uint16_t flags = sdl_texture_get_flags_for_test(cache_idx);
	ASSERT_TRUE(flags & SF_USED);
	ASSERT_TRUE(flags & SF_DIDALLOC);

	// Now simulate a prefetch of same sprite
	sdl_pre_add(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	// Pump pipeline
	for (int i = 0; i < 100; i++) {
		sdl_pre_tick_for_tests();
		flags = sdl_texture_get_flags_for_test(cache_idx);
		if ((flags & SF_DIDMAKE) && (flags & SF_DIDTEX)) {
			break; // Success!
		}
	}

	// Verify final state
	flags = sdl_texture_get_flags_for_test(cache_idx);
	ASSERT_TRUE(flags & SF_DIDMAKE);
	ASSERT_TRUE(flags & SF_DIDTEX);

	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Single-thread pipeline works (load + prefetch complete)\n");

	sdl_shutdown_for_tests();
}

// ============================================================================
// Multi-threaded worker tests
// ============================================================================

TEST(test_workers_process_jobs)
{
	ASSERT_TRUE(sdl_init_for_tests_with_workers(4));
	enumerate_valid_sprites();

	fprintf(stderr, "  → Testing 4 workers processing 1000 sprites...\n");

	int cache_indices[1000];
	
	// Load 1000 different sprites with workers
	for (int i = 0; i < 1000; i++) {
		unsigned int sprite = get_valid_sprite(i);
		cache_indices[i] = sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0,
		                                NULL, 0, 0);
		ASSERT_IN_RANGE(cache_indices[i], 0, MAX_TEXCACHE - 1);

		// Queue prefetch for this sprite (workers will process)
		sdl_pre_add(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}

	// Pump pipeline with workers running
	fprintf(stderr, "  → Pumping pipeline with 4 workers...\n");
	for (int tick = 0; tick < 2000; tick++) {
		sdl_pre_tick_for_tests();
		SDL_Delay(1); // Let workers make progress
		
		// Check invariants periodically
		if ((tick % 500) == 0) {
			ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());
		}
	}

	// Verify sprites were processed
	int completed = 0;
	for (int i = 0; i < 1000; i++) {
		uint16_t flags = sdl_texture_get_flags_for_test(cache_indices[i]);
		if ((flags & SF_DIDMAKE) && (flags & SF_DIDTEX)) {
			completed++;
		}
	}

	fprintf(stderr, "  → %d/1000 sprites completed by workers\n", completed);
	ASSERT_TRUE(completed >= 950); // At least 95% should complete

	// Final invariant check
	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Workers processed 1000 sprites successfully\n");

	sdl_shutdown_for_tests();
}

TEST(test_workers_saturate_cache)
{
	ASSERT_TRUE(sdl_init_for_tests_with_workers(4));
	enumerate_valid_sprites();

	fprintf(stderr, "  → Saturating cache with 4 workers (32,768 entries)...\n");

	// Fill the entire cache with workers processing
	int loaded = 0;
	for (int i = 0; i < MAX_TEXCACHE && i < num_valid_sprites; i++) {
		unsigned int sprite = get_valid_sprite(i);
		int idx = sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
		if (idx != STX_NONE) {
			loaded++;
			// Queue prefetch for background processing
			sdl_pre_add(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		}
		
		// Progress indicator
		if ((i % 5000) == 0 && i > 0) {
			fprintf(stderr, "  Loaded %d/%d textures...\n", i, MAX_TEXCACHE);
		}
		
	// Periodically pump and check invariants
	if ((i % 1000) == 0) {
		for (int tick = 0; tick < 10; tick++) {
			sdl_pre_tick_for_tests();
		}
		ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());
	}
	}

	fprintf(stderr, "  → Cache saturated! Pumping pipeline for workers to finish...\n");

	// Give workers time to finish all jobs
	for (int tick = 0; tick < 3000; tick++) {
		sdl_pre_tick_for_tests();
		SDL_Delay(1);
		
		if ((tick % 1000) == 0) {
			int queue_depth = sdl_get_job_queue_depth_for_test();
			fprintf(stderr, "  → Queue depth: %d\n", queue_depth);
		}
	}

	// Final invariant check
	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	// Count completed textures
	int completed = 0;
	for (int i = 0; i < MAX_TEXCACHE; i++) {
		uint16_t flags = sdl_texture_get_flags_for_test(i);
		if ((flags & SF_USED) && (flags & SF_DIDMAKE)) {
			completed++;
		}
	}

	fprintf(stderr, "  → Workers completed %d/%d textures\n", completed, loaded);
	ASSERT_TRUE(completed >= loaded * 90 / 100); // At least 90% complete

	fprintf(stderr, "  ✓ Workers successfully saturated cache\n");

	sdl_shutdown_for_tests();
}

// ============================================================================
// Concurrency edge cases (eviction during worker processing)
// ============================================================================

TEST(test_workers_with_eviction)
{
	ASSERT_TRUE(sdl_init_for_tests_with_workers(4));
	enumerate_valid_sprites();

	fprintf(stderr, "  → Testing workers with eviction (thrashing cache)...\n");

	// Load MORE sprites than cache can hold, forcing evictions
	const int num_sprites = MAX_TEXCACHE + 5000;
	
	for (int i = 0; i < num_sprites && i < num_valid_sprites; i++) {
		unsigned int sprite = get_valid_sprite(i);
		sdl_tx_load(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
		sdl_pre_add(sprite, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		
		// Progress
		if ((i % 5000) == 0 && i > 0) {
			fprintf(stderr, "  Loaded %d sprites (evicting old entries)...\n", i);
		}
		
	// Pump and check invariants periodically
	if ((i % 1000) == 0) {
		for (int tick = 0; tick < 10; tick++) {
			sdl_pre_tick_for_tests();
		}
		ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());
	}
	}

	fprintf(stderr, "  → Pumping pipeline for final processing...\n");
	
	// Final processing
	for (int tick = 0; tick < 2000; tick++) {
		sdl_pre_tick_for_tests();
		SDL_Delay(1);
		
		if ((tick % 500) == 0) {
			ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());
		}
	}

	// Final invariant check
	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Workers handled eviction correctly (no corruption)\n");

	sdl_shutdown_for_tests();
}

// ============================================================================
// Worker fuzz test - random operations with multiple threads
// ============================================================================

TEST(test_worker_fuzz)
{
	ASSERT_TRUE(sdl_init_for_tests_with_workers(4));
	enumerate_valid_sprites();

	// Configurable seed
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

	fprintf(stderr, "  → Running 1 million random ops with 4 workers (fuzz test)...\n");

	// Use a smaller subset of sprites to avoid invalid ones
	const int sprite_pool_size = (num_valid_sprites > 5000) ? 5000 : num_valid_sprites;
	
	const int ops = 1000000;
	for (int i = 0; i < ops; i++) {
		int choice = test_rng_range(0, 2);

		switch (choice) {
		case 0: { // Random load (may cause eviction)
			int sprite_idx = test_rng_range(0, sprite_pool_size - 1);
			unsigned int sprite = get_valid_sprite(sprite_idx);
			int scale = test_rng_range(1, 3);
			int cr = test_rng_range(0, 255);
			int cg = test_rng_range(0, 255);
			int cb = test_rng_range(0, 255);
		int preload = test_rng_range(0, 1);

		(void)sdl_tx_load(sprite, 0, 0, scale, cr, cg, cb, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, NULL,
		                  0, preload);
			break;
		}
		case 1: { // Random prefetch add
			int sprite_idx = test_rng_range(0, sprite_pool_size - 1);
			unsigned int sprite = get_valid_sprite(sprite_idx);
			int scale = test_rng_range(1, 3);
			int cr = test_rng_range(0, 255);
			int cg = test_rng_range(0, 255);
		int cb = test_rng_range(0, 255);

		sdl_pre_add(sprite, 0, 0, scale, cr, cg, cb, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		break;
		}
	case 2: { // Pipeline tick
		sdl_pre_tick_for_tests();
		break;
	}
		}

		// Check invariants every 1000 ops
		if ((i % 1000) == 0) {
			ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());
		}
	}

	// Final pump to finish pending jobs
	for (int tick = 0; tick < 200; tick++) {
		sdl_pre_tick_for_tests();
		SDL_Delay(1);
	}

	// Final invariant check
	ASSERT_EQ_INT(0, sdl_check_invariants_for_tests());

	fprintf(stderr, "  ✓ Worker fuzz test passed (1 million ops, 4 threads, all invariants held)\n");

	sdl_shutdown_for_tests();
}

// ============================================================================
// Test runner
// ============================================================================

TEST_MAIN(
    fprintf(stderr, "\n=== Single-Threaded Pipeline Tests ===\n");
    test_single_thread_pipeline();

    fprintf(stderr, "\n=== Multi-Threaded Worker Tests ===\n");
    test_workers_process_jobs();
    test_workers_saturate_cache();

    fprintf(stderr, "\n=== Concurrency Edge Cases ===\n");
    test_workers_with_eviction();

    fprintf(stderr, "\n=== Worker Fuzz Tests ===\n");
    test_worker_fuzz();
)
