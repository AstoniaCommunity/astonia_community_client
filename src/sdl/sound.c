/*
 * Part of Astonia Client. Please read license.txt.
 *
 * Sound
 *
 * Loads and plays sounds via SDL3_mixer library.
 *
 * Server-triggered sounds use IDs mapped via sounds.json config files.
 * The mapping is loaded from zip archives in priority order:
 *   1. res/sx_mod.zip/sounds.json   (highest priority, overrides all)
 *   2. res/sx_patch.zip/sounds.json (overrides base)
 *   3. res/sx.zip/sounds.json       (base mappings)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zip.h>
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include "astonia.h"
#include "sdl/sdl.h"
#include "sdl/sdl_private.h"
#include "lib/cjson/cJSON.h"

// Mod sound API data structures

#define MAX_MOD_SOUNDS 64

// Sound zip archives (opened once, kept open for sound loading)
// Priority: sx_mod.zip > sx_patch.zip > sx.zip
static zip_t *sx_zip = NULL; // Base sounds (res/sx.zip)
static zip_t *sx_patch_zip = NULL; // Patch sounds (res/sx_patch.zip)
static zip_t *sx_mod_zip = NULL; // Mod sounds (res/sx_mod.zip)

// Mod-loaded sounds (separate from built-in sound_effect[])
static MIX_Audio *mod_sounds[MAX_MOD_SOUNDS];
static int mod_sound_count = 0;

// Track state for channel queries
typedef struct {
	int in_use; // Is this channel currently playing?
	int sound_handle; // Which sound handle is playing (0 = built-in, >0 = mod sound)
	int looping; // Is this channel looping?
	float volume; // Current volume (0.0 - 1.0)
	float fade_target; // Target volume for fade
	float fade_step; // Volume change per tick
	int fade_ticks_left; // Ticks remaining in fade
} channel_state_t;

static channel_state_t channel_states[MAX_SOUND_CHANNELS];

// Sound ID mapping (JSON-based)
// Maps server sound IDs to file paths within zip archives.

#define MAX_SOUND_ID 256

// Mapping table: sound_id -> path string (NULL if not mapped)
static char *sound_map[MAX_SOUND_ID];
static int sound_map_loaded = 0;

// Legacy fallback table for backwards compatibility (used if no sounds.json)
static const char *sfx_fallback[] = {
    "sfx/null.wav", // 0
    "sfx/sdemonawaken.wav", // 1
    "sfx/door.wav", // 2
    "sfx/door2.wav", // 3
    "sfx/man_dead.wav", // 4
    "sfx/thunderrumble3.wav", // 5
    "sfx/explosion.wav", // 6
    "sfx/hit_body2.wav", // 7
    "sfx/miss.wav", // 8
    "sfx/man_hurt.wav", // 9
    "sfx/pigeon.wav", // 10
    "sfx/crow.wav", // 11
    "sfx/crow2.wav", // 12
    "sfx/laughingman6.wav", // 13
    "sfx/drip1.wav", // 14
    "sfx/drip2.wav", // 15
    "sfx/drip3.wav", // 16
    "sfx/howl1.wav", // 17
    "sfx/howl2.wav", // 18
    "sfx/bird1.wav", // 19
    "sfx/bird2.wav", // 20
    "sfx/bird3.wav", // 21
    "sfx/catmeow2.wav", // 22
    "sfx/cricket.wav", // 23
    "sfx/specht.wav", // 24
    "sfx/haeher.wav", // 25
    "sfx/owl1.wav", // 26
    "sfx/owl2.wav", // 27
    "sfx/owl3.wav", // 28
    "sfx/magic.wav", // 29
    "sfx/flash.wav", // 30 - lightning strike
    "sfx/scarynote.wav", // 31 - freeze
    "sfx/woman_hurt.wav", // 32
    "sfx/woman_dead.wav", // 33
    "sfx/parry1.wav", // 34
    "sfx/parry2.wav", // 35
    "sfx/dungeon_breath1.wav", // 36
    "sfx/dungeon_breath2.wav", // 37
    "sfx/pents_mood1.wav", // 38
    "sfx/pents_mood2.wav", // 39
    "sfx/pents_mood3.wav", // 40
    "sfx/ancient_activate.wav", // 41
    "sfx/pent_activate.wav", // 42
    "sfx/ancient_runout.wav", // 43
    "sfx/bubble1.wav", // 44
    "sfx/bubble2.wav", // 45
    "sfx/bubble3.wav", // 46
    "sfx/whale1.wav", // 47
    "sfx/whale2.wav", // 48
    "sfx/whale3.wav", // 49
    NULL // Sentinel
};
static int sfx_fallback_cnt = (int)(sizeof(sfx_fallback) / sizeof(sfx_fallback[0])) - 1;

int sound_volume = 128;
static uint64_t time_play_sound = 0;

static MIX_Audio *sound_effect[MAXSOUND];

MIX_Audio *load_sound_from_zip(zip_t *zip_archive, const char *filename);

/**
 * Load a text file from a zip archive.
 * @return Allocated string (caller must free) or NULL on error.
 */
static char *load_text_from_zip(zip_t *zip_archive, const char *filename)
{
	if (!zip_archive) {
		return NULL;
	}

	zip_stat_t stat;
	if (zip_stat(zip_archive, filename, 0, &stat) != 0 || !(stat.valid & ZIP_STAT_SIZE)) {
		return NULL; // File not found
	}

	zip_uint64_t len = stat.size;
	if (len > 1024 * 1024) { // Sanity limit: 1MB
		warn("sounds.json too large in archive");
		return NULL;
	}

	zip_file_t *zf = zip_fopen(zip_archive, filename, 0);
	if (!zf) {
		return NULL;
	}

	char *buffer = xmalloc((size_t)len + 1, MEM_TEMP);
	if ((zip_uint64_t)zip_fread(zf, buffer, len) != len) {
		xfree(buffer);
		zip_fclose(zf);
		return NULL;
	}
	zip_fclose(zf);
	buffer[len] = '\0';

	return buffer;
}

/**
 * Parse sounds.json and merge into sound_map.
 * Later calls override earlier mappings (for mod priority).
 */
static int load_sound_map_from_json(const char *json_str, const char *source_name)
{
	if (!json_str) {
		return 0;
	}

	cJSON *root = cJSON_Parse(json_str);
	if (!root) {
		warn("Failed to parse %s: %s", source_name, cJSON_GetErrorPtr());
		return -1;
	}

	cJSON *sounds = cJSON_GetObjectItem(root, "sounds");
	if (!sounds || !cJSON_IsObject(sounds)) {
		cJSON_Delete(root);
		warn("%s missing 'sounds' object", source_name);
		return -1;
	}

	int count = 0;
	cJSON *item;
	cJSON_ArrayForEach(item, sounds)
	{
		if (!cJSON_IsString(item)) {
			continue;
		}

		// Key is the sound ID as string, value is the path
		const char *key = item->string;
		const char *path = item->valuestring;

		int id = atoi(key);
		if (id < 0 || id >= MAX_SOUND_ID) {
			warn("%s: sound ID %d out of range (0-%d)", source_name, id, MAX_SOUND_ID - 1);
			continue;
		}

		// Free any existing mapping (for override)
		if (sound_map[id]) {
			xfree(sound_map[id]);
		}

		// Store the new mapping
		size_t path_len = strlen(path) + 1;
		sound_map[id] = xmalloc(path_len, MEM_GLOB);
		memcpy(sound_map[id], path, path_len);
		count++;
	}

	cJSON_Delete(root);
	return count;
}

/**
 * Load sound mappings from all zip archives.
 * Priority: base -> patch -> mod (later overrides earlier)
 */
static void load_sound_mappings(void)
{
	int total = 0;
	char *json;

	// Clear existing mappings
	for (int i = 0; i < MAX_SOUND_ID; i++) {
		if (sound_map[i]) {
			xfree(sound_map[i]);
			sound_map[i] = NULL;
		}
	}

	// Load base mappings from sx.zip
	json = load_text_from_zip(sx_zip, "sounds.json");
	if (json) {
		int count = load_sound_map_from_json(json, "sx.zip/sounds.json");
		if (count > 0) {
			total += count;
			note("Loaded %d sound mappings from sx.zip", count);
		}
		xfree(json);
	}

	// Load patch overrides from sx_patch.zip
	json = load_text_from_zip(sx_patch_zip, "sounds.json");
	if (json) {
		int count = load_sound_map_from_json(json, "sx_patch.zip/sounds.json");
		if (count > 0) {
			total += count;
			note("Loaded %d sound mappings from sx_patch.zip", count);
		}
		xfree(json);
	}

	// Load mod overrides from sx_mod.zip
	json = load_text_from_zip(sx_mod_zip, "sounds.json");
	if (json) {
		int count = load_sound_map_from_json(json, "sx_mod.zip/sounds.json");
		if (count > 0) {
			total += count;
			note("Loaded %d sound mappings from sx_mod.zip", count);
		}
		xfree(json);
	}

	// If no sounds.json found, use legacy fallback
	if (total == 0) {
		note("No sounds.json found, using legacy sound mappings");
		for (int i = 0; i < sfx_fallback_cnt && i < MAX_SOUND_ID; i++) {
			if (sfx_fallback[i]) {
				size_t len = strlen(sfx_fallback[i]) + 1;
				sound_map[i] = xmalloc(len, MEM_GLOB);
				memcpy(sound_map[i], sfx_fallback[i], len);
			}
		}
	}

	sound_map_loaded = 1;
}

/**
 * Get the file path for a server sound ID.
 * @return Path string or NULL if not mapped.
 */
static const char *get_sound_path(int sound_id)
{
	if (sound_id < 0 || sound_id >= MAX_SOUND_ID) {
		return NULL;
	}
	return sound_map[sound_id];
}

/**
 * Free all sound mappings.
 */
static void free_sound_mappings(void)
{
	for (int i = 0; i < MAX_SOUND_ID; i++) {
		if (sound_map[i]) {
			xfree(sound_map[i]);
			sound_map[i] = NULL;
		}
	}
	sound_map_loaded = 0;
}

int init_sound(void)
{
	int err;

	if (!(game_options & GO_SOUND)) {
		return -1;
	}

	// Open sound zip archives (keep open for mod sound loading)
	// Base sounds - required
	sx_zip = zip_open("res/sx.zip", ZIP_RDONLY, &err);
	if (!sx_zip) {
		warn("Opening sx.zip failed with error code %d.", err);
		game_options &= ~GO_SOUND;
		return -1;
	}

	// Patch and mod sounds - optional
	sx_patch_zip = zip_open("res/sx_patch.zip", ZIP_RDONLY, NULL);
	sx_mod_zip = zip_open("res/sx_mod.zip", ZIP_RDONLY, NULL);

	if (sx_patch_zip) {
		note("Loaded sx_patch.zip for sound patches");
	}
	if (sx_mod_zip) {
		note("Loaded sx_mod.zip for mod sounds");
	}

	// Load sound ID mappings from sounds.json files
	load_sound_mappings();

	// Pre-load all mapped sound effects
	for (int i = 1; i < MAXSOUND && i < MAX_SOUND_ID; i++) {
		const char *path = get_sound_path(i);
		if (path) {
			// Try to load from all zips (mod -> patch -> base)
			MIX_Audio *audio = NULL;
			if (sx_mod_zip) {
				audio = load_sound_from_zip(sx_mod_zip, path);
			}
			if (!audio && sx_patch_zip) {
				audio = load_sound_from_zip(sx_patch_zip, path);
			}
			if (!audio && sx_zip) {
				audio = load_sound_from_zip(sx_zip, path);
			}
			sound_effect[i] = audio;
		}
	}

	return 0;
}

MIX_Audio *load_sound_from_zip(zip_t *zip_archive, const char *filename)
{
	zip_stat_t stat;
	zip_file_t *zip_file;
	char *buffer;
	zip_uint64_t len;
	SDL_IOStream *rw;
	MIX_Audio *audio;

	if (!zip_archive || !filename) {
		return NULL;
	}

	// Get file stats from zip - silent on not found (we try multiple zips)
	if (zip_stat(zip_archive, filename, 0, &stat) != 0 || !(stat.valid & ZIP_STAT_SIZE)) {
		return NULL;
	}
	len = stat.size;
	if (len > INT_MAX) {
		warn("Sound file %s is too large.", filename);
		return NULL;
	}

	// Open file in zip
	zip_file = zip_fopen(zip_archive, filename, 0);
	if (!zip_file) {
		return NULL;
	}

	// Allocate buffer and read file data
	buffer = xmalloc(len, MEM_TEMP6);
	if ((zip_uint64_t)zip_fread(zip_file, buffer, len) != len) {
		warn("Could not read sound file %s from archive.", filename);
		zip_fclose(zip_file);
		xfree(buffer);
		return NULL;
	}
	zip_fclose(zip_file);

	// Create an SDL_IOStream from the memory buffer
	rw = SDL_IOFromConstMem(buffer, (size_t)len);
	if (!rw) {
		warn("Could not create SDL_IOStream for sound %s.", filename);
		xfree(buffer);
		return NULL;
	}

	// Load WAV from the IOStream
	// mixer=NULL means use first created mixer, predecode=true loads fully into memory, closeio=true frees the IOStream
	audio = MIX_LoadAudio_IO(NULL, rw, true, true);
	xfree(buffer); // Free the original buffer to prevent a memory leak.

	return audio;
}

void sound_exit(void)
{
	int i;

	// Cleanup mod sounds first
	sound_cleanup_mod_sounds();

	// Free all built-in sound effects
	for (i = 1; i < MAXSOUND; i++) {
		if (sound_effect[i]) {
			MIX_DestroyAudio(sound_effect[i]);
			sound_effect[i] = NULL;
		}
	}

	// Free sound ID mappings
	free_sound_mappings();

	// Close sound zip archives
	if (sx_mod_zip) {
		zip_close(sx_mod_zip);
		sx_mod_zip = NULL;
	}
	if (sx_patch_zip) {
		zip_close(sx_patch_zip);
		sx_patch_zip = NULL;
	}
	if (sx_zip) {
		zip_close(sx_zip);
		sx_zip = NULL;
	}

	return;
}

static void play_sdl_sound(unsigned int nr, int distance, int angle);

static void play_sdl_sound(unsigned int nr, int distance, int angle)
{
	static int sound_channel = 0;
	uint64_t time_start;

	// Check if sound is enabled
	if (!(game_options & GO_SOUND)) {
		return;
	}

	if (nr < 1U || nr >= (unsigned int)MAXSOUND) {
		return;
	}

	if (!sound_effect[nr]) {
		return; // Audio not loaded
	}

	// For debugging/optimization
	time_start = SDL_GetTicks();

#if 0
	const char *path = get_sound_path(nr);
	note("nr = %d: %s, distance = %d, angle = %d", nr, path ? path : "(unmapped)", distance, angle);
#endif

	// Get the track for this channel
	MIX_Track *track = sdl_tracks[sound_channel];
	if (!track) {
		warn("Track %d is NULL - audio system not initialized correctly", sound_channel);
		return;
	}

	// Convert angle/distance to 3D position for SDL3_mixer
	// SDL2_mixer used angle (degrees) and distance (0-255)
	// SDL3_mixer uses 3D coordinates via MIX_Point3D struct
	const float radians = (float)angle * (SDL_PI_F / 180.0f);
	const float f_dist = (float)distance / 255.0f; // Normalize to 0.0-1.0
	MIX_Point3D position = {.x = SDL_cosf(radians) * f_dist,
	    .y = 0.0f, // Keep vertically centered
	    .z = SDL_sinf(radians) * f_dist};

	// Set 3D position
	MIX_SetTrack3DPosition(track, &position);

	// Set volume gain
	// Note: sound_volume is an int (0 to -128) for backwards compatibility with the server protocol.
	// 0 = maximum volume (gain 1.0), -128 = silence (gain 0.0)
	// Convert from negative attenuation to positive gain: gain = 1.0 + (sound_volume / 128.0)
	float gain = 1.0f + ((float)sound_volume / 128.0f);
	MIX_SetTrackGain(track, gain);

	// Assign the audio to the track and play it
	MIX_SetTrackAudio(track, sound_effect[nr]);
	MIX_PlayTrack(track, 0); // 0 means use default properties

	// Increment sound channel so the next sound played is on its own layer
	sound_channel++;
	if (sound_channel >= MAX_SOUND_CHANNELS) {
		sound_channel = 0;
	}

	// For debug/optimization
	time_play_sound += SDL_GetTicks() - time_start;

	return;
}

/*
 * play_sound: Plays a sound effect with volume and pan.
 * nr: Sound effect number.
 * vol: Volume, from 0 (max) to -9999 (min).
 * p: Pan, from -9999 (left) to 9999 (right).
 */
void play_sound(unsigned int nr, int vol, int p)
{
	int dist, angle;
	if (!(game_options & GO_SOUND)) {
		return;
	}

	// force volume and pan to sane values
	if (vol > 0) {
		vol = 0;
	}
	if (vol < -9999) {
		vol = -9999;
	}

	if (p > 9999) {
		p = 9999;
	}
	if (p < -9999) {
		p = -9999;
	}

	// translate parameters to SDL
	// TODO: change client server protocol to provide angle instead of position
	dist = -(int)(vol) * 255 / 10000;
	angle = (int)p * 180 / 10000;

#if 0
	if (nr < (unsigned int)sfx_name_cnt) {
		note("nr = %d: %s, distance = %d, angle = %d (vol=%d, p=%d)", nr, sfx_name[nr], dist, angle, vol, p);
	} else {
		note("nr = %d: (unknown), distance = %d, angle = %d (vol=%d, p=%d)", nr, dist, angle, vol, p);
	}
#endif

	play_sdl_sound(nr, dist, angle);
}

// Mod sound API implementation

/**
 * Try to load a sound from a zip archive.
 * @param zip_archive  The zip archive to search
 * @param path         Path within the archive
 * @return             MIX_Audio* on success, NULL if not found
 */
static MIX_Audio *try_load_sound_from_zip(zip_t *zip_archive, const char *path)
{
	if (!zip_archive) {
		return NULL;
	}

	// Check if file exists in this archive
	zip_stat_t stat;
	if (zip_stat(zip_archive, path, 0, &stat) != 0) {
		return NULL; // File not found in this archive
	}

	return load_sound_from_zip(zip_archive, path);
}

/**
 * Load a sound effect from zip archives.
 * Search order: sx_mod.zip -> sx_patch.zip -> sx.zip
 * @param path   Path to sound file within zip (e.g., "weather/rain_loop.ogg")
 * @return       Sound handle (>0) on success, 0 on failure
 */
int sound_load(const char *path)
{
	MIX_Audio *audio = NULL;

	if (!path || !path[0]) {
		warn("sound_load: NULL or empty path");
		return 0;
	}

	if (mod_sound_count >= MAX_MOD_SOUNDS) {
		warn("sound_load: Maximum mod sounds reached (%d)", MAX_MOD_SOUNDS);
		return 0;
	}

	// Search for sound in zip archives (priority: mod > patch > base)
	// 1. Try sx_mod.zip first (mod additions)
	audio = try_load_sound_from_zip(sx_mod_zip, path);

	// 2. Try sx_patch.zip (patches/overrides)
	if (!audio) {
		audio = try_load_sound_from_zip(sx_patch_zip, path);
	}

	// 3. Fall back to sx.zip (base sounds)
	if (!audio) {
		audio = try_load_sound_from_zip(sx_zip, path);
	}

	if (!audio) {
		warn("sound_load: Could not find '%s' in any sound archive", path);
		return 0;
	}

	// Find a free slot (start at 1, slot 0 is reserved for "invalid")
	for (int i = 1; i < MAX_MOD_SOUNDS; i++) {
		if (!mod_sounds[i]) {
			mod_sounds[i] = audio;
			if (i >= mod_sound_count) {
				mod_sound_count = i + 1;
			}
			return i; // Return handle (1-based index)
		}
	}

	// Should not reach here if count check passed, but be safe
	MIX_DestroyAudio(audio);
	warn("sound_load: No free slots available");
	return 0;
}

/**
 * Unload a previously loaded sound.
 * @param handle Sound handle from sound_load()
 */
void sound_unload(int handle)
{
	if (handle < 1 || handle >= MAX_MOD_SOUNDS) {
		return;
	}

	if (mod_sounds[handle]) {
		// Stop any channels playing this sound
		for (int i = 0; i < MAX_SOUND_CHANNELS; i++) {
			if (channel_states[i].in_use && channel_states[i].sound_handle == handle) {
				sound_stop(i + 1); // Channel IDs are 1-based
			}
		}
		MIX_DestroyAudio(mod_sounds[handle]);
		mod_sounds[handle] = NULL;
	}
}

/**
 * Internal: Play a mod sound on the next available channel.
 * @param handle  Sound handle from sound_load()
 * @param volume  Volume level (0.0 = silent, 1.0 = full)
 * @param loop    0 for one-shot, -1 for infinite loop
 * @return        Channel ID (>0) for controlling playback, 0 on failure
 */
static int sound_play_internal(int handle, float volume, int loop)
{
	static int next_channel = 0;
	MIX_Audio *audio;
	MIX_Track *track;
	int channel;

	// Check if sound is enabled
	if (!(game_options & GO_SOUND)) {
		return 0;
	}

	// Validate handle
	if (handle < 1 || handle >= MAX_MOD_SOUNDS || !mod_sounds[handle]) {
		warn("sound_play: Invalid sound handle %d", handle);
		return 0;
	}

	audio = mod_sounds[handle];

	// Find a free channel or use round-robin
	channel = next_channel;
	for (int i = 0; i < MAX_SOUND_CHANNELS; i++) {
		int test_ch = (next_channel + i) % MAX_SOUND_CHANNELS;
		if (!channel_states[test_ch].in_use) {
			channel = test_ch;
			break;
		}
	}

	// Update round-robin counter
	next_channel = (channel + 1) % MAX_SOUND_CHANNELS;

	track = sdl_tracks[channel];
	if (!track) {
		warn("sound_play: Track %d is NULL", channel);
		return 0;
	}

	// Stop any currently playing sound on this channel
	if (channel_states[channel].in_use) {
		MIX_StopTrack(track, 0); // 0 = immediate stop (no fade)
	}

	// Clamp volume
	if (volume < 0.0f) {
		volume = 0.0f;
	}
	if (volume > 1.0f) {
		volume = 1.0f;
	}

	// Apply master volume
	float master = sound_get_master_volume();
	float final_volume = volume * master;

	// Set track properties
	MIX_SetTrackGain(track, final_volume);
	MIX_SetTrackAudio(track, audio);

	// Set looping: -1 = infinite loop, 0 = play once
	MIX_SetTrackLoops(track, loop != 0 ? -1 : 0);

	// Play the track
	MIX_PlayTrack(track, 0);

	// Update channel state
	channel_states[channel].in_use = 1;
	channel_states[channel].sound_handle = handle;
	channel_states[channel].looping = (loop != 0);
	channel_states[channel].volume = volume;
	channel_states[channel].fade_target = volume;
	channel_states[channel].fade_step = 0.0f;
	channel_states[channel].fade_ticks_left = 0;

	// Return 1-based channel ID
	return channel + 1;
}

/**
 * Play a sound effect once.
 * @param handle  Sound handle from sound_load()
 * @param volume  Volume level (0.0 = silent, 1.0 = full)
 * @return        Channel ID (>0) for controlling playback, 0 on failure
 */
int sound_play(int handle, float volume)
{
	return sound_play_internal(handle, volume, 0);
}

/**
 * Play a sound effect in a loop.
 * @param handle  Sound handle from sound_load()
 * @param volume  Volume level (0.0 = silent, 1.0 = full)
 * @return        Channel ID (>0) for controlling playback, 0 on failure
 */
int sound_play_loop(int handle, float volume)
{
	return sound_play_internal(handle, volume, -1);
}

/**
 * Stop a playing sound.
 * @param channel Channel ID from sound_play() or sound_play_loop()
 */
void sound_stop(int channel)
{
	int ch_idx = channel - 1; // Convert to 0-based

	if (ch_idx < 0 || ch_idx >= MAX_SOUND_CHANNELS) {
		return;
	}

	MIX_Track *track = sdl_tracks[ch_idx];
	if (track && channel_states[ch_idx].in_use) {
		MIX_StopTrack(track, 0); // 0 = immediate stop (no fade)
		channel_states[ch_idx].in_use = 0;
		channel_states[ch_idx].sound_handle = 0;
		channel_states[ch_idx].looping = 0;
		channel_states[ch_idx].fade_ticks_left = 0;
	}
}

/**
 * Stop all sounds on all channels.
 */
void sound_stop_all(void)
{
	for (int i = 0; i < MAX_SOUND_CHANNELS; i++) {
		if (channel_states[i].in_use) {
			sound_stop(i + 1); // Channel IDs are 1-based
		}
	}
}

/**
 * Set volume for a playing sound channel.
 * @param channel Channel ID from sound_play()
 * @param volume  Volume level (0.0 = silent, 1.0 = full)
 */
void sound_set_volume(int channel, float volume)
{
	int ch_idx = channel - 1;

	if (ch_idx < 0 || ch_idx >= MAX_SOUND_CHANNELS) {
		return;
	}

	if (!channel_states[ch_idx].in_use) {
		return;
	}

	// Clamp volume
	if (volume < 0.0f) {
		volume = 0.0f;
	}
	if (volume > 1.0f) {
		volume = 1.0f;
	}

	MIX_Track *track = sdl_tracks[ch_idx];
	if (track) {
		float master = sound_get_master_volume();
		MIX_SetTrackGain(track, volume * master);
		channel_states[ch_idx].volume = volume;
		// Cancel any ongoing fade
		channel_states[ch_idx].fade_target = volume;
		channel_states[ch_idx].fade_step = 0.0f;
		channel_states[ch_idx].fade_ticks_left = 0;
	}
}

/**
 * Fade a sound channel's volume over time.
 * Call sound_fade_tick() each game tick to process fades.
 * @param channel   Channel ID from sound_play()
 * @param target    Target volume (0.0 = silent, 1.0 = full)
 * @param duration  Fade duration in milliseconds
 */
void sound_fade(int channel, float target, int duration)
{
	int ch_idx = channel - 1;

	if (ch_idx < 0 || ch_idx >= MAX_SOUND_CHANNELS) {
		return;
	}

	if (!channel_states[ch_idx].in_use) {
		return;
	}

	// Clamp target
	if (target < 0.0f) {
		target = 0.0f;
	}
	if (target > 1.0f) {
		target = 1.0f;
	}

	if (duration <= 0) {
		// Instant change
		sound_set_volume(channel, target);
		return;
	}

	// Calculate fade parameters
	// Assuming 24 ticks per second (game tick rate)
	int ticks = (duration * 24) / 1000;
	if (ticks < 1) {
		ticks = 1;
	}

	float current = channel_states[ch_idx].volume;
	float step = (target - current) / (float)ticks;

	channel_states[ch_idx].fade_target = target;
	channel_states[ch_idx].fade_step = step;
	channel_states[ch_idx].fade_ticks_left = ticks;
}

/**
 * Process fade effects. Call this once per game tick.
 * This is called internally by the sound system.
 */
void sound_fade_tick(void)
{
	for (int i = 0; i < MAX_SOUND_CHANNELS; i++) {
		if (channel_states[i].in_use && channel_states[i].fade_ticks_left > 0) {
			float new_vol = channel_states[i].volume + channel_states[i].fade_step;

			channel_states[i].fade_ticks_left--;

			if (channel_states[i].fade_ticks_left == 0) {
				// Snap to target
				new_vol = channel_states[i].fade_target;
			}

			// Clamp
			if (new_vol < 0.0f) {
				new_vol = 0.0f;
			}
			if (new_vol > 1.0f) {
				new_vol = 1.0f;
			}

			channel_states[i].volume = new_vol;

			// Apply to track
			MIX_Track *track = sdl_tracks[i];
			if (track) {
				float master = sound_get_master_volume();
				MIX_SetTrackGain(track, new_vol * master);
			}

			// If faded to zero and not looping, stop the channel
			if (new_vol <= 0.0f && channel_states[i].fade_ticks_left == 0) {
				sound_stop(i + 1);
			}
		}
	}
}

/**
 * Get the master sound effects volume (from user settings).
 * @return Volume multiplier (0.0 to 1.0)
 */
float sound_get_master_volume(void)
{
	// sound_volume is 0-128 where 128 is full volume
	// Note: The original code interprets this as a negative offset,
	// but for the API we return 0.0-1.0 where 1.0 is full
	if (sound_volume < 0) {
		return 0.0f;
	}
	if (sound_volume > 128) {
		return 1.0f;
	}
	return (float)sound_volume / 128.0f;
}

/**
 * Check if a channel is currently playing.
 * @param channel Channel ID from sound_play()
 * @return        1 if playing, 0 if stopped/invalid
 */
int sound_is_playing(int channel)
{
	int ch_idx = channel - 1;

	if (ch_idx < 0 || ch_idx >= MAX_SOUND_CHANNELS) {
		return 0;
	}

	// Check our state tracking
	if (!channel_states[ch_idx].in_use) {
		return 0;
	}

	// Also verify with SDL3_mixer that it's actually playing
	MIX_Track *track = sdl_tracks[ch_idx];
	if (track && MIX_TrackPlaying(track)) {
		return 1;
	}

	// Track finished - update our state
	channel_states[ch_idx].in_use = 0;
	return 0;
}

/**
 * Check if sound is enabled in user settings.
 * @return 1 if sound enabled, 0 if disabled
 */
int sound_is_enabled(void)
{
	return (game_options & GO_SOUND) ? 1 : 0;
}

/**
 * Cleanup all mod sounds. Called on exit.
 */
void sound_cleanup_mod_sounds(void)
{
	// Stop all playing channels
	sound_stop_all();

	// Free all mod sounds
	for (int i = 1; i < MAX_MOD_SOUNDS; i++) {
		if (mod_sounds[i]) {
			MIX_DestroyAudio(mod_sounds[i]);
			mod_sounds[i] = NULL;
		}
	}
	mod_sound_count = 0;

	// Reset channel states
	memset(channel_states, 0, sizeof(channel_states));
}
