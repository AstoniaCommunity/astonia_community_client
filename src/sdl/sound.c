/*
 * Part of Astonia Client. Please read license.txt.
 *
 * Sound
 *
 * Loads and plays sounds via SDL2 library.
 */

#include <stdio.h>
#include <zip.h>
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include "astonia.h"
#include "sdl/sdl.h"
#include "sdl/sdl_private.h"

// Sound definitions
static char *sfx_name[] = {
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
    "sfx/flash.wav", // 30	lightning strike
    "sfx/scarynote.wav", // 31	freeze
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
    "sfx/whale3.wav" // 49
};
static int sfx_name_cnt = ARRAYSIZE(sfx_name);

int sound_volume = 128;
static uint64_t time_play_sound = 0;

static MIX_Audio *sound_effect[MAXSOUND];

MIX_Audio *load_sound_from_zip(zip_t *zip_archive, const char *filename);

int init_sound(void)
{
	int i, err;
	zip_t *sz;

	if (!(game_options & GO_SOUND)) {
		return -1;
	}

	sz = zip_open("res/sx.zip", ZIP_RDONLY, &err);
	if (!sz) {
		warn("Opening sx.zip failed with error code %d.", err);
		game_options &= ~GO_SOUND;
		return -1;
	}

	// Load all sound effects from the zip archive
	int max_sfx_idx = min(sfx_name_cnt, MAXSOUND);
	for (i = 1; i < max_sfx_idx; i++) {
		sound_effect[i] = load_sound_from_zip(sz, sfx_name[i]);
	}
	zip_close(sz);

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

	// Get file stats from zip
	if (zip_stat(zip_archive, filename, 0, &stat) != 0 || !(stat.valid & ZIP_STAT_SIZE)) {
		warn("Could not stat sound file %s in archive.", filename);
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
		warn("Could not open sound file %s in archive.", filename);
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

	// Free all sound effects
	// Starting at 1 since 0 is null
	for (i = 1; i < sfx_name_cnt; i++) {
		MIX_DestroyAudio(sound_effect[i]);
		sound_effect[i] = NULL;
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

	if (nr < 1U || nr >= (unsigned int)sfx_name_cnt || nr >= (unsigned int)MAXSOUND) {
		return;
	}

	if (!sound_effect[nr]) {
		return; // Audio not loaded
	}

	// For debugging/optimization
	time_start = SDL_GetTicks();

#if 0
	note("nr = %d: %s, distance = %d, angle = %d", nr, sfx_name[nr], distance, angle);
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
