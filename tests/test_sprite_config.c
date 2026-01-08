/*
 * Test suite for sprite_config system
 *
 * Verifies sprite variant lookups, metadata queries, and coverage thresholds.
 *
 * Build: make test_sprite_config
 * Run: ./bin/test_sprite_config
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "game/sprite_config.h"

/* ========== Stub implementations for standalone testing ========== */
#include <stdint.h>

/* Memory allocation wrappers - match astonia.h signature */
void *xmalloc(size_t size, uint8_t ID)
{
    (void)ID;  /* ID unused in tests */
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "FATAL: xmalloc failed for %zu bytes\n", size);
        exit(1);
    }
    return ptr;
}

void xfree(void *ptr)
{
    free(ptr);
}

/* Logging stubs - match astonia.h signatures (return int) */
int note(const char *format, ...)
{
    (void)format;
    /* Silent in tests */
    return 0;
}

int fail(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "FAIL: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
    return 0;  /* Never reached */
}

/* Global variables for animation calculations - match client.h */
uint16_t originx = 0;
uint16_t originy = 0;

/* Random number stub - match astonia.h */
int rrand(int range)
{
    return range > 0 ? (rand() % range) : 0;
}

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    fflush(stdout); \
    test_##name(); \
    printf("PASSED\n"); \
    tests_passed++; \
    tests_run++; \
} while(0)

#define ASSERT_EQ(expected, actual, msg) do { \
    if ((expected) != (actual)) { \
        printf("FAILED\n    %s: expected %d, got %d\n", msg, (int)(expected), (int)(actual)); \
        tests_failed++; \
        tests_run++; \
        return; \
    } \
} while(0)

#define ASSERT_NEQ(not_expected, actual, msg) do { \
    if ((not_expected) == (actual)) { \
        printf("FAILED\n    %s: should not be %d\n", msg, (int)(not_expected)); \
        tests_failed++; \
        tests_run++; \
        return; \
    } \
} while(0)

#define ASSERT_TRUE(condition, msg) do { \
    if (!(condition)) { \
        printf("FAILED\n    %s\n", msg); \
        tests_failed++; \
        tests_run++; \
        return; \
    } \
} while(0)

/* ========== is_cut_sprite tests ========== */

TEST(is_cut_sprite_non_cut_returns_sprite_id)
{
    /* Sprites not in the metadata should return their own ID */
    unsigned int sprite = 12345;
    int result = sprite_config_is_cut_sprite(sprite);
    ASSERT_EQ((int)sprite, result, "Non-cut sprite should return sprite ID");
}

TEST(is_cut_sprite_with_offset)
{
    /* Sprite 11104 has cut_offset: 4, so result should be 11104 + 4 = 11108 */
    int result = sprite_config_is_cut_sprite(11104);
    ASSERT_EQ(11108, result, "Sprite 11104 should return 11108 (offset +4)");
}

TEST(is_cut_sprite_specific_id)
{
    /* Sprite 11176 has cut_sprite: 17006 */
    int result = sprite_config_is_cut_sprite(11176);
    ASSERT_EQ(17006, result, "Sprite 11176 should return 17006");
}

TEST(is_cut_sprite_explicit_zero_returns_sprite_id)
{
    /* Sprites with cut_sprite: 0 should return their own ID (not 0) */
    /* Sprite 14068 has cut_sprite: 0 in the JSON */
    int result = sprite_config_is_cut_sprite(14068);
    ASSERT_EQ(14068, result, "Sprite 14068 with cut_sprite:0 should return sprite ID");
}

TEST(is_cut_sprite_negative)
{
    /* Sprites with cut_negative: true should return negative result */
    /* Check if we have any negative cut sprites in the metadata */
    const SpriteMetadata *m = sprite_config_lookup_metadata(20360);
    if (m && m->cut_negative) {
        int result = sprite_config_is_cut_sprite(20360);
        ASSERT_TRUE(result < 0, "Sprite with cut_negative should return negative");
    }
}

/* ========== is_door_sprite tests ========== */

TEST(is_door_sprite_returns_true)
{
    /* Find a door sprite in the metadata */
    /* Check sprite 50010 which should be a door */
    const SpriteMetadata *m = sprite_config_lookup_metadata(50010);
    if (m && m->door) {
        int result = sprite_config_is_door_sprite(50010);
        ASSERT_EQ(1, result, "Door sprite should return 1");
    }
}

TEST(is_door_sprite_returns_false)
{
    /* Non-door sprite should return 0 */
    int result = sprite_config_is_door_sprite(12345);
    ASSERT_EQ(0, result, "Non-door sprite should return 0");
}

/* ========== is_mov_sprite tests ========== */

TEST(is_mov_sprite_returns_default)
{
    /* Sprites not in metadata should return itemhint */
    int result = sprite_config_is_mov_sprite(12345, -7);
    ASSERT_EQ(-7, result, "Non-mov sprite should return itemhint");
}

TEST(is_mov_sprite_override)
{
    /* Find a mov sprite in the metadata and test it */
    /* Sprite 50001 should have mov: -5 */
    const SpriteMetadata *m = sprite_config_lookup_metadata(50001);
    if (m && m->mov != 0) {
        int result = sprite_config_is_mov_sprite(50001, -7);
        ASSERT_EQ(m->mov, result, "Mov sprite should return its mov value");
    }
}

/* ========== is_yadd_sprite tests ========== */

TEST(is_yadd_sprite_returns_zero)
{
    /* Non-yadd sprites should return 0 */
    int result = sprite_config_is_yadd_sprite(12345);
    ASSERT_EQ(0, result, "Non-yadd sprite should return 0");
}

TEST(is_yadd_sprite_returns_value)
{
    /* Sprite 13103 has yadd: 29 */
    int result = sprite_config_is_yadd_sprite(13103);
    ASSERT_EQ(29, result, "Sprite 13103 should return yadd 29");
}

/* ========== get_lay_sprite tests ========== */

TEST(get_lay_sprite_returns_default)
{
    /* Non-layer sprites should return the default lay parameter */
    int result = sprite_config_get_lay_sprite(12345, 50);
    ASSERT_EQ(50, result, "Non-layer sprite should return default");
}

TEST(get_lay_sprite_gme_lay)
{
    /* Sprite 14004 has layer: 110 (GME_LAY) */
    int result = sprite_config_get_lay_sprite(14004, 50);
    ASSERT_EQ(110, result, "Sprite 14004 should return GME_LAY (110)");
}

TEST(get_lay_sprite_gnd_lay)
{
    /* Sprite 14363 has layer: 100 (GND_LAY) */
    int result = sprite_config_get_lay_sprite(14363, 50);
    ASSERT_EQ(100, result, "Sprite 14363 should return GND_LAY (100)");
}

/* ========== get_offset_sprite tests ========== */

TEST(get_offset_sprite_no_offset)
{
    /* Non-offset sprites should return 0 and not modify px/py */
    int px = -1, py = -1;
    int result = sprite_config_get_offset_sprite(12345, &px, &py);
    ASSERT_EQ(0, result, "Non-offset sprite should return 0");
}

TEST(get_offset_sprite_with_offset)
{
    /* Sprite 16035 has offset_x: 6, offset_y: 8 */
    int px = 0, py = 0;
    int result = sprite_config_get_offset_sprite(16035, &px, &py);
    ASSERT_EQ(1, result, "Offset sprite should return 1");
    ASSERT_EQ(6, px, "Sprite 16035 should have offset_x 6");
    ASSERT_EQ(8, py, "Sprite 16035 should have offset_y 8");
}

/* ========== no_lighting_sprite tests ========== */

TEST(no_lighting_sprite_returns_false)
{
    /* Normal sprites should return 0 */
    int result = sprite_config_no_lighting_sprite(12345);
    ASSERT_EQ(0, result, "Normal sprite should return 0");
}

TEST(no_lighting_sprite_returns_true)
{
    /* Sprite 21410 has no_lighting: true */
    int result = sprite_config_no_lighting_sprite(21410);
    ASSERT_EQ(1, result, "Sprite 21410 should return 1 (no lighting)");
}

/* ========== Character variant tests ========== */

TEST(character_variant_lookup_exists)
{
    /* Sprite 121 should be in character variants (skelly) */
    const CharacterVariant *v = sprite_config_lookup_character(121);
    ASSERT_TRUE(v != NULL, "Sprite 121 should have character variant");
    ASSERT_EQ(8, v->base_sprite, "Sprite 121 should map to base sprite 8");
}

TEST(character_variant_lookup_not_exists)
{
    /* Non-variant sprite should return NULL */
    const CharacterVariant *v = sprite_config_lookup_character(1);
    ASSERT_TRUE(v == NULL, "Sprite 1 should not have character variant");
}

TEST(character_variant_apply)
{
    /* Test applying a character variant */
    const CharacterVariant *v = sprite_config_lookup_character(121);
    if (v) {
        int scale, cr, cg, cb, light, sat, c1, c2, c3, shine;
        int result = sprite_config_apply_character(v, 121, &scale, &cr, &cg, &cb,
            &light, &sat, &c1, &c2, &c3, &shine, 0);
        ASSERT_EQ(8, result, "Applied variant should return base sprite");
        ASSERT_TRUE(scale > 0, "Scale should be set");
    }
}

/* ========== Animated variant tests ========== */

TEST(animated_variant_lookup_exists)
{
    /* Check for an animated variant */
    const AnimatedVariant *v = sprite_config_lookup_animated(14136);
    if (v) {
        ASSERT_TRUE(v->base_sprite > 0, "Animated variant should have base sprite");
    }
}

TEST(animated_variant_lookup_not_exists)
{
    /* Non-variant sprite should return NULL */
    const AnimatedVariant *v = sprite_config_lookup_animated(1);
    ASSERT_TRUE(v == NULL, "Sprite 1 should not have animated variant");
}

/* ========== Stats test ========== */

TEST(config_stats)
{
    size_t char_count, anim_count;
    sprite_config_get_stats(&char_count, &anim_count);

    printf("\n    Stats: %zu char variants, %zu anim variants\n    ", char_count, anim_count);

    ASSERT_TRUE(char_count > 0, "Should have character variants loaded");
    ASSERT_TRUE(anim_count > 0, "Should have animated variants loaded");
}

/* ========== Metadata lookup test ========== */

TEST(metadata_lookup)
{
    /* Test metadata lookup for a known sprite */
    const SpriteMetadata *m = sprite_config_lookup_metadata(11104);
    ASSERT_TRUE(m != NULL, "Sprite 11104 should have metadata");
    ASSERT_EQ(11104, (int)m->id, "Metadata ID should match");
    ASSERT_EQ(4, m->cut_result, "Sprite 11104 should have cut_result 4");
    ASSERT_EQ(1, m->cut_offset, "Sprite 11104 should have cut_offset flag");
}

/* ========== Coverage tests - verify minimum entry counts ========== */

/* Minimum thresholds to detect data loss or loading failures */
#define MIN_CHARACTER_VARIANTS  300
#define MIN_ANIMATED_VARIANTS   1000
#define MIN_CUT_SPRITES         500
#define MIN_DOOR_SPRITES        40
#define MIN_MOV_SPRITES         40
#define MIN_YADD_SPRITES        50
#define MIN_LAYER_SPRITES       30
#define MIN_OFFSET_SPRITES      15
#define MIN_NO_LIGHTING_SPRITES 35

TEST(coverage_character_variants)
{
    size_t char_count, anim_count;
    sprite_config_get_stats(&char_count, &anim_count);

    printf("\n    Character variants: %zu (min: %d)\n    ", char_count, MIN_CHARACTER_VARIANTS);
    ASSERT_TRUE(char_count >= MIN_CHARACTER_VARIANTS,
        "Character variant count below minimum - possible data loss");
}

TEST(coverage_animated_variants)
{
    size_t char_count, anim_count;
    sprite_config_get_stats(&char_count, &anim_count);

    printf("\n    Animated variants: %zu (min: %d)\n    ", anim_count, MIN_ANIMATED_VARIANTS);
    ASSERT_TRUE(anim_count >= MIN_ANIMATED_VARIANTS,
        "Animated variant count below minimum - possible data loss");
}

TEST(coverage_cut_sprites)
{
    /* Count cut sprites by scanning known ranges (actual: 11104-60041, 552 entries) */
    int count = 0;
    for (unsigned int i = 11000; i <= 60100; i++) {
        int result = sprite_config_is_cut_sprite(i);
        if (result != (int)i) {  /* Has cut behavior */
            count++;
        }
    }

    printf("\n    Cut sprites found: %d (min: %d)\n    ", count, MIN_CUT_SPRITES);
    ASSERT_TRUE(count >= MIN_CUT_SPRITES,
        "Cut sprite count below minimum - possible data loss");
}

TEST(coverage_door_sprites)
{
    int count = 0;
    /* Scan door sprite range (actual: 20039-20702, 44 entries) */
    for (unsigned int i = 20000; i <= 21000; i++) {
        if (sprite_config_is_door_sprite(i)) {
            count++;
        }
    }

    printf("\n    Door sprites found: %d (min: %d)\n    ", count, MIN_DOOR_SPRITES);
    ASSERT_TRUE(count >= MIN_DOOR_SPRITES,
        "Door sprite count below minimum - possible data loss");
}

TEST(coverage_mov_sprites)
{
    int count = 0;
    /* Scan mov sprite range (actual: 20039-20702, 44 entries) */
    for (unsigned int i = 20000; i <= 21000; i++) {
        int result = sprite_config_is_mov_sprite(i, -999);  /* Use sentinel to detect override */
        if (result != -999) {
            count++;
        }
    }

    printf("\n    Mov sprites found: %d (min: %d)\n    ", count, MIN_MOV_SPRITES);
    ASSERT_TRUE(count >= MIN_MOV_SPRITES,
        "Mov sprite count below minimum - possible data loss");
}

TEST(coverage_yadd_sprites)
{
    int count = 0;
    /* Scan yadd sprite range (actual: 13103-50286, 59 entries) */
    for (unsigned int i = 13000; i <= 51000; i++) {
        if (sprite_config_is_yadd_sprite(i) != 0) {
            count++;
        }
    }

    printf("\n    Yadd sprites found: %d (min: %d)\n    ", count, MIN_YADD_SPRITES);
    ASSERT_TRUE(count >= MIN_YADD_SPRITES,
        "Yadd sprite count below minimum - possible data loss");
}

TEST(coverage_layer_sprites)
{
    int count = 0;
    /* Scan layer sprite range (actual: 14004-60022, 33 entries) */
    for (int i = 14000; i <= 60100; i++) {
        int result = sprite_config_get_lay_sprite(i, -999);  /* Use sentinel */
        if (result != -999) {
            count++;
        }
    }

    printf("\n    Layer sprites found: %d (min: %d)\n    ", count, MIN_LAYER_SPRITES);
    ASSERT_TRUE(count >= MIN_LAYER_SPRITES,
        "Layer sprite count below minimum - possible data loss");
}

TEST(coverage_offset_sprites)
{
    int count = 0;
    int px, py;
    /* Scan offset sprite range (actual: 16035-21688, 20 entries) */
    for (int i = 16000; i <= 22000; i++) {
        if (sprite_config_get_offset_sprite(i, &px, &py)) {
            count++;
        }
    }

    printf("\n    Offset sprites found: %d (min: %d)\n    ", count, MIN_OFFSET_SPRITES);
    ASSERT_TRUE(count >= MIN_OFFSET_SPRITES,
        "Offset sprite count below minimum - possible data loss");
}

TEST(coverage_no_lighting_sprites)
{
    int count = 0;
    /* Scan no-lighting sprite range (actual: 21410-26039, 40 entries) */
    for (unsigned int i = 21000; i <= 27000; i++) {
        if (sprite_config_no_lighting_sprite(i)) {
            count++;
        }
    }

    printf("\n    No-lighting sprites found: %d (min: %d)\n    ", count, MIN_NO_LIGHTING_SPRITES);
    ASSERT_TRUE(count >= MIN_NO_LIGHTING_SPRITES,
        "No-lighting sprite count below minimum - possible data loss");
}

/* ========== Main test runner ========== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== Sprite Config Test Suite ===\n\n");

    /* Initialize the sprite config system */
    printf("Initializing sprite config...\n");
    if (sprite_config_init() < 0) {
        printf("FATAL: Failed to initialize sprite config\n");
        return 1;
    }
    printf("Initialization complete.\n\n");

    /* Run tests */
    printf("Running tests:\n\n");

    printf("[is_cut_sprite]\n");
    RUN_TEST(is_cut_sprite_non_cut_returns_sprite_id);
    RUN_TEST(is_cut_sprite_with_offset);
    RUN_TEST(is_cut_sprite_specific_id);
    RUN_TEST(is_cut_sprite_explicit_zero_returns_sprite_id);
    RUN_TEST(is_cut_sprite_negative);
    printf("\n");

    printf("[is_door_sprite]\n");
    RUN_TEST(is_door_sprite_returns_true);
    RUN_TEST(is_door_sprite_returns_false);
    printf("\n");

    printf("[is_mov_sprite]\n");
    RUN_TEST(is_mov_sprite_returns_default);
    RUN_TEST(is_mov_sprite_override);
    printf("\n");

    printf("[is_yadd_sprite]\n");
    RUN_TEST(is_yadd_sprite_returns_zero);
    RUN_TEST(is_yadd_sprite_returns_value);
    printf("\n");

    printf("[get_lay_sprite]\n");
    RUN_TEST(get_lay_sprite_returns_default);
    RUN_TEST(get_lay_sprite_gme_lay);
    RUN_TEST(get_lay_sprite_gnd_lay);
    printf("\n");

    printf("[get_offset_sprite]\n");
    RUN_TEST(get_offset_sprite_no_offset);
    RUN_TEST(get_offset_sprite_with_offset);
    printf("\n");

    printf("[no_lighting_sprite]\n");
    RUN_TEST(no_lighting_sprite_returns_false);
    RUN_TEST(no_lighting_sprite_returns_true);
    printf("\n");

    printf("[character_variants]\n");
    RUN_TEST(character_variant_lookup_exists);
    RUN_TEST(character_variant_lookup_not_exists);
    RUN_TEST(character_variant_apply);
    printf("\n");

    printf("[animated_variants]\n");
    RUN_TEST(animated_variant_lookup_exists);
    RUN_TEST(animated_variant_lookup_not_exists);
    printf("\n");

    printf("[metadata]\n");
    RUN_TEST(metadata_lookup);
    printf("\n");

    printf("[stats]\n");
    RUN_TEST(config_stats);
    printf("\n");

    printf("[coverage - minimum entry counts]\n");
    RUN_TEST(coverage_character_variants);
    RUN_TEST(coverage_animated_variants);
    RUN_TEST(coverage_cut_sprites);
    RUN_TEST(coverage_door_sprites);
    RUN_TEST(coverage_mov_sprites);
    RUN_TEST(coverage_yadd_sprites);
    RUN_TEST(coverage_layer_sprites);
    RUN_TEST(coverage_offset_sprites);
    RUN_TEST(coverage_no_lighting_sprites);
    printf("\n");

    /* Cleanup */
    sprite_config_shutdown();

    /* Summary */
    printf("=== Test Summary ===\n");
    printf("Total:  %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
