/*
 * Sprite Variant Configuration System
 *
 * Loads sprite variant definitions from JSON configuration files.
 * Enables per-server customization without client recompilation.
 */

#ifndef SPRITE_CONFIG_H
#define SPRITE_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#include "astonia.h"

/* Animation type constants */
typedef enum {
	ANIM_NONE = 0, /* Simple replacement (just apply colors/scale) */
	ANIM_CYCLE, /* base + (tick/divisor) % frames */
	ANIM_POSITION_CYCLE, /* base + ((mn + tick/divisor) % frames) - desync across map */
	ANIM_BIDIRECTIONAL, /* Bounce 0->n->0 (ping-pong) */
	ANIM_FLICKER, /* Random variation using rrand() */
	ANIM_PULSE, /* abs() based color pulsing */
	ANIM_MULTI_BRANCH, /* Conditional branches based on modulo */
	ANIM_RANDOM_OFFSET, /* rrand() added to position calculation */
} AnimationType;

/* Dynamic effect type for character variants */
typedef enum {
	DYNAMIC_NONE = 0,
	DYNAMIC_PULSE_CR, /* Pulse red channel */
	DYNAMIC_PULSE_CG, /* Pulse green channel */
	DYNAMIC_PULSE_CB, /* Pulse blue channel */
} DynamicType;

/* Branch condition for multi-branch animations */
typedef struct {
	int modulo; /* Divisor for modulo operation (e.g., 17 for mod17) */
	int threshold; /* Threshold for comparison */
	int frames; /* Number of animation frames */
	int divisor; /* Tick divisor for animation speed */
} AnimBranch;

#define MAX_ANIM_BRANCHES 4

/* Character variant (trans_charno) - uses int for output params */
typedef struct {
	int32_t id; /* Variant sprite ID (key) */
	int32_t base_sprite; /* Target base sprite */
	int16_t scale; /* Scale percentage (100 = normal) */
	int16_t cr, cg, cb; /* Color balance adjustments */
	int16_t light; /* Light adjustment */
	int16_t sat; /* Saturation adjustment */
	int16_t c1, c2, c3; /* Color replacement values (RGB555) */
	int16_t shine; /* Shine effect value */

	/* Dynamic effects (e.g., pulsing fire demon) */
	uint8_t dynamic_type; /* DynamicType enum */
	uint8_t pulse_period; /* Period of pulse animation (power of 2) */
	int16_t pulse_base; /* Base value for pulsing color */
	int16_t pulse_amplitude; /* Amplitude of pulse */
} CharacterVariant;

/* Animated sprite variant (trans_asprite) - uses unsigned char/short for output params */
typedef struct {
	uint32_t id; /* Variant sprite ID (key) */
	uint32_t base_sprite; /* Target base sprite */
	uint8_t scale; /* Scale percentage (100 = normal) */
	int8_t cr, cg, cb; /* Color balance adjustments */
	int8_t light; /* Light adjustment */
	int8_t sat; /* Saturation adjustment */
	uint16_t c1, c2, c3; /* Color replacement values (RGB555) */
	uint16_t shine; /* Shine effect value */

	/* Animation settings */
	uint8_t animation_type; /* AnimationType enum */
	uint8_t frames; /* Number of animation frames */
	uint8_t divisor; /* Tick divisor for animation speed */
	uint8_t random_range; /* Range for random offset (ANIM_RANDOM_OFFSET) */

	/* For multi-branch animations */
	uint8_t branch_count;
	AnimBranch branches[MAX_ANIM_BRANCHES];

	/* For pulsing color effects (e.g., teleporter glow) */
	uint8_t color_pulse_target; /* 0=none, 1=c1, 2=c2, 3=c3 */
	uint8_t color_pulse_r; /* Fixed red component */
	uint8_t color_pulse_g; /* Fixed green component */
	uint8_t color_pulse_max; /* Max value for pulsing component */
	uint8_t color_pulse_period; /* Period of pulse (attick % period) */
	uint8_t color_pulse_divisor; /* Divisor for pulse value (default 1) */
	uint8_t color_pulse_offset; /* Offset added to pulse value */

	/* For pulsing light effects */
	uint8_t light_pulse_max; /* Max value for light pulse */
	uint8_t light_pulse_period; /* Period of light pulse */
	uint8_t light_pulse_divisor; /* Divisor for light pulse value */
	uint8_t light_pulse_offset; /* Offset added to light pulse value */
} AnimatedVariant;

/* Hash table for O(1) lookups */
typedef struct {
	CharacterVariant *entries;
	size_t capacity;
	size_t count;
} CharacterVariantTable;

typedef struct {
	AnimatedVariant *entries;
	size_t capacity;
	size_t count;
} AnimatedVariantTable;

/*
 * Sprite metadata for is_cut_sprite, is_door_sprite, etc.
 *
 * Cut sprite patterns:
 *   - cut_result > 0, !cut_offset: specific cut sprite ID
 *   - cut_result > 0, cut_offset: cut sprite = id + cut_result
 *   - cut_negative: return value is negated
 */
typedef struct {
	uint32_t id; /* Sprite ID (key) */

	/* is_cut_sprite result */
	int32_t cut_result; /* 0 = not cut, >0 = offset or sprite ID */
	uint8_t cut_offset; /* If true, cut_result is offset from sprite ID */
	uint8_t cut_negative; /* If true, negate the return value */

	/* is_door_sprite, is_mov_sprite */
	uint8_t door; /* Is this a door sprite? */
	int8_t mov; /* is_mov_sprite result (0 = use default) */

	/* is_yadd_sprite */
	int16_t yadd; /* Y-offset for sprite (0 = none) */

	/* get_lay_sprite */
	int16_t layer; /* Layer value (0 = use default) */

	/* get_offset_sprite */
	int8_t offset_x; /* X pixel offset */
	int8_t offset_y; /* Y pixel offset */

	/* no_lighting_sprite */
	uint8_t no_lighting; /* Disable lighting for this sprite? */

	uint8_t _padding; /* Alignment padding */
} SpriteMetadata;

typedef struct {
	SpriteMetadata *entries;
	size_t capacity;
	size_t count;
} SpriteMetadataTable;

/*
 * Initialize the sprite configuration system.
 * Loads default variants from res/config/ directory.
 * Should be called once at startup.
 *
 * Returns: 0 on success, -1 on error
 */
int sprite_config_init(void);

/*
 * Shutdown the sprite configuration system.
 * Frees all allocated memory.
 */
void sprite_config_shutdown(void);

/*
 * Load character variants from a JSON file.
 * Can be called multiple times to add/override variants.
 *
 * path: Path to JSON file
 * Returns: Number of variants loaded, or -1 on error
 */
DLL_EXPORT int sprite_config_load_characters(const char *path);

/*
 * Load animated variants from a JSON file.
 * Can be called multiple times to add/override variants.
 *
 * path: Path to JSON file
 * Returns: Number of variants loaded, or -1 on error
 */
DLL_EXPORT int sprite_config_load_animated(const char *path);

/*
 * Load variants from a JSON buffer (for future server-sent config).
 *
 * json_data: JSON string data
 * len: Length of JSON data
 * Returns: Number of variants loaded, or -1 on error
 */
int sprite_config_load_from_buffer(const char *json_data, size_t len);

/*
 * Clear all loaded variants (for reload).
 */
void sprite_config_clear(void);

/*
 * Look up a character variant by ID.
 *
 * id: Sprite variant ID to look up
 * Returns: Pointer to variant, or NULL if not found
 */
const CharacterVariant *sprite_config_lookup_character(int id);

/*
 * Look up an animated variant by ID.
 *
 * id: Sprite variant ID to look up
 * Returns: Pointer to variant, or NULL if not found
 */
const AnimatedVariant *sprite_config_lookup_animated(unsigned int id);

/*
 * Apply a character variant to output parameters.
 * Handles dynamic effects like pulsing.
 *
 * v: Pointer to variant (may be NULL for identity)
 * csprite: Original sprite ID
 * attick: Current animation tick
 * pscale, pcr, pcg, pcb, plight, psat, pc1, pc2, pc3, pshine: Output parameters
 *
 * Returns: Base sprite ID to use
 */
int sprite_config_apply_character(const CharacterVariant *v, int csprite, int *pscale, int *pcr, int *pcg, int *pcb,
    int *plight, int *psat, int *pc1, int *pc2, int *pc3, int *pshine, int attick);

/*
 * Apply an animated variant to output parameters.
 * Handles animation frame calculation.
 *
 * v: Pointer to variant (may be NULL for identity)
 * mn: Map index for position-aware animations
 * sprite: Original sprite ID
 * attick: Current animation tick
 * pscale, pcr, pcg, pcb, plight, psat, pc1, pc2, pc3, pshine: Output parameters
 *
 * Returns: Transformed sprite ID
 */
unsigned int sprite_config_apply_animated(const AnimatedVariant *v, map_index_t mn, unsigned int sprite, tick_t attick,
    unsigned char *pscale, unsigned char *pcr, unsigned char *pcg, unsigned char *pcb, unsigned char *plight,
    unsigned char *psat, unsigned short *pc1, unsigned short *pc2, unsigned short *pc3, unsigned short *pshine);

/*
 * Get statistics about loaded variants.
 *
 * char_count: Output for character variant count
 * anim_count: Output for animated variant count
 */
void sprite_config_get_stats(size_t *char_count, size_t *anim_count);

/*
 * Load sprite metadata from a JSON file.
 *
 * path: Path to JSON file
 * Returns: Number of metadata entries loaded, or -1 on error
 */
DLL_EXPORT int sprite_config_load_metadata(const char *path);

/*
 * Look up sprite metadata by ID.
 *
 * id: Sprite ID to look up
 * Returns: Pointer to metadata, or NULL if not found
 */
const SpriteMetadata *sprite_config_lookup_metadata(unsigned int id);

/*
 * Sprite metadata query functions.
 * These use the loaded JSON config data.
 */
int sprite_config_is_cut_sprite(unsigned int sprite);
int sprite_config_is_door_sprite(unsigned int sprite);
int sprite_config_is_mov_sprite(unsigned int sprite, int itemhint);
int sprite_config_is_yadd_sprite(unsigned int sprite);
int sprite_config_get_lay_sprite(int sprite, int lay);
int sprite_config_get_offset_sprite(int sprite, int *px, int *py);
int sprite_config_no_lighting_sprite(unsigned int sprite);

#endif /* SPRITE_CONFIG_H */
