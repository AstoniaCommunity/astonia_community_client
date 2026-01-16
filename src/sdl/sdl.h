/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 */

#include <stdbool.h>

#define SDL_CUR_c_only    1
#define SDL_CUR_c_take    2
#define SDL_CUR_c_drop    3
#define SDL_CUR_c_attack  4
#define SDL_CUR_c_raise   5
#define SDL_CUR_c_give    6
#define SDL_CUR_c_use     7
#define SDL_CUR_c_usewith 8
#define SDL_CUR_c_swap    9
#define SDL_CUR_c_sell    10
#define SDL_CUR_c_buy     11
#define SDL_CUR_c_look    12
#define SDL_CUR_c_set     13
#define SDL_CUR_c_spell   14
#define SDL_CUR_c_pix     15
#define SDL_CUR_c_say     16
#define SDL_CUR_c_junk    17
#define SDL_CUR_c_get     18

#define SDL_KEYM_SHIFT 1
#define SDL_KEYM_CTRL  2
#define SDL_KEYM_ALT   4

#define SDL_MOUM_NONE  0
#define SDL_MOUM_LUP   1
#define SDL_MOUM_LDOWN 2
#define SDL_MOUM_RUP   3
#define SDL_MOUM_RDOWN 4
#define SDL_MOUM_MUP   5
#define SDL_MOUM_MDOWN 6
#define SDL_MOUM_WHEEL 7

struct renderfont;
typedef struct renderfont RenderFont;

DLL_EXPORT extern int sdl_cache_size;
DLL_EXPORT extern int sdl_scale;
DLL_EXPORT extern int sdl_frames;
DLL_EXPORT extern int sdl_multi;

extern int sound_volume;

void sdl_set_cursor(int cursor);
int init_sound(void);
void sound_exit(void);
void play_sound(unsigned int nr, int vol, int p);

void sdl_bargraph_add(int dx, unsigned char *data, int val);
void sdl_bargraph(int sx, int sy, int dx, unsigned char *data, int x_offset, int y_offset);
bool sdl_has_focus(void);
bool sdl_is_shown(void);
void sdl_set_cursor_pos(int x, int y);
void sdl_capture_mouse(int flag);
int sdl_tx_load(unsigned int sprite, signed char sink, unsigned char freeze, unsigned char scale, char cr, char cg,
    char cb, char light, char sat, int c1, int c2, int c3, int shine, char ml, char ll, char rl, char ul, char dl,
    const char *text, int text_color, int text_flags, void *text_font, int checkonly, int preload);
int sdl_init(int width, int height, char *title, int monitor);
void sdl_exit(void);
void sdl_loop(void);
int sdl_clear(void);
int sdl_render(void);
int sdlt_xoff(int cache_index);
int sdlt_yoff(int cache_index);
int sdlt_xres(int cache_index);
int sdlt_yres(int cache_index);
void sdl_blit(
    int cache_index, int sx, int sy, int clipsx, int clipsy, int clipex, int clipey, int x_offset, int y_offset);
int sdl_drawtext(int sx, int sy, unsigned short int color, int flags, const char *text, struct renderfont *font,
    int clipsx, int clipsy, int clipex, int clipey, int x_offset, int y_offset);
// Basic drawing primitives
void sdl_pixel(int x, int y, unsigned short color, int x_offset, int y_offset);
void sdl_pixel_alpha(int x, int y, unsigned short color, unsigned char alpha, int x_offset, int y_offset);
void sdl_line(int fx, int fy, int tx, int ty, unsigned short color, int clipsx, int clipsy, int clipex, int clipey,
    int x_offset, int y_offset);
void sdl_line_alpha(int fx, int fy, int tx, int ty, unsigned short color, unsigned char alpha, int clipsx, int clipsy,
    int clipex, int clipey, int x_offset, int y_offset);
void sdl_line_aa(int x0, int y0, int x1, int y1, unsigned short color, unsigned char alpha, int x_offset, int y_offset);
void sdl_thick_line_alpha(int fx, int fy, int tx, int ty, int thickness, unsigned short color, unsigned char alpha,
    int clipsx, int clipsy, int clipex, int clipey, int x_offset, int y_offset);

// Rectangle primitives
void sdl_rect(int sx, int sy, int ex, int ey, unsigned short int color, int clipsx, int clipsy, int clipex, int clipey,
    int x_offset, int y_offset);
void sdl_shaded_rect(int sx, int sy, int ex, int ey, unsigned short int color, unsigned short alpha, int clipsx,
    int clipsy, int clipex, int clipey, int x_offset, int y_offset);
void sdl_rect_outline_alpha(int sx, int sy, int ex, int ey, unsigned short color, unsigned char alpha, int clipsx,
    int clipsy, int clipex, int clipey, int x_offset, int y_offset);
void sdl_rounded_rect_alpha(int sx, int sy, int ex, int ey, int radius, unsigned short color, unsigned char alpha,
    int clipsx, int clipsy, int clipex, int clipey, int x_offset, int y_offset);
void sdl_rounded_rect_filled_alpha(int sx, int sy, int ex, int ey, int radius, unsigned short color,
    unsigned char alpha, int clipsx, int clipsy, int clipex, int clipey, int x_offset, int y_offset);

// Circle and ellipse primitives
void sdl_render_circle(int32_t centreX, int32_t centreY, int32_t radius, uint32_t color);
void sdl_circle_alpha(
    int cx, int cy, int radius, unsigned short color, unsigned char alpha, int x_offset, int y_offset);
void sdl_circle_filled_alpha(
    int cx, int cy, int radius, unsigned short color, unsigned char alpha, int x_offset, int y_offset);
void sdl_ellipse_alpha(
    int cx, int cy, int rx, int ry, unsigned short color, unsigned char alpha, int x_offset, int y_offset);
void sdl_ellipse_filled_alpha(
    int cx, int cy, int rx, int ry, unsigned short color, unsigned char alpha, int x_offset, int y_offset);
void sdl_ring_alpha(int cx, int cy, int inner_radius, int outer_radius, int start_angle, int end_angle,
    unsigned short color, unsigned char alpha, int x_offset, int y_offset);

// Triangle primitives
void sdl_triangle_alpha(int x1, int y1, int x2, int y2, int x3, int y3, unsigned short color, unsigned char alpha,
    int clipsx, int clipsy, int clipex, int clipey, int x_offset, int y_offset);
void sdl_triangle_filled_alpha(int x1, int y1, int x2, int y2, int x3, int y3, unsigned short color,
    unsigned char alpha, int clipsx, int clipsy, int clipex, int clipey, int x_offset, int y_offset);

// Arc and curve primitives
void sdl_arc_alpha(int cx, int cy, int radius, int start_angle, int end_angle, unsigned short color,
    unsigned char alpha, int x_offset, int y_offset);
void sdl_bezier_quadratic_alpha(int x0, int y0, int x1, int y1, int x2, int y2, unsigned short color,
    unsigned char alpha, int x_offset, int y_offset);
void sdl_bezier_cubic_alpha(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, unsigned short color,
    unsigned char alpha, int x_offset, int y_offset);

// Gradient primitives
void sdl_gradient_rect_h(int sx, int sy, int ex, int ey, unsigned short color1, unsigned short color2,
    unsigned char alpha, int clipsx, int clipsy, int clipex, int clipey, int x_offset, int y_offset);
void sdl_gradient_rect_v(int sx, int sy, int ex, int ey, unsigned short color1, unsigned short color2,
    unsigned char alpha, int clipsx, int clipsy, int clipex, int clipey, int x_offset, int y_offset);
void sdl_gradient_circle(int cx, int cy, int radius, unsigned short color, unsigned char center_alpha,
    unsigned char edge_alpha, int x_offset, int y_offset);

// Blend mode control
void sdl_set_blend_mode(int mode);
int sdl_get_blend_mode(void);
void sdl_reset_blend_mode(void);

// Texture utilities
DLL_EXPORT uint32_t *sdl_load_png(char *filename, int *dx, int *dy);
void sdl_set_title(char *title);
void *sdl_create_texture(int width, int height);
void sdl_render_copy(void *tex, void *sr, void *dr);
void sdl_render_copy_ex(void *tex, void *sr, void *dr, double angle);
int sdl_tex_xres(int cache_index);
int sdl_tex_yres(int cache_index);

// Custom texture loading
int sdl_load_mod_texture(const char *path);
void sdl_unload_mod_texture(int tex_id);
void sdl_cleanup_mod_textures(void);
void sdl_render_mod_texture(int tex_id, int x, int y, unsigned char alpha, int clipsx, int clipsy, int clipex,
    int clipey, int x_offset, int y_offset);
void sdl_render_mod_texture_scaled(int tex_id, int x, int y, float scale, unsigned char alpha, int clipsx, int clipsy,
    int clipex, int clipey, int x_offset, int y_offset);
int sdl_get_mod_texture_width(int tex_id);
int sdl_get_mod_texture_height(int tex_id);

// Render targets
int sdl_create_render_target(int width, int height);
void sdl_destroy_render_target(int target_id);
int sdl_set_render_target(int target_id);
void sdl_render_target_to_screen(int target_id, int x, int y, unsigned char alpha);
void sdl_clear_render_target(int target_id);

void sdl_flush_textinput(void);
void sdl_dump(FILE *fp);
#ifdef DEVELOPER
void sdl_dump_spritecache(void);
#endif
void sdl_tex_alpha(int cache_index, int alpha);
int sdl_check_mouse(void);
long long sdl_get_mem_tex(void);
