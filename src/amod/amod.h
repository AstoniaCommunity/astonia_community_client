/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * Mod SDK Header - API for creating client modifications
 */

#include "../dll.h"
#include "../astonia.h"
#include "amod_structs.h"
#include <SDL3/SDL_keycode.h>

// =====================================================================
// Type Aliases - Match client's semantic types
// =====================================================================
typedef uint32_t tick_t; // SDL ticks, timestamps
typedef uint16_t stat_t; // Character stats: hp, mana, rage, endurance, lifeshield
typedef size_t map_index_t; // Map tile index, selection indices

// =====================================================================
// Mod Entry Points
// =====================================================================
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

// Event handlers - return values:
//   1  = processed, client and later mods should ignore
//  -1  = client should ignore, but allow other mods to process
//   0  = not processed, continue normal handling
DLL_EXPORT int amod_mouse_click(int x, int y, int what);
DLL_EXPORT int amod_keydown(SDL_Keycode key);
DLL_EXPORT int amod_keyup(SDL_Keycode key);
DLL_EXPORT int amod_client_cmd(const char *buf);

// Main mod only:
DLL_EXPORT int amod_process(const unsigned char *buf);
DLL_EXPORT int amod_prefetch(const unsigned char *buf);
DLL_EXPORT int amod_display_skill_line(int v, int base, int curr, int cn, char *buf);
DLL_EXPORT int amod_is_playersprite(int sprite);

// =====================================================================
// Client Exported Functions - Call these from your mod
// =====================================================================

// --- Logging ---
DLL_IMPORT int note(const char *format, ...) __attribute__((format(printf, 1, 2)));
DLL_IMPORT int warn(const char *format, ...) __attribute__((format(printf, 1, 2)));
DLL_IMPORT int fail(const char *format, ...) __attribute__((format(printf, 1, 2)));
DLL_IMPORT void paranoia(const char *format, ...) __attribute__((format(printf, 1, 2)));
DLL_IMPORT void addline(const char *format, ...) __attribute__((format(printf, 1, 2)));

// --- Chat ---
DLL_IMPORT void cmd_add_text(const char *buf, int typ);

// --- Render: Clipping ---
DLL_IMPORT void render_push_clip(void);
DLL_IMPORT void render_pop_clip(void);
DLL_IMPORT void render_more_clip(int sx, int sy, int ex, int ey);
DLL_IMPORT void render_set_clip(int sx, int sy, int ex, int ey);
DLL_IMPORT void render_clear_clip(void);
DLL_IMPORT void render_get_clip(int *out_start_x, int *out_start_y, int *out_end_x, int *out_end_y);

// --- Render: Sprites ---
DLL_IMPORT void render_sprite(unsigned int sprite, int scrx, int scry, char light, char align);
DLL_IMPORT int render_sprite_fx(RenderFX *fx, int scrx, int scry);

// --- Render: Basic Primitives ---
DLL_IMPORT void render_pixel(int x, int y, unsigned short col);
DLL_IMPORT void render_line(int fx, int fy, int tx, int ty, unsigned short col);
DLL_IMPORT void render_rect(int sx, int sy, int ex, int ey, unsigned short int color);

// --- Render: Alpha Primitives ---
DLL_IMPORT void render_pixel_alpha(int x, int y, unsigned short col, unsigned char alpha);
DLL_IMPORT void render_line_alpha(int fx, int fy, int tx, int ty, unsigned short col, unsigned char alpha);
DLL_IMPORT void render_line_aa(int x0, int y0, int x1, int y1, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_rect_alpha(int sx, int sy, int ex, int ey, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_rect_outline_alpha(int sx, int sy, int ex, int ey, unsigned short color, unsigned char alpha);

// --- Render: Shapes with Alpha ---
DLL_IMPORT void render_circle_alpha(int cx, int cy, int radius, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_circle_filled_alpha(int cx, int cy, int radius, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_ellipse_alpha(int cx, int cy, int rx, int ry, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_ellipse_filled_alpha(int cx, int cy, int rx, int ry, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_triangle_alpha(
    int x0, int y0, int x1, int y1, int x2, int y2, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_triangle_filled_alpha(
    int x0, int y0, int x1, int y1, int x2, int y2, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_rounded_rect_alpha(
    int sx, int sy, int ex, int ey, int radius, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_rounded_rect_filled_alpha(
    int sx, int sy, int ex, int ey, int radius, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_ring_alpha(int cx, int cy, int inner_radius, int outer_radius, int start_angle, int end_angle,
    unsigned short color, unsigned char alpha);
DLL_IMPORT void render_arc_alpha(
    int cx, int cy, int radius, int start_angle, int end_angle, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_thick_line_alpha(
    int x0, int y0, int x1, int y1, int thickness, unsigned short color, unsigned char alpha);

// --- Render: Curves ---
DLL_IMPORT void render_bezier_quadratic_alpha(
    int x0, int y0, int x1, int y1, int x2, int y2, unsigned short color, unsigned char alpha);
DLL_IMPORT void render_bezier_cubic_alpha(
    int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, unsigned short color, unsigned char alpha);

// --- Render: Gradients ---
DLL_IMPORT void render_gradient_rect_h(
    int sx, int sy, int ex, int ey, unsigned short color1, unsigned short color2, unsigned char alpha);
DLL_IMPORT void render_gradient_rect_v(
    int sx, int sy, int ex, int ey, unsigned short color1, unsigned short color2, unsigned char alpha);
DLL_IMPORT void render_gradient_circle(
    int cx, int cy, int radius, unsigned short inner_color, unsigned short outer_color, unsigned char alpha);

// --- Render: Screen Effects ---
DLL_IMPORT void render_vignette(unsigned char intensity);
DLL_IMPORT void render_screen_flash(unsigned short color, unsigned char intensity);
DLL_IMPORT void render_screen_tint(unsigned short color, unsigned char intensity);

// --- Render: Blend Mode ---
DLL_IMPORT void render_set_blend_mode(int mode);
DLL_IMPORT int render_get_blend_mode(void);

// --- Render: Textures ---
DLL_IMPORT int render_load_texture(const char *path);
DLL_IMPORT void render_unload_texture(int tex_id);
DLL_IMPORT void render_texture(int tex_id, int x, int y, unsigned char alpha);
DLL_IMPORT void render_texture_scaled(int tex_id, int x, int y, float scale, unsigned char alpha);
DLL_IMPORT int render_texture_width(int tex_id);
DLL_IMPORT int render_texture_height(int tex_id);

// --- Render: Render Targets ---
DLL_IMPORT int render_create_target(int width, int height);
DLL_IMPORT void render_destroy_target(int target_id);
DLL_IMPORT int render_set_target(int target_id);
DLL_IMPORT void render_clear_target(int target_id);
DLL_IMPORT void render_target_to_screen(int target_id, int x, int y, unsigned char alpha);

// --- Render: Text ---
DLL_IMPORT int render_text_length(int flags, const char *text);
DLL_IMPORT int render_text(int sx, int sy, unsigned short int color, int flags, const char *text);
DLL_IMPORT int render_text_fmt(int64_t sx, int64_t sy, unsigned short int color, int flags, const char *format, ...)
    __attribute__((format(printf, 5, 6)));
DLL_IMPORT int render_text_break(int x, int y, int breakx, unsigned short color, int flags, const char *ptr);
DLL_IMPORT int render_text_break_fmt(int sx, int sy, int breakx, unsigned short int color, int flags,
    const char *format, ...) __attribute__((format(printf, 6, 7)));
DLL_IMPORT int render_text_break_length(int x, int y, int breakx, unsigned short color, int flags, const char *ptr);
DLL_IMPORT int render_text_nl(int x, int y, unsigned short color, int flags, const char *ptr);

// --- GUI: Dots and Buttons ---
DLL_IMPORT int dotx(int didx);
DLL_IMPORT int doty(int didx);
DLL_IMPORT int butx(int bidx);
DLL_IMPORT int buty(int bidx);

// --- GUI: Selection Helpers ---
DLL_IMPORT size_t get_near_ground(int x, int y);
DLL_IMPORT map_index_t get_near_item(int x, int y, unsigned int flag, unsigned int looksize);
DLL_IMPORT map_index_t get_near_char(int x, int y, unsigned int looksize);
DLL_IMPORT map_index_t mapmn(unsigned int x, unsigned int y);

// --- Misc ---
DLL_IMPORT void set_teleport(int idx, int x, int y);
DLL_IMPORT int exp2level(int val);
DLL_IMPORT int level2exp(int level);
DLL_IMPORT int mil_rank(int exp);

// --- Client/Server Communication ---
DLL_IMPORT void client_send(void *buf, size_t len);

// --- Sound ---
// Sounds loaded from: sx_mod.zip > sx_patch.zip > sx.zip
// Example: sound_load("weather/rain_loop.ogg")
DLL_IMPORT int sound_load(const char *path);
DLL_IMPORT void sound_unload(int handle);
DLL_IMPORT int sound_play(int handle, float volume);
DLL_IMPORT int sound_play_loop(int handle, float volume);
DLL_IMPORT void sound_stop(int channel);
DLL_IMPORT void sound_stop_all(void);
DLL_IMPORT void sound_set_volume(int channel, float volume);
DLL_IMPORT void sound_fade(int channel, float target, int duration);
DLL_IMPORT float sound_get_master_volume(void);
DLL_IMPORT int sound_is_playing(int channel);
DLL_IMPORT int sound_is_enabled(void);

// --- Sprite Config ---
// Load custom sprite configurations in amod_sprite_config()
DLL_IMPORT int sprite_config_load_characters(const char *path);
DLL_IMPORT int sprite_config_load_animated(const char *path);
DLL_IMPORT int sprite_config_load_metadata(const char *path);

// =====================================================================
// Client Exported Variables
// =====================================================================

// --- Skill Table ---
DLL_IMPORT extern int skltab_cnt;
DLL_IMPORT extern struct skltab *skltab;
DLL_IMPORT extern int weatab[];

// --- Input State ---
DLL_IMPORT int vk_shift, vk_control, vk_alt;

// --- Current Selection ---
DLL_IMPORT unsigned int cflags;
DLL_IMPORT unsigned int csprite;
DLL_IMPORT uint16_t act;
DLL_IMPORT uint16_t actx;
DLL_IMPORT uint16_t acty;

// --- Map Data ---
DLL_IMPORT uint16_t originx;
DLL_IMPORT uint16_t originy;
DLL_IMPORT struct map map[MAPDX * MAPDY];
DLL_IMPORT struct map map2[MAPDX * MAPDY];

// --- Character Stats ---
DLL_IMPORT uint16_t value[2][V_MAX];
DLL_IMPORT stat_t hp;
DLL_IMPORT stat_t mana;
DLL_IMPORT stat_t rage;
DLL_IMPORT stat_t endurance;
DLL_IMPORT stat_t lifeshield;
DLL_IMPORT uint32_t experience;
DLL_IMPORT uint32_t experience_used;
DLL_IMPORT uint32_t mil_exp;
DLL_IMPORT uint32_t gold;

// --- Inventory ---
DLL_IMPORT uint32_t item[MAX_INVENTORYSIZE];
DLL_IMPORT uint32_t item_flags[MAX_INVENTORYSIZE];

// --- Container ---
DLL_IMPORT int con_type;
DLL_IMPORT char con_name[80];
DLL_IMPORT int con_cnt;
DLL_IMPORT uint32_t container[MAX_CONTAINERSIZE];
DLL_IMPORT uint32_t price[MAX_CONTAINERSIZE];
DLL_IMPORT uint32_t itemprice[MAX_CONTAINERSIZE];
DLL_IMPORT uint32_t cprice;

// --- Look Window ---
DLL_IMPORT uint32_t lookinv[12];
DLL_IMPORT uint32_t looksprite, lookc1, lookc2, lookc3;
DLL_IMPORT char look_name[80];
DLL_IMPORT char look_desc[1024];

// --- Players and Effects ---
DLL_IMPORT struct player player[MAXCHARS];
DLL_IMPORT union ceffect ceffect[MAXEF];
DLL_IMPORT unsigned char ueffect[MAXEF];

// --- Pents ---
DLL_IMPORT char pent_str[7][80];
DLL_IMPORT int pspeed;

// --- Protocol ---
DLL_IMPORT int protocol_version;

// --- Colors ---
DLL_IMPORT unsigned short int healthcolor, manacolor, endurancecolor, shieldcolor;
DLL_IMPORT unsigned short int whitecolor, lightgraycolor, graycolor, darkgraycolor, blackcolor;
DLL_IMPORT unsigned short int lightredcolor, redcolor, darkredcolor;
DLL_IMPORT unsigned short int lightgreencolor, greencolor, darkgreencolor;
DLL_IMPORT unsigned short int lightbluecolor, bluecolor, darkbluecolor;
DLL_IMPORT unsigned short int lightorangecolor, orangecolor, darkorangecolor;
DLL_IMPORT unsigned short int textcolor;

// --- Quest System ---
DLL_IMPORT struct quest quest[MAXQUEST];
DLL_IMPORT struct shrine_ppd shrine;
DLL_IMPORT extern struct questlog *game_questlog;
DLL_IMPORT extern int *game_questcount;

// --- Hover Texts ---
DLL_IMPORT char hover_bless_text[120];
DLL_IMPORT char hover_freeze_text[120];
DLL_IMPORT char hover_heal_text[120];
DLL_IMPORT char hover_potion_text[120];
DLL_IMPORT char hover_rage_text[120];
DLL_IMPORT char hover_level_text[120];
DLL_IMPORT char hover_rank_text[120];
DLL_IMPORT char hover_time_text[120];

// --- Connection Info ---
DLL_IMPORT char *target_server;
DLL_IMPORT char password[16];
DLL_IMPORT char username[40];
DLL_IMPORT char server_url[256];
DLL_IMPORT int server_port;

// --- Timing ---
DLL_IMPORT tick_t tick;
DLL_IMPORT uint32_t mirror;
DLL_IMPORT uint32_t realtime;
DLL_IMPORT int frames_per_second;

// --- Display Settings ---
DLL_IMPORT extern int __yres;
DLL_IMPORT int want_width;
DLL_IMPORT int want_height;
DLL_IMPORT int sdl_scale;
DLL_IMPORT int sdl_frames;
DLL_IMPORT int sdl_multi;
DLL_IMPORT int sdl_cache_size;

// --- Game Options ---
DLL_IMPORT uint64_t game_options;
DLL_IMPORT int game_slowdown;

// =====================================================================
// Override-able Functions - Client defaults, call with underscore prefix
// =====================================================================
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
DLL_IMPORT unsigned int _opt_sprite(unsigned int sprite);
DLL_IMPORT int _no_lighting_sprite(unsigned int sprite);
DLL_IMPORT int _get_skltab_sep(int i);
DLL_IMPORT int _get_skltab_index(int n);
DLL_IMPORT int _get_skltab_show(int i);
DLL_IMPORT int _do_display_random(void);
DLL_IMPORT int _do_display_help(int nr);

// =====================================================================
// Mod-Provided Overrides - Implement these to customize behavior
// =====================================================================
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
