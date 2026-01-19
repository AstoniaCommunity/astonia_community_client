/*
 * Sprite Variant Configuration System - Implementation
 *
 * Loads sprite variant definitions from JSON files and provides
 * O(1) lookups via hash tables.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sprite_config.h"
#include "lib/cjson/cJSON.h"
#include "game.h"
#include "client/client.h"

/* Hash table sizes (power of 2 for fast modulo) */
#define CHAR_TABLE_SIZE 512 /* For ~320 character variants */
#define ANIM_TABLE_SIZE 2048 /* For ~1092 animated variants */
#define META_TABLE_SIZE 2048 /* For ~900 metadata entries */

/* Hash macros using bitwise AND for power-of-2 sizes */
#define CHAR_HASH(id) ((unsigned int)(id) & (CHAR_TABLE_SIZE - 1))
#define ANIM_HASH(id) ((id) & (ANIM_TABLE_SIZE - 1))
#define META_HASH(id) ((id) & (META_TABLE_SIZE - 1))

/* Empty slot marker (sprite ID 0 is never used as a variant) */
#define EMPTY_SLOT 0

/* Global hash tables */
static CharacterVariantTable char_table = {NULL, 0, 0};
static AnimatedVariantTable anim_table = {NULL, 0, 0};
static SpriteMetadataTable meta_table = {NULL, 0, 0};

/*
 * Initialize hash tables with empty slots.
 */
static int init_tables(void)
{
	if (char_table.entries == NULL) {
		char_table.entries = xmalloc(CHAR_TABLE_SIZE * sizeof(CharacterVariant), MEM_GAME);
		if (!char_table.entries) {
			fail("sprite_config: Failed to allocate character variant table");
			return -1;
		}
		memset(char_table.entries, 0, CHAR_TABLE_SIZE * sizeof(CharacterVariant));
		char_table.capacity = CHAR_TABLE_SIZE;
		char_table.count = 0;
	}

	if (anim_table.entries == NULL) {
		anim_table.entries = xmalloc(ANIM_TABLE_SIZE * sizeof(AnimatedVariant), MEM_GAME);
		if (!anim_table.entries) {
			fail("sprite_config: Failed to allocate animated variant table");
			return -1;
		}
		memset(anim_table.entries, 0, ANIM_TABLE_SIZE * sizeof(AnimatedVariant));
		anim_table.capacity = ANIM_TABLE_SIZE;
		anim_table.count = 0;
	}

	return 0;
}

/*
 * Insert a character variant into the hash table.
 * Uses open addressing with linear probing.
 */
static int insert_character(const CharacterVariant *v)
{
	if (char_table.count >= char_table.capacity * 3 / 4) {
		warn("sprite_config: Character variant table is full (>75%% load)");
		return -1;
	}

	unsigned int idx = CHAR_HASH(v->id);
	for (unsigned int i = 0; i < char_table.capacity; i++) {
		unsigned int probe = (idx + i) & (CHAR_TABLE_SIZE - 1);
		if (char_table.entries[probe].id == EMPTY_SLOT || char_table.entries[probe].id == v->id) {
			/* Empty slot or update existing */
			if (char_table.entries[probe].id == EMPTY_SLOT) {
				char_table.count++;
			}
			memcpy(&char_table.entries[probe], v, sizeof(CharacterVariant));
			return 0;
		}
	}

	warn("sprite_config: Failed to insert character variant %d", v->id);
	return -1;
}

/*
 * Insert an animated variant into the hash table.
 */
static int insert_animated(const AnimatedVariant *v)
{
	if (anim_table.count >= anim_table.capacity * 3 / 4) {
		warn("sprite_config: Animated variant table is full (>75%% load)");
		return -1;
	}

	unsigned int idx = ANIM_HASH(v->id);
	for (unsigned int i = 0; i < anim_table.capacity; i++) {
		unsigned int probe = (idx + i) & (ANIM_TABLE_SIZE - 1);
		if (anim_table.entries[probe].id == EMPTY_SLOT || anim_table.entries[probe].id == v->id) {
			if (anim_table.entries[probe].id == EMPTY_SLOT) {
				anim_table.count++;
			}
			memcpy(&anim_table.entries[probe], v, sizeof(AnimatedVariant));
			return 0;
		}
	}

	warn("sprite_config: Failed to insert animated variant %u", v->id);
	return -1;
}

/*
 * Parse a color object from JSON: {"r": 16, "g": 0, "b": 0} -> RGB555
 */
static int parse_color_rgb555(cJSON *obj)
{
	if (!cJSON_IsObject(obj)) {
		return 0;
	}

	cJSON *r = cJSON_GetObjectItem(obj, "r");
	cJSON *g = cJSON_GetObjectItem(obj, "g");
	cJSON *b = cJSON_GetObjectItem(obj, "b");

	int rv = r && cJSON_IsNumber(r) ? r->valueint : 0;
	int gv = g && cJSON_IsNumber(g) ? g->valueint : 0;
	int bv = b && cJSON_IsNumber(b) ? b->valueint : 0;

	/* Clamp to 5-bit range */
	if (rv < 0) {
		rv = 0;
	}
	if (rv > 31) {
		rv = 31;
	}
	if (gv < 0) {
		gv = 0;
	}
	if (gv > 31) {
		gv = 31;
	}
	if (bv < 0) {
		bv = 0;
	}
	if (bv > 31) {
		bv = 31;
	}

	return IRGB(rv, gv, bv);
}

/*
 * Parse animation type string to enum.
 */
static AnimationType parse_animation_type(const char *type_str)
{
	if (!type_str) {
		return ANIM_NONE;
	}

	if (strcmp(type_str, "cycle") == 0 || strcmp(type_str, "simple") == 0) {
		return ANIM_CYCLE;
	} else if (strcmp(type_str, "position_cycle") == 0 || strcmp(type_str, "location_aware") == 0) {
		return ANIM_POSITION_CYCLE;
	} else if (strcmp(type_str, "bidirectional") == 0 || strcmp(type_str, "pingpong") == 0) {
		return ANIM_BIDIRECTIONAL;
	} else if (strcmp(type_str, "flicker") == 0) {
		return ANIM_FLICKER;
	} else if (strcmp(type_str, "pulse") == 0) {
		return ANIM_PULSE;
	} else if (strcmp(type_str, "multi_branch") == 0) {
		return ANIM_MULTI_BRANCH;
	} else if (strcmp(type_str, "random_offset") == 0) {
		return ANIM_RANDOM_OFFSET;
	}

	return ANIM_NONE;
}

/*
 * Parse dynamic type string to enum.
 */
static DynamicType parse_dynamic_type(const char *affects)
{
	if (!affects) {
		return DYNAMIC_NONE;
	}

	if (strcmp(affects, "cr") == 0 || strcmp(affects, "red") == 0) {
		return DYNAMIC_PULSE_CR;
	} else if (strcmp(affects, "cg") == 0 || strcmp(affects, "green") == 0) {
		return DYNAMIC_PULSE_CG;
	} else if (strcmp(affects, "cb") == 0 || strcmp(affects, "blue") == 0) {
		return DYNAMIC_PULSE_CB;
	}

	return DYNAMIC_NONE;
}

/*
 * Parse a single character variant from JSON.
 */
static int parse_character_variant(cJSON *item, CharacterVariant *v)
{
	memset(v, 0, sizeof(CharacterVariant));
	v->scale = 100; /* Default scale */

	cJSON *id = cJSON_GetObjectItem(item, "id");
	if (!id || !cJSON_IsNumber(id)) {
		warn("sprite_config: Character variant missing 'id'");
		return -1;
	}
	v->id = id->valueint;

	cJSON *base = cJSON_GetObjectItem(item, "base_sprite");
	v->base_sprite = base && cJSON_IsNumber(base) ? base->valueint : v->id;

	cJSON *scale = cJSON_GetObjectItem(item, "scale");
	if (scale && cJSON_IsNumber(scale)) {
		v->scale = (int16_t)scale->valueint;
	}

	cJSON *cr = cJSON_GetObjectItem(item, "cr");
	if (cr && cJSON_IsNumber(cr)) {
		v->cr = (int16_t)cr->valueint;
	}

	cJSON *cg = cJSON_GetObjectItem(item, "cg");
	if (cg && cJSON_IsNumber(cg)) {
		v->cg = (int16_t)cg->valueint;
	}

	cJSON *cb = cJSON_GetObjectItem(item, "cb");
	if (cb && cJSON_IsNumber(cb)) {
		v->cb = (int16_t)cb->valueint;
	}

	cJSON *light = cJSON_GetObjectItem(item, "light");
	if (light && cJSON_IsNumber(light)) {
		v->light = (int16_t)light->valueint;
	}

	cJSON *sat = cJSON_GetObjectItem(item, "saturation");
	if (!sat) {
		sat = cJSON_GetObjectItem(item, "sat");
	}
	if (sat && cJSON_IsNumber(sat)) {
		v->sat = (int16_t)sat->valueint;
	}

	cJSON *shine = cJSON_GetObjectItem(item, "shine");
	if (shine && cJSON_IsNumber(shine)) {
		v->shine = (int16_t)shine->valueint;
	}

	/* Color replacements - can be object {r,g,b} or direct integer */
	cJSON *c1 = cJSON_GetObjectItem(item, "c1");
	if (c1) {
		v->c1 = cJSON_IsNumber(c1) ? (int16_t)c1->valueint : (int16_t)parse_color_rgb555(c1);
	}

	cJSON *c2 = cJSON_GetObjectItem(item, "c2");
	if (c2) {
		v->c2 = cJSON_IsNumber(c2) ? (int16_t)c2->valueint : (int16_t)parse_color_rgb555(c2);
	}

	cJSON *c3 = cJSON_GetObjectItem(item, "c3");
	if (c3) {
		v->c3 = cJSON_IsNumber(c3) ? (int16_t)c3->valueint : (int16_t)parse_color_rgb555(c3);
	}

	/* Dynamic/animation effects */
	cJSON *anim = cJSON_GetObjectItem(item, "animation");
	if (anim && cJSON_IsObject(anim)) {
		cJSON *type = cJSON_GetObjectItem(anim, "type");
		if (type && cJSON_IsString(type) && strcmp(type->valuestring, "pulse") == 0) {
			cJSON *affects = cJSON_GetObjectItem(anim, "affects");
			v->dynamic_type =
			    (uint8_t)parse_dynamic_type(affects && cJSON_IsString(affects) ? affects->valuestring : "cr");

			cJSON *period = cJSON_GetObjectItem(anim, "period");
			v->pulse_period = period && cJSON_IsNumber(period) ? (uint8_t)period->valueint : 32;

			cJSON *base_val = cJSON_GetObjectItem(anim, "base");
			v->pulse_base = base_val && cJSON_IsNumber(base_val) ? (int16_t)base_val->valueint : 0;

			cJSON *amp = cJSON_GetObjectItem(anim, "amplitude");
			v->pulse_amplitude = amp && cJSON_IsNumber(amp) ? (int16_t)amp->valueint : 0;
		}
	}

	/* Legacy "dynamic" field support */
	cJSON *dynamic = cJSON_GetObjectItem(item, "dynamic");
	if (dynamic && cJSON_IsObject(dynamic)) {
		cJSON *type = cJSON_GetObjectItem(dynamic, "type");
		if (type && cJSON_IsString(type) && strcmp(type->valuestring, "pulse") == 0) {
			cJSON *period = cJSON_GetObjectItem(dynamic, "period");
			v->pulse_period = period && cJSON_IsNumber(period) ? (uint8_t)period->valueint : 32;

			cJSON *color_red = cJSON_GetObjectItem(dynamic, "color_red");
			if (color_red && cJSON_IsObject(color_red)) {
				v->dynamic_type = DYNAMIC_PULSE_CR;
				cJSON *base_val = cJSON_GetObjectItem(color_red, "base");
				cJSON *amp = cJSON_GetObjectItem(color_red, "amplitude");
				v->pulse_base = base_val && cJSON_IsNumber(base_val) ? (int16_t)base_val->valueint : 0;
				v->pulse_amplitude = amp && cJSON_IsNumber(amp) ? (int16_t)amp->valueint : 0;
			}
		}
	}

	return 0;
}

/*
 * Parse a single animated variant from JSON.
 */
static int parse_animated_variant(cJSON *item, AnimatedVariant *v)
{
	memset(v, 0, sizeof(AnimatedVariant));
	v->scale = 100;

	cJSON *id = cJSON_GetObjectItem(item, "id");
	if (!id || !cJSON_IsNumber(id)) {
		warn("sprite_config: Animated variant missing 'id'");
		return -1;
	}
	v->id = (uint32_t)id->valueint;

	cJSON *base = cJSON_GetObjectItem(item, "base_sprite");
	v->base_sprite = base && cJSON_IsNumber(base) ? (uint32_t)base->valueint : v->id;

	cJSON *scale = cJSON_GetObjectItem(item, "scale");
	if (scale && cJSON_IsNumber(scale)) {
		v->scale = (uint8_t)scale->valueint;
	}

	cJSON *cr = cJSON_GetObjectItem(item, "cr");
	if (cr && cJSON_IsNumber(cr)) {
		v->cr = (int8_t)cr->valueint;
	}

	cJSON *cg = cJSON_GetObjectItem(item, "cg");
	if (cg && cJSON_IsNumber(cg)) {
		v->cg = (int8_t)cg->valueint;
	}

	cJSON *cb = cJSON_GetObjectItem(item, "cb");
	if (cb && cJSON_IsNumber(cb)) {
		v->cb = (int8_t)cb->valueint;
	}

	cJSON *light = cJSON_GetObjectItem(item, "light");
	if (light && cJSON_IsNumber(light)) {
		v->light = (int8_t)light->valueint;
	}

	cJSON *sat = cJSON_GetObjectItem(item, "saturation");
	if (!sat) {
		sat = cJSON_GetObjectItem(item, "sat");
	}
	if (sat && cJSON_IsNumber(sat)) {
		v->sat = (int8_t)sat->valueint;
	}

	cJSON *shine = cJSON_GetObjectItem(item, "shine");
	if (shine && cJSON_IsNumber(shine)) {
		v->shine = (uint16_t)shine->valueint;
	}

	/* Color replacements */
	cJSON *c1 = cJSON_GetObjectItem(item, "c1");
	if (c1) {
		v->c1 = cJSON_IsNumber(c1) ? (uint16_t)c1->valueint : (uint16_t)parse_color_rgb555(c1);
	}

	cJSON *c2 = cJSON_GetObjectItem(item, "c2");
	if (c2) {
		v->c2 = cJSON_IsNumber(c2) ? (uint16_t)c2->valueint : (uint16_t)parse_color_rgb555(c2);
	}

	cJSON *c3 = cJSON_GetObjectItem(item, "c3");
	if (c3) {
		v->c3 = cJSON_IsNumber(c3) ? (uint16_t)c3->valueint : (uint16_t)parse_color_rgb555(c3);
	}

	/* Animation settings */
	cJSON *anim = cJSON_GetObjectItem(item, "animation");
	if (anim && cJSON_IsObject(anim)) {
		cJSON *type = cJSON_GetObjectItem(anim, "type");
		v->animation_type = (uint8_t)parse_animation_type(type && cJSON_IsString(type) ? type->valuestring : NULL);

		cJSON *frames = cJSON_GetObjectItem(anim, "frames");
		v->frames = frames && cJSON_IsNumber(frames) ? (uint8_t)frames->valueint : 8;

		cJSON *divisor = cJSON_GetObjectItem(anim, "divisor");
		v->divisor = divisor && cJSON_IsNumber(divisor) ? (uint8_t)divisor->valueint : 1;

		cJSON *random_range = cJSON_GetObjectItem(anim, "random_range");
		v->random_range = random_range && cJSON_IsNumber(random_range) ? (uint8_t)random_range->valueint : 0;

		/* Multi-branch parsing */
		cJSON *branches = cJSON_GetObjectItem(anim, "branches");
		if (branches && cJSON_IsArray(branches)) {
			v->branch_count = 0;
			cJSON *branch;
			cJSON_ArrayForEach(branch, branches)
			{
				if (v->branch_count >= MAX_ANIM_BRANCHES) {
					break;
				}

				AnimBranch *b = &v->branches[v->branch_count];

				cJSON *cond = cJSON_GetObjectItem(branch, "condition");
				if (cond && cJSON_IsString(cond)) {
					/* Parse conditions like "mod17 < 14" */
					int modulo = 0, threshold = 0;
					if (sscanf(cond->valuestring, "mod%d < %d", &modulo, &threshold) == 2) {
						b->modulo = modulo;
						b->threshold = threshold;
					} else if (strcmp(cond->valuestring, "default") == 0) {
						b->modulo = 0; /* Default branch */
						b->threshold = 0;
					}
				}

				cJSON *frames_b = cJSON_GetObjectItem(branch, "frames");
				b->frames = frames_b && cJSON_IsNumber(frames_b) ? frames_b->valueint : 8;

				cJSON *div_b = cJSON_GetObjectItem(branch, "divisor");
				b->divisor = div_b && cJSON_IsNumber(div_b) ? div_b->valueint : 1;

				v->branch_count++;
			}
		}
	}

	/* Color pulse effects (e.g., teleporter glow) */
	cJSON *color_pulse = cJSON_GetObjectItem(item, "color_pulse");
	if (color_pulse && cJSON_IsObject(color_pulse)) {
		cJSON *target = cJSON_GetObjectItem(color_pulse, "target");
		if (target && cJSON_IsString(target)) {
			if (strcmp(target->valuestring, "c1") == 0) {
				v->color_pulse_target = 1;
			} else if (strcmp(target->valuestring, "c2") == 0) {
				v->color_pulse_target = 2;
			} else if (strcmp(target->valuestring, "c3") == 0) {
				v->color_pulse_target = 3;
			}
		}

		cJSON *r = cJSON_GetObjectItem(color_pulse, "r");
		v->color_pulse_r = r && cJSON_IsNumber(r) ? (uint8_t)r->valueint : 0;

		cJSON *g = cJSON_GetObjectItem(color_pulse, "g");
		v->color_pulse_g = g && cJSON_IsNumber(g) ? (uint8_t)g->valueint : 0;

		cJSON *max_blue = cJSON_GetObjectItem(color_pulse, "max_blue");
		v->color_pulse_max = max_blue && cJSON_IsNumber(max_blue) ? (uint8_t)max_blue->valueint : 31;

		cJSON *period = cJSON_GetObjectItem(color_pulse, "period");
		v->color_pulse_period = period && cJSON_IsNumber(period) ? (uint8_t)period->valueint : 63;

		cJSON *divisor = cJSON_GetObjectItem(color_pulse, "divisor");
		v->color_pulse_divisor = divisor && cJSON_IsNumber(divisor) ? (uint8_t)divisor->valueint : 1;

		cJSON *offset = cJSON_GetObjectItem(color_pulse, "offset");
		v->color_pulse_offset = offset && cJSON_IsNumber(offset) ? (uint8_t)offset->valueint : 0;
	}

	/* Light pulse effects */
	cJSON *light_pulse = cJSON_GetObjectItem(item, "light_pulse");
	if (light_pulse && cJSON_IsObject(light_pulse)) {
		cJSON *max = cJSON_GetObjectItem(light_pulse, "max");
		v->light_pulse_max = max && cJSON_IsNumber(max) ? (uint8_t)max->valueint : 30;

		cJSON *period = cJSON_GetObjectItem(light_pulse, "period");
		v->light_pulse_period = period && cJSON_IsNumber(period) ? (uint8_t)period->valueint : 61;

		cJSON *divisor = cJSON_GetObjectItem(light_pulse, "divisor");
		v->light_pulse_divisor = divisor && cJSON_IsNumber(divisor) ? (uint8_t)divisor->valueint : 1;

		cJSON *offset = cJSON_GetObjectItem(light_pulse, "offset");
		v->light_pulse_offset = offset && cJSON_IsNumber(offset) ? (uint8_t)offset->valueint : 0;
	}

	return 0;
}

/*
 * Load a file into a buffer.
 */
static char *load_file(const char *path, size_t *out_len)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (len <= 0) {
		fclose(f);
		return NULL;
	}

	char *buf = xmalloc((size_t)len + 1, MEM_TEMP);
	if (!buf) {
		fclose(f);
		return NULL;
	}

	size_t read = fread(buf, 1, (size_t)len, f);
	fclose(f);

	buf[read] = '\0';
	if (out_len) {
		*out_len = read;
	}

	return buf;
}

/*
 * Public API implementations
 */

int sprite_config_init(void)
{
	if (init_tables() < 0) {
		return -1;
	}

	/* Try to load default config files */
	int char_loaded = sprite_config_load_characters("res/config/character_variants.json");
	int anim_loaded = sprite_config_load_animated("res/config/animated_variants.json");
	int meta_loaded = sprite_config_load_metadata("res/config/sprite_metadata.json");

	if (char_loaded < 0 && anim_loaded < 0 && meta_loaded < 0) {
		note("sprite_config: No config files found, using empty config");
	} else {
		note("sprite_config: Loaded %d character variants, %d animated variants, %d metadata entries",
		    char_loaded > 0 ? char_loaded : 0, anim_loaded > 0 ? anim_loaded : 0, meta_loaded > 0 ? meta_loaded : 0);
	}

	return 0;
}

void sprite_config_shutdown(void)
{
	if (char_table.entries) {
		xfree(char_table.entries);
		char_table.entries = NULL;
		char_table.capacity = 0;
		char_table.count = 0;
	}

	if (anim_table.entries) {
		xfree(anim_table.entries);
		anim_table.entries = NULL;
		anim_table.capacity = 0;
		anim_table.count = 0;
	}

	if (meta_table.entries) {
		xfree(meta_table.entries);
		meta_table.entries = NULL;
		meta_table.capacity = 0;
		meta_table.count = 0;
	}
}

DLL_EXPORT int sprite_config_load_characters(const char *path)
{
	if (init_tables() < 0) {
		return -1;
	}

	size_t len;
	char *json_str = load_file(path, &len);
	if (!json_str) {
		return -1;
	}

	cJSON *root = cJSON_Parse(json_str);
	xfree(json_str);

	if (!root) {
		warn("sprite_config: Failed to parse %s: %s", path, cJSON_GetErrorPtr());
		return -1;
	}

	cJSON *variants = cJSON_GetObjectItem(root, "character_variants");
	if (!variants || !cJSON_IsArray(variants)) {
		cJSON_Delete(root);
		warn("sprite_config: %s missing 'character_variants' array", path);
		return -1;
	}

	int count = 0;
	cJSON *item;
	cJSON_ArrayForEach(item, variants)
	{
		CharacterVariant v;
		if (parse_character_variant(item, &v) == 0) {
			if (insert_character(&v) == 0) {
				count++;
			}
		}
	}

	cJSON_Delete(root);
	return count;
}

DLL_EXPORT int sprite_config_load_animated(const char *path)
{
	if (init_tables() < 0) {
		return -1;
	}

	size_t len;
	char *json_str = load_file(path, &len);
	if (!json_str) {
		return -1;
	}

	cJSON *root = cJSON_Parse(json_str);
	xfree(json_str);

	if (!root) {
		warn("sprite_config: Failed to parse %s: %s", path, cJSON_GetErrorPtr());
		return -1;
	}

	cJSON *variants = cJSON_GetObjectItem(root, "animated_variants");
	if (!variants || !cJSON_IsArray(variants)) {
		cJSON_Delete(root);
		warn("sprite_config: %s missing 'animated_variants' array", path);
		return -1;
	}

	int count = 0;
	cJSON *item;
	cJSON_ArrayForEach(item, variants)
	{
		AnimatedVariant v;
		if (parse_animated_variant(item, &v) == 0) {
			if (insert_animated(&v) == 0) {
				count++;
			}
		}
	}

	cJSON_Delete(root);
	return count;
}

int sprite_config_load_from_buffer(const char *json_data, size_t len)
{
	(void)len; /* cJSON uses null-terminated strings */

	if (init_tables() < 0) {
		return -1;
	}

	cJSON *root = cJSON_Parse(json_data);
	if (!root) {
		warn("sprite_config: Failed to parse buffer: %s", cJSON_GetErrorPtr());
		return -1;
	}

	int total = 0;

	/* Try to load character variants */
	cJSON *char_variants = cJSON_GetObjectItem(root, "character_variants");
	if (char_variants && cJSON_IsArray(char_variants)) {
		cJSON *item;
		cJSON_ArrayForEach(item, char_variants)
		{
			CharacterVariant v;
			if (parse_character_variant(item, &v) == 0) {
				if (insert_character(&v) == 0) {
					total++;
				}
			}
		}
	}

	/* Try to load animated variants */
	cJSON *anim_variants = cJSON_GetObjectItem(root, "animated_variants");
	if (anim_variants && cJSON_IsArray(anim_variants)) {
		cJSON *item;
		cJSON_ArrayForEach(item, anim_variants)
		{
			AnimatedVariant v;
			if (parse_animated_variant(item, &v) == 0) {
				if (insert_animated(&v) == 0) {
					total++;
				}
			}
		}
	}

	cJSON_Delete(root);
	return total;
}

void sprite_config_clear(void)
{
	if (char_table.entries) {
		memset(char_table.entries, 0, char_table.capacity * sizeof(CharacterVariant));
		char_table.count = 0;
	}

	if (anim_table.entries) {
		memset(anim_table.entries, 0, anim_table.capacity * sizeof(AnimatedVariant));
		anim_table.count = 0;
	}
}

const CharacterVariant *sprite_config_lookup_character(int id)
{
	if (!char_table.entries || id <= 0) {
		return NULL;
	}

	unsigned int idx = CHAR_HASH(id);
	for (unsigned int i = 0; i < char_table.capacity; i++) {
		unsigned int probe = (idx + i) & (CHAR_TABLE_SIZE - 1);
		if (char_table.entries[probe].id == EMPTY_SLOT) {
			return NULL; /* Not found */
		}
		if (char_table.entries[probe].id == id) {
			return &char_table.entries[probe];
		}
	}

	return NULL;
}

const AnimatedVariant *sprite_config_lookup_animated(unsigned int id)
{
	if (!anim_table.entries || id == 0) {
		return NULL;
	}

	unsigned int idx = ANIM_HASH(id);
	for (unsigned int i = 0; i < anim_table.capacity; i++) {
		unsigned int probe = (idx + i) & (ANIM_TABLE_SIZE - 1);
		if (anim_table.entries[probe].id == EMPTY_SLOT) {
			return NULL;
		}
		if (anim_table.entries[probe].id == id) {
			return &anim_table.entries[probe];
		}
	}

	return NULL;
}

int sprite_config_apply_character(const CharacterVariant *v, int csprite, int *pscale, int *pcr, int *pcg, int *pcb,
    int *plight, int *psat, int *pc1, int *pc2, int *pc3, int *pshine, int attick)
{
	/* Initialize defaults */
	*pscale = 100;
	*pcr = 0;
	*pcg = 0;
	*pcb = 0;
	*plight = 0;
	*psat = 0;
	*pc1 = 0;
	*pc2 = 0;
	*pc3 = 0;
	*pshine = 0;

	if (!v) {
		return csprite; /* No variant, return unchanged */
	}

	/* Apply static values */
	*pscale = v->scale;
	*pcr = v->cr;
	*pcg = v->cg;
	*pcb = v->cb;
	*plight = v->light;
	*psat = v->sat;
	*pc1 = v->c1;
	*pc2 = v->c2;
	*pc3 = v->c3;
	*pshine = v->shine;

	/* Apply dynamic effects */
	if (v->dynamic_type != DYNAMIC_NONE && v->pulse_period > 0) {
		int helper = attick & (v->pulse_period - 1);
		if (helper > v->pulse_period / 2) {
			helper = v->pulse_period - helper;
		}
		int pulse_value = v->pulse_base + helper * v->pulse_amplitude / (v->pulse_period / 2);

		switch (v->dynamic_type) {
		case DYNAMIC_PULSE_CR:
			*pcr = pulse_value;
			break;
		case DYNAMIC_PULSE_CG:
			*pcg = pulse_value;
			break;
		case DYNAMIC_PULSE_CB:
			*pcb = pulse_value;
			break;
		default:
			break;
		}
	}

	return v->base_sprite;
}

unsigned int sprite_config_apply_animated(const AnimatedVariant *v, map_index_t mn, unsigned int sprite, tick_t attick,
    unsigned char *pscale, unsigned char *pcr, unsigned char *pcg, unsigned char *pcb, unsigned char *plight,
    unsigned char *psat, unsigned short *pc1, unsigned short *pc2, unsigned short *pc3, unsigned short *pshine)
{
	/* Initialize defaults */
	*pscale = 100;
	*pcr = 0;
	*pcg = 0;
	*pcb = 0;
	*plight = 0;
	*psat = 0;
	*pc1 = 0;
	*pc2 = 0;
	*pc3 = 0;
	*pshine = 0;

	if (!v) {
		return sprite; /* No variant, return unchanged */
	}

	unsigned int result = v->base_sprite;

	/* Apply static color/effect values */
	*pscale = v->scale;
	*pcr = (unsigned char)v->cr;
	*pcg = (unsigned char)v->cg;
	*pcb = (unsigned char)v->cb;
	*plight = (unsigned char)v->light;
	*psat = (unsigned char)v->sat;
	*pc1 = v->c1;
	*pc2 = v->c2;
	*pc3 = v->c3;
	*pshine = v->shine;

	/* Apply animation */
	if (v->frames > 0 && v->divisor > 0) {
		unsigned int frame = 0;
		unsigned int div = v->divisor;
		unsigned int nframes = v->frames;

		switch (v->animation_type) {
		case ANIM_CYCLE:
			/* Simple: base + (tick/divisor) % frames */
			frame = (attick / div) % nframes;
			result = v->base_sprite + frame;
			break;

		case ANIM_POSITION_CYCLE:
			/* Position-aware: includes map position for desync */
			{
				unsigned int pos_offset = (unsigned int)((mn % (size_t)MAPDX + (size_t)originx) +
				                                         (mn / (size_t)MAPDX + (size_t)originy) * 256);
				frame = (pos_offset + attick / div) % nframes;
				result = v->base_sprite + frame;
			}
			break;

		case ANIM_BIDIRECTIONAL:
			/* Ping-pong: 0->n->0 */
			{
				unsigned int cycle_len = nframes * 2 - 2;
				unsigned int cycle = (attick / div) % cycle_len;
				if (cycle >= nframes) {
					frame = cycle_len - cycle;
				} else {
					frame = cycle;
				}
				result = v->base_sprite + frame;
			}
			break;

		case ANIM_FLICKER:
			/* Random flicker */
			{
				unsigned int pos_offset = (unsigned int)((mn % (size_t)MAPDX + (size_t)originx) +
				                                         (mn / (size_t)MAPDX + (size_t)originy) * 256);
				unsigned int rand_val = (unsigned int)rrand(v->random_range + 1);
				frame = (pos_offset + attick / div + rand_val) % nframes;
				result = v->base_sprite + frame;
			}
			break;

		case ANIM_RANDOM_OFFSET:
			/* Random offset with threshold-based alternative */
			{
				unsigned int pos_offset = (unsigned int)((mn % (size_t)MAPDX + (size_t)originx) +
				                                         (mn / (size_t)MAPDX + (size_t)originy) * 256);
				unsigned int rand_val = (unsigned int)rrand(v->random_range + 1);
				unsigned int help = pos_offset + attick / div + rand_val;
				/* Use branches[0] for threshold if available */
				if (v->branch_count > 0 && v->branches[0].threshold > 0) {
					unsigned int thresh = (unsigned int)v->branches[0].threshold;
					if ((help % 50) > thresh) {
						result = v->base_sprite + 5; /* Alternative offset */
					} else if ((help % 50) < nframes) {
						frame = (help % 50);
						result = v->base_sprite + frame;
					} else {
						frame = (nframes * 2 - 2) - (help % 50);
						result = v->base_sprite + frame;
					}
				} else {
					frame = help % nframes;
					result = v->base_sprite + frame;
				}
			}
			break;

		case ANIM_MULTI_BRANCH:
			/* Multi-branch conditional */
			{
				unsigned int pos_offset = (unsigned int)((mn % (size_t)MAPDX + (size_t)originx) +
				                                         (mn / (size_t)MAPDX + (size_t)originy) * 256);
				unsigned int help = pos_offset + attick / div;

				/* Find matching branch */
				int found = 0;
				for (int i = 0; i < v->branch_count && !found; i++) {
					const AnimBranch *b = &v->branches[i];
					unsigned int b_div = (unsigned int)b->divisor;
					unsigned int b_frames = (unsigned int)b->frames;
					if (b->modulo == 0) {
						/* Default branch */
						frame = (pos_offset + attick / b_div) % b_frames;
						result = v->base_sprite + frame;
						found = 1;
					} else if ((int)(help % (unsigned int)b->modulo) < b->threshold) {
						frame = (pos_offset + attick / b_div) % b_frames;
						result = v->base_sprite + frame;
						found = 1;
					}
				}

				if (!found) {
					frame = (pos_offset + attick / div) % nframes;
					result = v->base_sprite + frame;
				}
			}
			break;

		case ANIM_PULSE:
			/* Pulse animation (bidirectional with color effect) */
			{
				unsigned int cycle_len = nframes * 2;
				unsigned int cycle = (attick / div) % cycle_len;
				if (cycle >= nframes) {
					frame = cycle_len - cycle - 1;
				} else {
					frame = cycle;
				}
				result = v->base_sprite + frame;
			}
			break;

		case ANIM_NONE:
		default:
			result = v->base_sprite;
			break;
		}
	}

	/* Apply color pulse effect (e.g., teleporter glow) */
	if (v->color_pulse_target != 0 && v->color_pulse_period > 0) {
		/* Calculate pulsing value: abs(max - (attick % period)) / divisor + offset */
		int pulse_raw = (int)v->color_pulse_max - (int)(attick % v->color_pulse_period);
		int pulse_val = (pulse_raw < 0) ? -pulse_raw : pulse_raw;
		if (v->color_pulse_divisor > 1) {
			pulse_val = pulse_val / v->color_pulse_divisor;
		}
		pulse_val += v->color_pulse_offset;

		/* Clamp to 5-bit range */
		if (pulse_val > 31) {
			pulse_val = 31;
		}
		if (pulse_val < 0) {
			pulse_val = 0;
		}

		/* Construct RGB555 color: IRGB(r, g, pulse_val) */
		unsigned short color =
		    (unsigned short)(((v->color_pulse_r & 31) << 10) | ((v->color_pulse_g & 31) << 5) | (pulse_val & 31));

		switch (v->color_pulse_target) {
		case 1:
			*pc1 = color;
			break;
		case 2:
			*pc2 = color;
			break;
		case 3:
			*pc3 = color;
			break;
		}
	}

	/* Apply light pulse effect */
	if (v->light_pulse_period > 0) {
		int pulse_raw = (int)v->light_pulse_max - (int)(attick % v->light_pulse_period);
		int pulse_val = (pulse_raw < 0) ? -pulse_raw : pulse_raw;
		if (v->light_pulse_divisor > 1) {
			pulse_val = pulse_val / v->light_pulse_divisor;
		}
		pulse_val += v->light_pulse_offset;

		/* Clamp to unsigned char range */
		if (pulse_val > 255) {
			pulse_val = 255;
		}
		if (pulse_val < 0) {
			pulse_val = 0;
		}

		*plight = (unsigned char)pulse_val;
	}

	return result;
}

void sprite_config_get_stats(size_t *char_count, size_t *anim_count)
{
	if (char_count) {
		*char_count = char_table.count;
	}
	if (anim_count) {
		*anim_count = anim_table.count;
	}
}

/*
 * Sprite metadata implementation
 */

static int init_metadata_table(void)
{
	if (meta_table.entries == NULL) {
		meta_table.entries = xmalloc(META_TABLE_SIZE * sizeof(SpriteMetadata), MEM_GAME);
		if (!meta_table.entries) {
			fail("sprite_config: Failed to allocate metadata table");
			return -1;
		}
		memset(meta_table.entries, 0, META_TABLE_SIZE * sizeof(SpriteMetadata));
		meta_table.capacity = META_TABLE_SIZE;
		meta_table.count = 0;
	}
	return 0;
}

static int insert_metadata(const SpriteMetadata *m)
{
	if (meta_table.count >= meta_table.capacity * 3 / 4) {
		warn("sprite_config: Metadata table is full (>75%% load)");
		return -1;
	}

	unsigned int idx = META_HASH(m->id);
	for (unsigned int i = 0; i < meta_table.capacity; i++) {
		unsigned int probe = (idx + i) & (META_TABLE_SIZE - 1);
		if (meta_table.entries[probe].id == EMPTY_SLOT || meta_table.entries[probe].id == m->id) {
			if (meta_table.entries[probe].id == EMPTY_SLOT) {
				meta_table.count++;
			}
			memcpy(&meta_table.entries[probe], m, sizeof(SpriteMetadata));
			return 0;
		}
	}

	warn("sprite_config: Failed to insert metadata %u", m->id);
	return -1;
}

static int parse_sprite_metadata(cJSON *item, SpriteMetadata *m)
{
	memset(m, 0, sizeof(SpriteMetadata));

	cJSON *id = cJSON_GetObjectItem(item, "id");
	if (!id || !cJSON_IsNumber(id)) {
		warn("sprite_config: Metadata entry missing 'id'");
		return -1;
	}
	m->id = (uint32_t)id->valueint;

	/* Cut sprite */
	cJSON *cut_offset = cJSON_GetObjectItem(item, "cut_offset");
	cJSON *cut_sprite = cJSON_GetObjectItem(item, "cut_sprite");
	cJSON *cut_negative = cJSON_GetObjectItem(item, "cut_negative");

	if (cut_offset && cJSON_IsNumber(cut_offset)) {
		m->cut_result = cut_offset->valueint;
		m->cut_offset = 1;
	} else if (cut_sprite && cJSON_IsNumber(cut_sprite)) {
		m->cut_result = cut_sprite->valueint;
		m->cut_offset = 0;
	}

	if (cut_negative && cJSON_IsBool(cut_negative)) {
		m->cut_negative = cJSON_IsTrue(cut_negative) ? 1 : 0;
	}

	/* Door and mov */
	cJSON *door = cJSON_GetObjectItem(item, "door");
	if (door && cJSON_IsBool(door)) {
		m->door = cJSON_IsTrue(door) ? 1 : 0;
	}

	cJSON *mov = cJSON_GetObjectItem(item, "mov");
	if (mov && cJSON_IsNumber(mov)) {
		m->mov = (int8_t)mov->valueint;
	}

	/* Y-add */
	cJSON *yadd = cJSON_GetObjectItem(item, "yadd");
	if (yadd && cJSON_IsNumber(yadd)) {
		m->yadd = (int16_t)yadd->valueint;
	}

	/* Layer */
	cJSON *layer = cJSON_GetObjectItem(item, "layer");
	if (layer && cJSON_IsNumber(layer)) {
		m->layer = (int16_t)layer->valueint;
	}

	/* Offset */
	cJSON *offset_x = cJSON_GetObjectItem(item, "offset_x");
	cJSON *offset_y = cJSON_GetObjectItem(item, "offset_y");
	if (offset_x && cJSON_IsNumber(offset_x)) {
		m->offset_x = (int8_t)offset_x->valueint;
	}
	if (offset_y && cJSON_IsNumber(offset_y)) {
		m->offset_y = (int8_t)offset_y->valueint;
	}

	/* No lighting */
	cJSON *no_lighting = cJSON_GetObjectItem(item, "no_lighting");
	if (no_lighting && cJSON_IsBool(no_lighting)) {
		m->no_lighting = cJSON_IsTrue(no_lighting) ? 1 : 0;
	}

	return 0;
}

DLL_EXPORT int sprite_config_load_metadata(const char *path)
{
	if (init_metadata_table() < 0) {
		return -1;
	}

	size_t len;
	char *json_str = load_file(path, &len);
	if (!json_str) {
		return -1;
	}

	cJSON *root = cJSON_Parse(json_str);
	xfree(json_str);

	if (!root) {
		warn("sprite_config: Failed to parse %s: %s", path, cJSON_GetErrorPtr());
		return -1;
	}

	cJSON *sprites = cJSON_GetObjectItem(root, "sprite_metadata");
	if (!sprites || !cJSON_IsArray(sprites)) {
		cJSON_Delete(root);
		warn("sprite_config: %s missing 'sprite_metadata' array", path);
		return -1;
	}

	int count = 0;
	cJSON *item;
	cJSON_ArrayForEach(item, sprites)
	{
		SpriteMetadata m;
		if (parse_sprite_metadata(item, &m) == 0) {
			if (insert_metadata(&m) == 0) {
				count++;
			}
		}
	}

	cJSON_Delete(root);
	return count;
}

const SpriteMetadata *sprite_config_lookup_metadata(unsigned int id)
{
	if (!meta_table.entries || id == 0) {
		return NULL;
	}

	unsigned int idx = META_HASH(id);
	for (unsigned int i = 0; i < meta_table.capacity; i++) {
		unsigned int probe = (idx + i) & (META_TABLE_SIZE - 1);
		if (meta_table.entries[probe].id == EMPTY_SLOT) {
			return NULL;
		}
		if (meta_table.entries[probe].id == id) {
			return &meta_table.entries[probe];
		}
	}

	return NULL;
}

int sprite_config_is_cut_sprite(unsigned int sprite)
{
	const SpriteMetadata *m = sprite_config_lookup_metadata(sprite);
	if (!m || m->cut_result == 0) {
		/*
		 * Return the sprite ID itself when not a cut sprite.
		 * game_lighting.c uses: tmp = abs(is_cut_sprite(sprite));
		 *                       if (tmp != sprite) sprite = tmp;
		 * Returning 0 would incorrectly hide the sprite.
		 * Returning sprite means abs(sprite) == sprite, no change.
		 */
		return (int)sprite;
	}

	int result;
	if (m->cut_offset) {
		/* cut_result is offset from sprite ID */
		result = (int)sprite + m->cut_result;
	} else {
		/* cut_result is the specific sprite ID */
		result = m->cut_result;
	}

	return m->cut_negative ? -result : result;
}

int sprite_config_is_door_sprite(unsigned int sprite)
{
	const SpriteMetadata *m = sprite_config_lookup_metadata(sprite);
	return m ? m->door : 0;
}

int sprite_config_is_mov_sprite(unsigned int sprite, int itemhint)
{
	const SpriteMetadata *m = sprite_config_lookup_metadata(sprite);
	if (!m || m->mov == 0) {
		return itemhint; /* Use default */
	}
	return m->mov;
}

int sprite_config_is_yadd_sprite(unsigned int sprite)
{
	const SpriteMetadata *m = sprite_config_lookup_metadata(sprite);
	return m ? m->yadd : 0;
}

int sprite_config_get_lay_sprite(int sprite, int lay)
{
	const SpriteMetadata *m = sprite_config_lookup_metadata((unsigned int)sprite);
	if (!m || m->layer == 0) {
		return lay; /* Use default */
	}
	return m->layer;
}

int sprite_config_get_offset_sprite(int sprite, int *px, int *py)
{
	const SpriteMetadata *m = sprite_config_lookup_metadata((unsigned int)sprite);
	if (!m || (m->offset_x == 0 && m->offset_y == 0)) {
		return 0;
	}

	if (px) {
		*px = m->offset_x;
	}
	if (py) {
		*py = m->offset_y;
	}
	return 1;
}

int sprite_config_no_lighting_sprite(unsigned int sprite)
{
	const SpriteMetadata *m = sprite_config_lookup_metadata(sprite);
	return m ? m->no_lighting : 0;
}
