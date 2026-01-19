/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 */

#include "../dll.h"
#include "../astonia.h"
#include "amod_structs.h"
#include <SDL3/SDL_keycode.h>

DLL_EXPORT void amod_init(void);
DLL_EXPORT void amod_exit(void);
DLL_EXPORT char *amod_version(void);
DLL_EXPORT void amod_gamestart(void);
DLL_EXPORT void amod_sprite_config(void);
DLL_EXPORT void amod_areachange(void);
DLL_EXPORT void amod_frame(void);
DLL_EXPORT void amod_tick(void);
DLL_EXPORT void amod_mouse_move(int x, int y);
DLL_EXPORT void amod_mouse_capture(int onoff);
DLL_EXPORT void amod_update_hover_texts(void);

// the following functions should return 1 if they process the event and want the client
// and all later mods to ignore it.
// return -1 if you want the client to ignore it, but allow other mods to process it.
// return 0 otherwise
DLL_EXPORT int amod_mouse_click(int x, int y, int what);
DLL_EXPORT int amod_keydown(SDL_Keycode key); // if you catch keydown ...
DLL_EXPORT int amod_keyup(SDL_Keycode key); // ... you must also catch keyup
DLL_EXPORT int amod_client_cmd(const char *buf);

// main mod only:
DLL_EXPORT int amod_process(const unsigned char *buf); // return length of server command, 0 = unknown
DLL_EXPORT int amod_prefetch(const unsigned char *buf); // return length of server command, 0 = unknown
DLL_EXPORT int amod_display_skill_line(int v, int base, int curr, int cn, char *buf);
DLL_EXPORT int amod_is_playersprite(int sprite);

// --------- Client exported functions -----------

// basics
DLL_IMPORT int note(const char *format, ...) __attribute__((format(printf, 1, 2)));
DLL_IMPORT int warn(const char *format, ...) __attribute__((format(printf, 1, 2)));
DLL_IMPORT int fail(const char *format, ...) __attribute__((format(printf, 1, 2)));
DLL_IMPORT void paranoia(const char *format, ...) __attribute__((format(printf, 1, 2)));
DLL_IMPORT void addline(const char *format, ...) __attribute__((format(printf, 1, 2)));
// game, drawing
DLL_IMPORT void render_push_clip(void);
DLL_IMPORT void render_pop_clip(void);
DLL_IMPORT void render_more_clip(int sx, int sy, int ex, int ey);
DLL_IMPORT void render_sprite(int sprite, int scrx, int scry, int light, int align);
DLL_IMPORT int render_sprite_fx(RenderFX *ddfx, int scrx, int scry);
DLL_IMPORT void render_rect(int sx, int sy, int ex, int ey, unsigned short int color);
DLL_IMPORT void render_line(int fx, int fy, int tx, int ty, unsigned short col);
DLL_IMPORT int render_text_length(int flags, const char *text);
DLL_IMPORT int render_text(int sx, int sy, unsigned short int color, int flags, const char *text);
DLL_IMPORT int render_text_break(int x, int y, int breakx, unsigned short color, int flags, const char *ptr);
DLL_IMPORT int render_text_break_length(int x, int y, int breakx, unsigned short color, int flags, const char *ptr);
DLL_IMPORT void render_pixel(int x, int y, unsigned short col);
DLL_IMPORT int render_text_fmt(int sx, int sy, unsigned short int color, int flags, const char *format, ...);
DLL_IMPORT int render_text_break_fmt(
    int sx, int sy, int breakx, unsigned short int color, int flags, const char *format, ...);
DLL_IMPORT int render_text_nl(int x, int y, int unsigned short color, int flags, const char *ptr);
// gui, dots and buttons
DLL_IMPORT int dotx(int didx);
DLL_IMPORT int doty(int didx);
DLL_IMPORT int butx(int bidx);
DLL_IMPORT int buty(int bidx);
// gui helperls
DLL_IMPORT size_t get_near_ground(int x, int y);
DLL_IMPORT size_t get_near_item(int x, int y, unsigned int flag, unsigned int looksize);
DLL_IMPORT size_t get_near_char(int x, int y, unsigned int looksize);
DLL_IMPORT map_index_t mapmn(unsigned int x, unsigned int y);
// misc
DLL_IMPORT void set_teleport(int idx, int x, int y);
DLL_IMPORT int exp2level(int val);
DLL_IMPORT int level2exp(int level);
DLL_IMPORT int mil_rank(int exp);
// client / server communication
DLL_IMPORT void client_send(void *buf, size_t len);

// Sound functions
// Sounds loaded from: sx_mod.zip > sx_patch.zip > sx.zip
// Example: sound_load("weather/rain_loop.ogg")
DLL_IMPORT int sound_load(const char *path);
DLL_IMPORT void sound_unload(int handle);

/* Playback */
DLL_IMPORT int sound_play(int handle, float volume);
DLL_IMPORT int sound_play_loop(int handle, float volume);
DLL_IMPORT void sound_stop(int channel);
DLL_IMPORT void sound_stop_all(void);

/* Volume control */
DLL_IMPORT void sound_set_volume(int channel, float volume);
DLL_IMPORT void sound_fade(int channel, float target, int duration);
DLL_IMPORT float sound_get_master_volume(void);

/* Query */
DLL_IMPORT int sound_is_playing(int channel);
DLL_IMPORT int sound_is_enabled(void);

// Sprite config - load custom variants in amod_sprite_config()
DLL_IMPORT int sprite_config_load_characters(const char *path);
DLL_IMPORT int sprite_config_load_animated(const char *path);
DLL_IMPORT int sprite_config_load_metadata(const char *path);


// ---------- Client exported data structures -------------
DLL_IMPORT extern int skltab_cnt;
DLL_IMPORT extern struct skltab *skltab;
DLL_IMPORT extern int item_flags[];
DLL_IMPORT extern int weatab[];
DLL_IMPORT int act;
DLL_IMPORT int actx;
DLL_IMPORT int acty;
DLL_IMPORT extern int __yres;

DLL_IMPORT int vk_shift, vk_control, vk_alt;

DLL_IMPORT unsigned int cflags; // current item flags
DLL_IMPORT unsigned int csprite; // and sprite

DLL_IMPORT int originx;
DLL_IMPORT int originy;
DLL_IMPORT struct map map[MAPDX * MAPDY];
DLL_IMPORT struct map map2[MAPDX * MAPDY];

DLL_IMPORT int value[2][V_MAX];
DLL_IMPORT int item[MAX_INVENTORYSIZE];
DLL_IMPORT int item_flags[MAX_INVENTORYSIZE];
DLL_IMPORT int hp;
DLL_IMPORT int mana;
DLL_IMPORT int rage;
DLL_IMPORT int endurance;
DLL_IMPORT int lifeshield;
DLL_IMPORT int experience;
DLL_IMPORT int experience_used;
DLL_IMPORT int mil_exp;
DLL_IMPORT int gold;
DLL_IMPORT struct player player[MAXCHARS];
DLL_IMPORT union ceffect ceffect[MAXEF];
DLL_IMPORT unsigned char ueffect[MAXEF];
DLL_IMPORT int con_type;
DLL_IMPORT char con_name[80];
DLL_IMPORT int con_cnt;
DLL_IMPORT int container[MAX_CONTAINERSIZE];
DLL_IMPORT int price[MAX_CONTAINERSIZE];
DLL_IMPORT int itemprice[MAX_CONTAINERSIZE];
DLL_IMPORT int cprice;
DLL_IMPORT int lookinv[12];
DLL_IMPORT int looksprite, lookc1, lookc2, lookc3;
DLL_IMPORT char look_name[80];
DLL_IMPORT char look_desc[1024];
DLL_IMPORT char pent_str[7][80];
DLL_IMPORT int pspeed;
DLL_IMPORT int protocol_version;

DLL_IMPORT unsigned short int healthcolor, manacolor, endurancecolor, shieldcolor;
DLL_IMPORT unsigned short int whitecolor, lightgraycolor, graycolor, darkgraycolor, blackcolor;
DLL_IMPORT unsigned short int lightredcolor, redcolor, darkredcolor;
DLL_IMPORT unsigned short int lightgreencolor, greencolor, darkgreencolor;
DLL_IMPORT unsigned short int lightbluecolor, bluecolor, darkbluecolor;
DLL_IMPORT unsigned short int lightorangecolor, orangecolor, darkorangecolor;
DLL_IMPORT unsigned short int textcolor;

DLL_IMPORT struct quest quest[MAXQUEST];
DLL_IMPORT struct shrine_ppd shrine;

DLL_IMPORT char hover_bless_text[120];
DLL_IMPORT char hover_freeze_text[120];
DLL_IMPORT char hover_potion_text[120];
DLL_IMPORT char hover_rage_text[120];
DLL_IMPORT char hover_level_text[120];
DLL_IMPORT char hover_rank_text[120];
DLL_IMPORT char hover_time_text[120];

DLL_IMPORT char *target_server;

DLL_IMPORT char password[16];
DLL_IMPORT char username[40];
DLL_IMPORT int tick;
DLL_IMPORT int mirror;
DLL_IMPORT int realtime;
DLL_IMPORT char server_url[256];
DLL_IMPORT int server_port;
DLL_IMPORT int want_width;
DLL_IMPORT int want_height;
DLL_IMPORT int sdl_scale;
DLL_IMPORT int sdl_frames;
DLL_IMPORT int sdl_multi;
DLL_IMPORT int sdl_cache_size;
DLL_IMPORT int frames_per_second;
DLL_IMPORT uint64_t game_options;
DLL_IMPORT int game_slowdown;


// ---------------- override-able functions, also exported from client ----------------
DLL_IMPORT int _is_cut_sprite(unsigned int sprite);
DLL_IMPORT int _is_mov_sprite(unsigned int sprite, int itemhint);
DLL_IMPORT int _is_door_sprite(unsigned int sprite);
DLL_IMPORT int _is_yadd_sprite(unsigned int sprite);
DLL_IMPORT int _get_chr_height(unsigned int csprite);
DLL_IMPORT unsigned int _trans_asprite(map_index_t mn, unsigned int sprite, tick_t attick, unsigned char *pscale,
    unsigned char *pcr, unsigned char *pcg, unsigned char *pcb, unsigned char *plight, unsigned char *psat,
    unsigned short *pc1, unsigned short *pc2, unsigned short *pc3, unsigned short *pshine);
DLL_IMPORT int _trans_charno(int csprite, int *pscale, int *pcr, int *pcg, int *pcb, int *plight, int *psat, int *pc1,
    int *pc2, int *pc3, int *pshine, int attick);
DLL_IMPORT int _get_player_sprite(int nr, int zdir, int action, int step, int duration, int attick);
DLL_IMPORT void _trans_csprite(map_index_t mn, struct map *cmap, tick_t attick);
DLL_IMPORT int _get_lay_sprite(int sprite, int lay);
DLL_IMPORT int _get_offset_sprite(int sprite, int *px, int *py);
DLL_IMPORT int _additional_sprite(unsigned int sprite, int attick);
DLL_IMPORT int _opt_sprite(int sprite);
DLL_IMPORT int _no_lighting_sprite(unsigned int sprite);
DLL_IMPORT int _get_skltab_sep(int i);
DLL_IMPORT int _get_skltab_index(int n);
DLL_IMPORT int _get_skltab_show(int i);
DLL_IMPORT int _do_display_random(void);
DLL_IMPORT int _do_display_help(int nr);

// ------------ declarations for functions the mod might provide -------------------
DLL_EXPORT int is_cut_sprite(unsigned int sprite);
DLL_EXPORT int is_mov_sprite(unsigned int sprite, int itemhint);
DLL_EXPORT int is_door_sprite(unsigned int sprite);
DLL_EXPORT int is_yadd_sprite(unsigned int sprite);
DLL_EXPORT int get_chr_height(unsigned int csprite);
DLL_EXPORT unsigned int trans_asprite(map_index_t mn, unsigned int sprite, tick_t attick, unsigned char *pscale,
    unsigned char *pcr, unsigned char *pcg, unsigned char *pcb, unsigned char *plight, unsigned char *psat,
    unsigned short *pc1, unsigned short *pc2, unsigned short *pc3, unsigned short *pshine);
DLL_EXPORT int trans_charno(int csprite, int *pscale, int *pcr, int *pcg, int *pcb, int *plight, int *psat, int *pc1,
    int *pc2, int *pc3, int *pshine, int attick);
DLL_EXPORT int get_player_sprite(int nr, int zdir, int action, int step, int duration, int attick);
DLL_EXPORT void trans_csprite(int mn, struct map *cmap, int attick);
DLL_EXPORT int get_lay_sprite(int sprite, int lay);
DLL_EXPORT int get_offset_sprite(int sprite, int *px, int *py);
DLL_EXPORT int additional_sprite(unsigned int sprite, int attick);
DLL_EXPORT int opt_sprite(unsigned int sprite);
DLL_EXPORT int no_lighting_sprite(unsigned int sprite);
DLL_EXPORT int get_skltab_sep(int i);
DLL_EXPORT int get_skltab_index(int n);
DLL_EXPORT int get_skltab_show(int i);
DLL_EXPORT int do_display_random(void);
DLL_EXPORT int do_display_help(int nr);
