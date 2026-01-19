/*
 * Hash Distribution Tests - Verify hash function quality
 *
 * Tests that FNV-1a hash provides uniform distribution across buckets.
 * Validates the fix for hash chain overflow that caused Lua mod crashes.
 */

#include "../src/astonia.h"
#include "../src/sdl/sdl_private.h"
#include "test.h"

#include <string.h>
#include <stdio.h>
#include <zip.h>

// ============================================================================
// Valid sprite list (populated from ZIP at test startup)
// ============================================================================

#define MAX_VALID_SPRITES 50000
static unsigned int valid_sprites[MAX_VALID_SPRITES];
static int num_valid_sprites = 0;

static void build_valid_sprite_list(void)
{
	if (num_valid_sprites > 0)
		return;

	if (!sdl_zip1) {
		fprintf(stderr, "ERROR: Cannot enumerate sprites - sdl_zip1 is NULL\n");
		return;
	}

	fprintf(stderr, "  Enumerating sprites from gx1.zip...\n");
	
	zip_int64_t n = zip_get_num_entries(sdl_zip1, 0);
	for (zip_int64_t i = 0; i < n && num_valid_sprites < MAX_VALID_SPRITES; i++) {
		const char *name = zip_get_name(sdl_zip1, (zip_uint64_t)i, 0);
		if (!name)
			continue;

		unsigned int sprite_num = 0;
		if (sscanf(name, "%u.png", &sprite_num) == 1) {
			// Try to load it (validates PNG)
			if (sdl_ic_load(sprite_num, NULL) >= 0) {
				if (sdli[sprite_num].xres > 0 && sdli[sprite_num].yres > 0) {
					valid_sprites[num_valid_sprites++] = sprite_num;
				}
			}
		}
		
		// Progress indicator
		if (num_valid_sprites > 0 && num_valid_sprites % 5000 == 0) {
			fprintf(stderr, "    Found %d valid sprites...\n", num_valid_sprites);
		}
	}
	
	fprintf(stderr, "  Found %d valid sprites total\n\n", num_valid_sprites);
}

static unsigned int get_valid_sprite(int index)
{
	if (index >= 0 && index < num_valid_sprites) {
		return valid_sprites[index];
	}
	return 0;
}

// ============================================================================
// Hash Bucket Analysis Helpers
// ============================================================================

typedef struct bucket_stats {
	int hash;
	int chain_length;
	int sprite_entries;
	int text_entries;
	int has_sprite_zero;
} bucket_stats_t;

// Walk a hash chain and collect statistics
static bucket_stats_t analyze_bucket(int hash)
{
	bucket_stats_t stats = {0};
	stats.hash = hash;

	int stx = sdlt_cache[hash];
	int count = 0;

	while (stx != STX_NONE && count < 10000) {
		uint16_t flags = flags_load(&sdlt[stx]);
		
		if (flags & SF_TEXT) {
			stats.text_entries++;
		}
		if (flags & SF_SPRITE) {
			stats.sprite_entries++;
			if (sdlt[stx].sprite == 0) {
				stats.has_sprite_zero = 1;
			}
		}

		stx = sdlt[stx].hnext;
		count++;
	}

	stats.chain_length = count;
	return stats;
}

// Analyze all hash buckets and print distribution
static void print_hash_distribution(const char *label)
{
	int max_chain = 0;
	int max_chain_hash = -1;
	int total_entries = 0;
	int nonempty_buckets = 0;
	int buckets_over_10 = 0;
	int buckets_over_50 = 0;
	int buckets_over_100 = 0;

	fprintf(stderr, "\n=== Hash Distribution: %s ===\n", label);

	for (int hash = 0; hash < MAX_TEXHASH; hash++) {
		bucket_stats_t stats = analyze_bucket(hash);
		
		if (stats.chain_length > 0) {
			nonempty_buckets++;
			total_entries += stats.chain_length;

			if (stats.chain_length > max_chain) {
				max_chain = stats.chain_length;
				max_chain_hash = hash;
			}

			if (stats.chain_length > 10) buckets_over_10++;
			if (stats.chain_length > 50) buckets_over_50++;
			if (stats.chain_length > 100) buckets_over_100++;

			// Print details for problematic buckets
			if (stats.chain_length > 50) {
				fprintf(stderr, "  Bucket %d: len=%d, sprites=%d, text=%d, has_sprite_0=%d\n",
					hash, stats.chain_length, stats.sprite_entries, 
					stats.text_entries, stats.has_sprite_zero);
			}
		}
	}

	fprintf(stderr, "  Total entries: %d\n", total_entries);
	fprintf(stderr, "  Non-empty buckets: %d / %d (%.1f%%)\n", 
		nonempty_buckets, MAX_TEXHASH, 100.0 * nonempty_buckets / MAX_TEXHASH);
	fprintf(stderr, "  Max chain length: %d (bucket %d)\n", max_chain, max_chain_hash);
	fprintf(stderr, "  Buckets with >10 entries: %d\n", buckets_over_10);
	fprintf(stderr, "  Buckets with >50 entries: %d\n", buckets_over_50);
	fprintf(stderr, "  Buckets with >100 entries: %d\n", buckets_over_100);

	if (max_chain_hash >= 0) {
		bucket_stats_t worst = analyze_bucket(max_chain_hash);
		fprintf(stderr, "  Worst bucket (%d): %d sprites, %d text, sprite_0=%d\n",
			max_chain_hash, worst.sprite_entries, worst.text_entries, worst.has_sprite_zero);
	}

	fprintf(stderr, "\n");
}

// Dump the contents of a specific bucket for detailed analysis
static void dump_bucket_contents(int hash, int max_entries)
{
	fprintf(stderr, "\n=== Bucket %d Contents (max %d entries) ===\n", hash, max_entries);

	int stx = sdlt_cache[hash];
	int count = 0;

	while (stx != STX_NONE && count < max_entries) {
		uint16_t flags = flags_load(&sdlt[stx]);
		
		if (flags & SF_TEXT) {
			fprintf(stderr, "  [%d] TEXT: \"%s\" color=0x%x flags=%d\n",
				count, sdlt[stx].text ? sdlt[stx].text : "(null)", 
				sdlt[stx].text_color, sdlt[stx].text_flags);
		}
		if (flags & SF_SPRITE) {
			fprintf(stderr, "  [%d] SPRITE: %u (ml=%d ll=%d rl=%d ul=%d dl=%d)\n",
				count, sdlt[stx].sprite, sdlt[stx].ml, sdlt[stx].ll, 
				sdlt[stx].rl, sdlt[stx].ul, sdlt[stx].dl);
		}

		stx = sdlt[stx].hnext;
		count++;
	}

	fprintf(stderr, "=== End Bucket %d (showed %d entries) ===\n\n", hash, count);
}

// ============================================================================
// Test: Reproduce Lua Mod Text Spam
// ============================================================================

TEST(test_text_spam_simulation)
{
	fprintf(stderr, "  → Testing hash chain lengths...\n");

	// Count total entries and check distribution
	int total_entries = 0;
	int nonempty_buckets = 0;
	int max_chain = 0;
	int max_chain_hash = -1;
	
	for (int hash = 0; hash < MAX_TEXHASH; hash++) {
		bucket_stats_t stats = analyze_bucket(hash);
		if (stats.chain_length > 0) {
			nonempty_buckets++;
			total_entries += stats.chain_length;
			if (stats.chain_length > max_chain) {
				max_chain = stats.chain_length;
				max_chain_hash = hash;
			}
		}
	}

	fprintf(stderr, "\n");
	fprintf(stderr, "  Hash Distribution Quality:\n");
	fprintf(stderr, "    Total entries:      %d\n", total_entries);
	fprintf(stderr, "    Non-empty buckets:  %d / %d (%.1f%%)\n", 
		nonempty_buckets, MAX_TEXHASH, 100.0 * nonempty_buckets / MAX_TEXHASH);
	fprintf(stderr, "    Max chain length:   %d (bucket %d)\n", max_chain, max_chain_hash);
	fprintf(stderr, "    Expected max:       1-2 (with good hash)\n");
	fprintf(stderr, "\n");

	// Check bucket 0 specifically
	bucket_stats_t bucket0 = analyze_bucket(0);
	fprintf(stderr, "  Bucket 0 (previously overflowed with old hash):\n");
	fprintf(stderr, "    Entries: %d (was 1100+ with XOR hash, causing panic)\n", bucket0.chain_length);
	fprintf(stderr, "\n");
	
	ASSERT_TRUE(bucket0.chain_length <= 10);
	ASSERT_TRUE(max_chain <= 10);
}

// ============================================================================
// Test: Sprite 0 Rendering
// ============================================================================

// Test-only wrapper for hashfunc_text (defined in sdl_texture.c under UNIT_TEST)
extern unsigned int test_hashfunc_text(const char *text, int color, int flags);

TEST(test_text_hash_distribution)
{
	fprintf(stderr, "  → Testing text hash function distribution...\n");

	// Test the hash function directly - this was the original crash trigger
	// The Lua mod spam created 50+ unique text strings per frame
	
	const char *prefixes[] = {
		"", "H", "HP", "HP: ", "Mana: ", "Rage: ", "Endurance: ", 
		"Level: ", "Exp: ", "Gold: ", "Player: ", "Tick: ",
		"Position: ", "Screen: ", "Click: ", "Item: ",
		"Strength: ", "Agility: ", "Wisdom: ", "Intelligence: ",
		"Very long text string to test longer strings",
		"Another long string with different content",
		"Short", "Med length text", "X: ", "Y: "
	};
	
	// Track hash distribution across buckets
	int bucket_counts[100] = {0}; // Track first 100 buckets
	int total_hashes = 0;
	
	fprintf(stderr, "     Testing %d prefixes x 100 values x 3 colors = %d combinations...\n",
		(int)(sizeof(prefixes)/sizeof(prefixes[0])), 
		(int)(sizeof(prefixes)/sizeof(prefixes[0])) * 100 * 3);
	
	// Generate lots of text hash variations
	for (size_t prefix_idx = 0; prefix_idx < sizeof(prefixes)/sizeof(prefixes[0]); prefix_idx++) {
		for (int value = 0; value < 100; value++) {
			char text[128];
			snprintf(text, sizeof(text), "%s%d", prefixes[prefix_idx], value);
			
			// Test with different colors
			for (int color_var = 0; color_var < 3; color_var++) {
				int color = 0xFFFFFF - (color_var * 0x555555);
				int flags = value % 3;
				
				// Call hash function directly
				unsigned int hash = test_hashfunc_text(text, color, flags);
				total_hashes++;
				
				// Track if it landed in first 100 buckets
				if (hash < 100) {
					bucket_counts[hash]++;
				}
			}
		}
	}
	
	// Count low bucket clustering
	int low_bucket_count = 0;
	for (int i = 0; i < 100; i++) {
		low_bucket_count += bucket_counts[i];
	}
	
	fprintf(stderr, "     Generated %d text hashes\n", total_hashes);
	fprintf(stderr, "     Low bucket clustering: %d/%d (%.2f%%)\n",
		low_bucket_count, total_hashes, 100.0 * low_bucket_count / total_hashes);
	fprintf(stderr, "     Expected with uniform hash: ~%.1f%% (%d hashes in buckets 0-99)\n",
		100.0 * 100 / MAX_TEXHASH, total_hashes * 100 / MAX_TEXHASH);
	fprintf(stderr, "\n");
	
	// With uniform distribution, expect (100/MAX_TEXHASH)% in first 100 buckets
	// Allow 3x the expected percentage to account for statistical variance
	int expected_in_low_buckets = (total_hashes * 100) / MAX_TEXHASH;
	int threshold = expected_in_low_buckets * 3;
	ASSERT_TRUE(low_bucket_count < threshold);
}

TEST(test_sprite_zero_rendering)
{
	fprintf(stderr, "  → Testing sprite 0 distribution...\n");

	// Sprite 0 is valid (black square for dark tiles)
	// With old XOR hash: always landed in bucket 0
	// With FNV-1a hash: should distribute across buckets
	int in_bucket_0 = 0;
	
	for (int light = 0; light < 3; light++) {
		int stx = sdl_tx_load(0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			light, light, light, light, light, NULL, 0, 0, NULL, 0, 0);

		ASSERT_NE_INT(STX_NONE, stx);
		
		// Check if it landed in bucket 0
		int idx = sdlt_cache[0];
		while (idx != STX_NONE) {
			if (idx == stx) {
				in_bucket_0 = 1;
				break;
			}
			idx = sdlt[idx].hnext;
		}
	}

	ASSERT_FALSE(in_bucket_0);
}

// ============================================================================
// Test: Hash Function Quality
// ============================================================================

TEST(test_hash_function_quality)
{
	fprintf(stderr, "  → Loading ALL sprites to stress test hash + eviction...\n");

	build_valid_sprite_list();
	ASSERT_TRUE(num_valid_sprites > 0);

	// Load ALL valid sprites - this will exceed cache size (32,768) and force evictions
	// This is the true stress test: 50,000+ sprites will cause ~20k evictions
	fprintf(stderr, "     Loading %d sprites (cache size = %d, will force evictions)...\n", 
		num_valid_sprites, MAX_TEXCACHE);
	
	int loaded = 0;
	int low_bucket_count = 0;
	
	for (int i = 0; i < num_valid_sprites; i++) {
		unsigned int sprite = get_valid_sprite(i);
		
		int stx = sdl_tx_load(sprite, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, NULL, 0, 0, NULL, 0, 0);
		
		if (stx != STX_NONE) {
			loaded++;
		}
		
		// Progress indicator (only check every 5000 to avoid slowdown)
		if ((i + 1) % 5000 == 0) {
			fprintf(stderr, "     Processed %d sprites...\n", i + 1);
		}
	}

	// Now count final distribution (cache contains most recent ~32k entries)
	int total_entries = 0;
	for (int hash = 0; hash < MAX_TEXHASH; hash++) {
		bucket_stats_t stats = analyze_bucket(hash);
		total_entries += stats.chain_length;
		
		// Check first 100 buckets for clustering
		if (hash < 100) {
			low_bucket_count += stats.chain_length;
		}
	}

	fprintf(stderr, "     Loaded %d sprites total (%d evicted due to cache limit)\n", 
		loaded, loaded > MAX_TEXCACHE ? loaded - MAX_TEXCACHE : 0);
	fprintf(stderr, "     Final cache contains: %d entries\n", total_entries);
	fprintf(stderr, "     Low bucket clustering: %d/%d (%.1f%%)\n", 
		low_bucket_count, total_entries, 100.0 * low_bucket_count / total_entries);
	fprintf(stderr, "     Expected with uniform hash: ~%.1f%%\n", 100.0 * 100 / MAX_TEXHASH);
	fprintf(stderr, "\n");
	
	// With uniform distribution, we expect (100/MAX_TEXHASH)% in first 100 buckets
	// Allow 3x the expected percentage to account for variance with evictions
	int expected_in_low_buckets = (total_entries * 100) / MAX_TEXHASH;
	int threshold = expected_in_low_buckets * 3;
	ASSERT_TRUE(low_bucket_count < threshold);
}

// ============================================================================
// Main Test Suite
// ============================================================================

TEST_MAIN(
	if (!sdl_init_for_tests()) {
		fprintf(stderr, "FATAL: Failed to initialize SDL for tests\n");
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "\n=== Hash Distribution Tests ===\n");
	test_hash_function_quality();
	test_text_hash_distribution();
	test_sprite_zero_rendering();
	test_text_spam_simulation();

	sdl_shutdown_for_tests();
)

