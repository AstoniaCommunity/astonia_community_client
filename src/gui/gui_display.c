/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * Graphical User Interface - Display rendering functions
 *
 */

#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <SDL3/SDL.h>

#include "astonia.h"
#include "gui/gui.h"
#include "gui/gui_private.h"
#include "client/client.h"
#include "game/game.h"
#include "sdl/sdl.h"
#include "modder/modder.h"
#include "lib/cjson/cJSON.h"

void display_helpandquest(void)
{
	if (display_help) {
		render_sprite(opt_sprite(990), dotx(DOT_HLP), doty(DOT_HLP), RENDERFX_NORMAL_LIGHT, RENDER_ALIGN_NORMAL);
	}
	if (display_quest) {
		render_sprite(opt_sprite(995), dotx(DOT_HLP), doty(DOT_HLP), RENDERFX_NORMAL_LIGHT, RENDER_ALIGN_NORMAL);
	}

	if (display_help) {
		do_display_help(display_help);
	}
	if (display_quest) {
		do_display_questlog(display_quest);
	}
}

char perf_text[256];

static void display_toplogic(void)
{
	static int top_opening = 0, top_closing = 1, top_open = 0;
	static int topframes = 0;

	if (mousey < 10) {
		topframes++;
	} else {
		topframes = 0;
	}

	if (topframes > frames_per_second / 2 && !top_opening && !top_open) {
		top_opening = 1;
		top_closing = 0;
	}
	if (mousey > 60 && !top_closing && top_open) {
		top_closing = 1;
		top_opening = 0;
	}

	if (top_opening) {
		gui_topoff = -38 + top_opening;
		top_opening += 6;
		if (top_opening >= 38) {
			top_open = 1;
			top_opening = 0;
		}
	}

	if (top_open) {
		gui_topoff = 0;
	}

	if (top_closing) {
		gui_topoff = -top_closing;
		top_closing += 6;
		if (top_closing >= 38) {
			top_open = 0;
			top_closing = 0;
		}
	}
}

void display_wheel(void)
{
	int i;

	render_push_clip();
	render_more_clip(0, 0, XRES, YRES0);

	if (now - vk_special_time < 2000) {
		int n, panic = 99;

		render_shaded_rect(mousex + 5, mousey - 7 - 20, mousex + 71, mousey + 31, 0x0000, 95);

		for (n = (vk_special + 1) % max_special, i = -1; panic-- && i > -3; n = (n + 1) % max_special) {
			if (!special_tab[n].req || value[0][special_tab[n].req]) {
				render_text(mousex + 9, mousey - 3 + i * 10, graycolor, RENDER_TEXT_LEFT, special_tab[n].name);
				i--;
			}
		}
		render_text(mousex + 9, mousey - 3, whitecolor, RENDER_TEXT_LEFT, special_tab[vk_special].name);

		for (n = (vk_special + max_special - 1) % max_special, i = 1; panic-- && i < 3;
		    n = (n + max_special - 1) % max_special) {
			if (!special_tab[n].req || value[0][special_tab[n].req]) {
				render_text(mousex + 9, mousey - 3 + i * 10, graycolor, RENDER_TEXT_LEFT, special_tab[n].name);
				i++;
			}
		}
	}
	render_pop_clip();
}

void dx_copysprite_emerald(int scrx, int scry, int emx, int emy)
{
	RenderFX ddfx;

	bzero(&ddfx, sizeof(ddfx));
	ddfx.sprite = 37;
	ddfx.align = RENDER_ALIGN_OFFSET;
	ddfx.clipsx = (short)(emx * 10);
	ddfx.clipsy = (short)(emy * 10);
	ddfx.clipex = ddfx.clipsx + 10;
	ddfx.clipey = ddfx.clipsy + 10;
	ddfx.ml = ddfx.ll = ddfx.rl = ddfx.ul = ddfx.dl = RENDERFX_NORMAL_LIGHT;
	ddfx.scale = 100;
	render_sprite_fx(&ddfx, scrx - ddfx.clipsx - 5, scry - ddfx.clipsy - 5);
}

size_t get_memory_usage(void);

void display(void)
{
	extern long long sdl_time_make, sdl_time_tex, sdl_time_tex_main, sdl_time_text, sdl_time_blit;
	time_t t;
	int tmp;
	uint64_t start = SDL_GetTicks();

#if 0
	// Performance for stuff happening during the actual tick only.
	// So zero them now after preload is done.
	sdl_time_make=0;
	sdl_time_tex=0;
	sdl_time_text=0;
	sdl_time_blit=0;
#endif

	if ((tmp = sdl_check_mouse())) {
		mousex = -1;
		if (tmp == -1) {
			mousey = 0;
		} else {
			mousey = YRES / 2;
		}
	}

	display_toplogic();
	if (game_slowdown) {
		display_toplogic();
		display_toplogic();
		display_toplogic();
	}

	set_cmd_states();

	if (sockstate < 4 && ((t = time(NULL) - (time_t)socktimeout) > 10 || !originx)) {
		render_rect(0, 0, XRES, YRES0 - 60, blackcolor);
		display_screen();
		display_text();
		if ((now / 1000) & 1) {
			render_text(
			    XRES / 2, (YRES0 - 60) / 2 - 60, redcolor, RENDER_ALIGN_CENTER | RENDER_TEXT_LARGE, "not connected");
		}
		render_sprite(60, XRES / 2, ((YRES0 - 60) - 240) / 2, RENDERFX_NORMAL_LIGHT, RENDER_ALIGN_CENTER);
		if (!kicked_out) {
			render_text_fmt(XRES / 2, (YRES0 - 60) / 2 - 40, textcolor,
			    RENDER_TEXT_SMALL | RENDER_ALIGN_CENTER | RENDER_TEXT_FRAMED,
			    "Trying to establish connection. %ld seconds...", (long)t);
			if (t > 15) {
				render_text_fmt(XRES / 2, (YRES0 - 60) / 2 - 0, textcolor,
				    RENDER_TEXT_LARGE | RENDER_ALIGN_CENTER | RENDER_TEXT_FRAMED,
				    "Please check %s for troubleshooting advice.", game_url);
			}
		}
		goto display_graphs; // I know, I know. goto considered harmful and all that.
	}

	render_push_clip();
	render_more_clip(dotx(DOT_MTL), doty(DOT_MTL), dotx(DOT_MBR), doty(DOT_MBR));
	display_game();
	render_pop_clip();

	display_screen();

	display_keys();
	if (game_options & GO_WHEEL) {
		display_wheel();
	}
	if (show_look) {
		display_look();
	}
	display_wear();
	display_inventory();
	display_action();
	if (con_cnt) {
		display_container();
	} else {
		display_skill();
	}
	display_scrollbars();
	display_text();
	display_gold();
	display_mode();
	display_selfspells();
	display_exp();
	display_military();
	display_teleport();
	display_color();
	display_rage();
	display_game_special();
	display_tutor();
	display_selfbars();
	display_minimap();
	display_citem();
	context_display(mousex, mousey);
	display_helpandquest(); // display last because it is on top

	// Display lag warning when no server data received for > 500ms
	if (sockstate == 4 && last_tick_received_time > 0) {
		uint64_t lag_ms = SDL_GetTicks() - last_tick_received_time;
		if (lag_ms > 500) {
			render_text_fmt(XRES / 2, doty(DOT_MTL) + 35, IRGB(31, 0, 0),
			    RENDER_TEXT_LARGE | RENDER_ALIGN_CENTER | RENDER_TEXT_FRAMED | RENDER_TEXT_NOCACHE,
			    "LAG: %" PRIu64 "ms", lag_ms);
		}
	}

display_graphs:;

	int64_t duration = (int64_t)(SDL_GetTicks() - start);

	if (display_vc) {
		extern long long texc_miss, texc_pre; // mem_tex,
		extern uint64_t sdl_backgnd_wait, sdl_backgnd_work, sdl_time_preload, sdl_time_load, gui_time_network;
		extern uint64_t gui_frametime, gui_ticktime;
		extern uint64_t sdl_time_pre1, sdl_time_pre2, sdl_time_pre3, sdl_time_mutex, sdl_time_alloc, sdl_time_make_main;
		extern int x_offset, y_offset; // pre_2,pre_in,pre_3;
		// static int dur=0,make=0,tex=0,text=0,blit=0,stay=0;
		static int size;
		static unsigned char dur_graph[100], size1_graph[100], size2_graph[100],
		    size3_graph[100]; //,size_graph[100];load_graph[100],
		static unsigned char pre1_graph[100], pre2_graph[100], pre3_graph[100];
		// static int frame_min=99,frame_max=0,frame_step=0;
		// static int tick_min=99,tick_max=0,tick_step=0;
		int px = XRES - 110, py = 35 + (!(game_options & GO_SMALLTOP) ? 0 : gui_topoff);

		// render_text_fmt(px,py+=10,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"skip
		// %3.0f%%",100.0*skip/tota);
		// render_text_fmt(px,py+=10,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"idle
		// %3.0f%%",100.0*idle/tota);
		// render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Tex: %5.2f
		// MB",mem_tex/(1024.0*1024.0));
		render_text_fmt(px, py += 10, IRGB(8, 31, 8), RENDER_TEXT_LEFT | RENDER_TEXT_FRAMED | RENDER_TEXT_NOCACHE,
		    "Mem: %5.2f MB", (double)get_memory_usage() / (1024.0 * 1024.0));

#if 0
	    if (pre_in>=pre_3) size=pre_in-pre_3;
	    else size=16384+pre_in-pre_3;

	    render_text_fmt(px,py+=10,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"PreC %d",size);
#endif
#if 0
	    extern int pre_in,pre_1,pre_2,pre_3;
	    extern int texc_used;
	    py+=10;
	    render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"PreI %d",pre_in);
	    render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Pre1 %d",pre_1);
	    render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Pre2 %d",pre_2);
	    render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Pre3 %d",pre_3);
	    render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Used %d",texc_used);
	    render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Cache %d/%d",sdl_cache_size,MAX_TEXCACHE);
#endif
		// render_text_fmt(px,py+=10,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Miss
		// %lld",texc_miss);
		// render_text_fmt(px,py+=10,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Prel
		// %lld",texc_pre);

		py += 10;

		{
			uint64_t sum = (uint64_t)duration + gui_time_network;
			size = sum > 42 ? 42 : (int)sum;
		}
		render_text(px, py += 10, IRGB(8, 31, 8), RENDER_TEXT_LEFT | RENDER_TEXT_FRAMED, "Render");
		sdl_bargraph_add(sizeof(dur_graph), dur_graph, size);
		sdl_bargraph(px, py += 40, sizeof(dur_graph), dur_graph, x_offset, y_offset);

#if 0
	    if (gui_frametime<frame_min) frame_min=gui_frametime;
	    if (gui_frametime>frame_max) frame_max=gui_frametime;
	    render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_NOCACHE|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED,"FT %d %d",frame_min,frame_max);

	    if (gui_ticktime<tick_min) tick_min=gui_ticktime;
	    if (gui_ticktime>tick_max) tick_max=gui_ticktime;
	    render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_NOCACHE|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED,"TT %d %d",tick_min,tick_max);
#endif
		{
			uint64_t val = gui_frametime / 2;
			size = val > 42 ? 42 : (int)val;
		}
		render_text_fmt(px, py += 10, IRGB(8, 31, 8), RENDER_TEXT_NOCACHE | RENDER_TEXT_LEFT | RENDER_TEXT_FRAMED,
		    "Frametime %" PRId64, gui_frametime);
		sdl_bargraph_add(sizeof(pre2_graph), pre2_graph, size);
		sdl_bargraph(px, py += 40, sizeof(pre2_graph), pre2_graph, x_offset, y_offset);

		{
			uint64_t val = gui_ticktime / 2;
			size = val > 42 ? 42 : (int)val;
		}
		render_text_fmt(px, py += 10, IRGB(8, 31, 8), RENDER_TEXT_NOCACHE | RENDER_TEXT_LEFT | RENDER_TEXT_FRAMED,
		    "Ticktime %" PRId64, gui_ticktime);
		sdl_bargraph_add(sizeof(pre3_graph), pre3_graph, size);
		sdl_bargraph(px, py += 40, sizeof(pre3_graph), pre3_graph, x_offset, y_offset);
#if 0
	    size=gui_time_network;
	    render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED,"Network");
	    sdl_bargraph_add(sizeof(pre2_graph),pre2_graph,size<42?size:42);
	    sdl_bargraph(px,py+=40,sizeof(pre2_graph),pre2_graph,x_offset,y_offset);

	    size=sdl_time_pre1;
	    render_text(px,py+=10,IRGB(8,31,8),RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED,"Alloc");
	    sdl_bargraph_add(sizeof(size1_graph),size3_graph,size<42?size:42);
	    sdl_bargraph(px,py+=40,sizeof(size1_graph),size3_graph,x_offset,y_offset);
#endif


		size = (lasttick + q_size) * 2;
		render_text_fmt(px, py += 10, IRGB(8, 31, 8), RENDER_TEXT_FRAMED | RENDER_TEXT_LEFT, "Queue %d", size / 2);
		sdl_bargraph_add(sizeof(pre2_graph), size3_graph, size < 42 ? size : 42);
		sdl_bargraph(px, py += 40, sizeof(pre2_graph), size3_graph, x_offset, y_offset);

		// Tick interval indicator - time between server tick batch arrivals
		{
			static unsigned char lag_graph[100];
			static int was_lagging = 0;
			// Normal tick interval is ~40ms, show warning color if consistently high
			int lag_size = tick_receive_interval > 200 ? 42 : (int)(tick_receive_interval * 42 / 200);
			// Hysteresis to prevent color flicker: red at 120ms, green at 80ms
			if (tick_receive_interval > 120) {
				was_lagging = 1;
			} else if (tick_receive_interval < 80) {
				was_lagging = 0;
			}
			unsigned short lag_color = was_lagging ? IRGB(31, 8, 8) : IRGB(8, 31, 8);
			render_text_fmt(px, py += 10, lag_color, RENDER_TEXT_FRAMED | RENDER_TEXT_LEFT | RENDER_TEXT_NOCACHE,
			    "Tick %" PRIu64 "ms", tick_receive_interval);
			sdl_bargraph_add(sizeof(lag_graph), lag_graph, lag_size);
			sdl_bargraph(px, py += 40, sizeof(lag_graph), lag_graph, x_offset, y_offset);
		}

		{
			uint64_t sum = sdl_time_pre1 + sdl_time_pre3;
			size = sum > 42 ? 42 : (int)sum;
		}
		render_text(px, py += 10, IRGB(8, 31, 8), RENDER_TEXT_LEFT | RENDER_TEXT_FRAMED, "Pre-Main");
		sdl_bargraph_add(sizeof(size1_graph), size2_graph, size);
		sdl_bargraph(px, py += 40, sizeof(size1_graph), size2_graph, x_offset, y_offset);
#if 0

#endif
		if (sdl_multi) {
			uint64_t val = sdl_backgnd_work / (uint64_t)sdl_multi;
			size = val > 42 ? 42 : (int)val;
			render_text_fmt(
			    px, py += 10, IRGB(8, 31, 8), RENDER_TEXT_LEFT | RENDER_TEXT_FRAMED, "Pre-Back (%d)", sdl_multi);
		} else {
			uint64_t val = sdl_time_pre2;
			size = val > 42 ? 42 : (int)val;
			render_text_fmt(px, py += 10, IRGB(8, 31, 8), RENDER_TEXT_LEFT | RENDER_TEXT_FRAMED, "Make");
		}
		sdl_bargraph_add(sizeof(pre1_graph), pre1_graph, size);
		sdl_bargraph(px, py += 40, sizeof(pre1_graph), pre1_graph, x_offset, y_offset);
#if 0
	        render_text_fmt(px,py+=10,IRGB(8,31,8),RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED,"Mutex");
	        sdl_bargraph_add(sizeof(pre2_graph),pre2_graph,sdl_time_mutex/sdl_multi<42?sdl_time_mutex/sdl_multi:42);
	        sdl_bargraph(px,py+=40,sizeof(pre2_graph),pre2_graph,x_offset,y_offset);
#endif
#if 0
	    render_text(px,py+=10,IRGB(8,31,8),RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED,"Pre-Queue Tot");
	    sdl_bargraph_add(sizeof(size_graph),size_graph,size/4<42?size/4:42);
	    sdl_bargraph(px,py+=40,sizeof(size_graph),size_graph,x_offset,y_offset);

	    render_text(px,py+=10,IRGB(8,31,8),RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED,"Pre2");
	    sdl_bargraph_add(sizeof(pre2_graph),pre2_graph,sdl_time_pre2<42?sdl_time_pre2:42);
	    sdl_bargraph(px,py+=40,sizeof(pre2_graph),pre2_graph,x_offset,y_offset);

	    render_text(px,py+=10,IRGB(8,31,8),RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED,"Texture");
	    sdl_bargraph_add(sizeof(pre3_graph),pre3_graph,sdl_time_pre3<42?sdl_time_pre3:42);
	    sdl_bargraph(px,py+=40,sizeof(pre3_graph),pre3_graph,x_offset,y_offset);

#endif
#if 0
	    if (pre_2>=pre_3) size=pre_2-pre_3;
	    else size=16384+pre_2-pre_3;

	    render_text(px,py+=10,IRGB(8,31,8),RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED,"Size Tex");
	    sdl_bargraph_add(sizeof(size3_graph),size3_graph,size/4<42?size/4:42);
	    sdl_bargraph(px,py+=40,sizeof(size3_graph),size3_graph,x_offset,y_offset);


	    if (duration>10 && (!stay || duration>dur)) {
	        dur=duration;
	        make=sdl_time_make;
	        tex=sdl_time_tex;
	        text=sdl_time_text;
	        blit=sdl_time_blit;
	        stay=24*6;
	    }

	    if (stay>0) {
	        stay--;
	        render_text_fmt(px,py+=20,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Dur %dms (%.0f%%)",dur,100.0*(make+tex+text+blit)/dur);
	        render_text_fmt(px,py+=10,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Make %dms (%.0f%%)",make,100.0*make/dur);
	        render_text_fmt(px,py+=10,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Tex %dms (%.0f%%)",tex,100.0*tex/dur);
	        render_text_fmt(px,py+=10,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Text %dms (%.0f%%)",text,100.0*text/dur);
	        render_text_fmt(px,py+=10,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_LEFT|RENDER_TEXT_FRAMED|RENDER_TEXT_NOCACHE,"Blit %dms (%.0f%%)",blit,100.0*blit/dur);
	    }
#endif
		sdl_time_preload = 0;
		sdl_time_make = 0;
		sdl_time_tex = 0;
		sdl_time_text = 0;
		sdl_time_blit = 0;
		sdl_backgnd_work = 0;
		sdl_backgnd_wait = 0;
		sdl_time_load = 0;
		sdl_time_pre1 = 0;
		sdl_time_pre2 = 0;
		sdl_time_pre3 = 0;
		sdl_time_mutex = 0;
		sdl_time_tex_main = 0;
		gui_time_misc = 0;
		sdl_time_alloc = 0;
		texc_miss = 0;
		texc_pre = 0;
		sdl_time_make_main = 0;
		gui_time_network = 0;
#if 0
	    if (SDL_GetTicks()-frame_step>1000) {
	        frame_step=SDL_GetTicks();
	        frame_min=99;
	        frame_max=0;
	    }
	    if (SDL_GetTicks()-tick_step>1000) {
	        tick_step=SDL_GetTicks();
	        tick_min=99;
	        tick_max=0;
	    }
#endif
	} // else render_text_fmt(650,15,0xffff,RENDER_TEXT_SMALL|RENDER_TEXT_FRAMED,"Mirror %d",mirror);

	sprintf(perf_text, "mem usage=%zu/%.2fMB, %d/%dKBlocks", memsize[0] / 1024 / 1024,
	    (double)memused / 1024.0 / 1024.0, memptrs[0] / 1024, memptrused / 1024);
}

// cmd

void update_ui_layout(void)
{
	static int last_con_cnt = 0;

	if (update_skltab) {
		set_skltab();
		update_skltab = 0;
	}
	if (last_con_cnt != con_cnt) {
		conoff = 0;
		max_conoff = (con_cnt / CONDX) - CONDY;
		last_con_cnt = con_cnt;
		set_conoff(0, conoff);
		set_skloff(0, skloff);
	}
	max_invoff = ((_inventorysize - 30) / INVDX) - INVDY;
	set_button_flags();
}

typedef enum HelpBlockType {
	HELP_BLOCK_TITLE = 0,
	HELP_BLOCK_TEXT = 1,
} HelpBlockType;

typedef struct HelpBlock {
	HelpBlockType type;
	char *text;
} HelpBlock;

typedef struct HelpTopic {
	char *title;
	HelpBlock *blocks;
	int block_count;
} HelpTopic;

static HelpTopic *help_topics = NULL;
static int help_topic_count = 0;
static int *help_topic_pages = NULL;
static char **help_fast_help = NULL;
static int help_fast_help_count = 0;
static char **help_index_titles = NULL;
static int *help_index_pages = NULL;
int help_page_count = 2;
int help_index_count = 0;

static char *help_load_file(const char *path, size_t *out_len)
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

static void help_format_text(const char *in, char *out, size_t out_size)
{
	size_t out_len = 0;
	int i;

	struct {
		const char *token;
		const char *value;
	} replacements[] = {
	    {"{game_url}", game_url ? game_url : ""},
	    {"{game_email_cash}", game_email_cash ? game_email_cash : ""},
	    {"{game_email_main}", game_email_main ? game_email_main : ""},
	};

	if (!in || !out || out_size == 0) {
		return;
	}

	for (i = 0; in[i] && out_len + 1 < out_size;) {
		int replaced = 0;
		int r;

		for (r = 0; r < (int)(sizeof(replacements) / sizeof(replacements[0])); r++) {
			size_t token_len = strlen(replacements[r].token);
			if (strncmp(&in[i], replacements[r].token, token_len) == 0) {
				size_t value_len = strlen(replacements[r].value);
				size_t copy_len = min(value_len, out_size - 1 - out_len);
				if (copy_len > 0) {
					memcpy(out + out_len, replacements[r].value, copy_len);
					out_len += copy_len;
				}
				i += (int)token_len;
				replaced = 1;
				break;
			}
		}
		if (!replaced) {
			out[out_len++] = in[i++];
		}
	}

	out[out_len] = '\0';
}

static int help_text_height(const char *text, unsigned short color)
{
	char buf[4096];

	help_format_text(text, buf, sizeof(buf));
	return render_text_break_length(0, 0, HELP_TEXT_WIDTH, color, 0, buf);
}

static void help_truncate_index_title(const char *text, char *out, size_t out_size, int max_width)
{
	size_t n = 0;
	int ellipsis_width;
	int full_width;
	int truncated = 0;

	if (!text || !out || out_size == 0) {
		return;
	}

	full_width = render_text_length(0, text);
	if (full_width > max_width) {
		truncated = 1;
		ellipsis_width = render_text_length(0, "...");
		if (max_width > ellipsis_width) {
			max_width -= ellipsis_width;
		}
	}

	while (text[n] && n + 1 < out_size) {
		int width = render_text_len(0, text, (int)(n + 1));
		if (width > max_width) {
			break;
		}
		n++;
	}

	memcpy(out, text, n);
	out[n] = '\0';

	if (truncated && n + 3 < out_size) {
		strncat(out, "...", out_size - n - 1);
	}
}

static int help_topic_height(const HelpTopic *topic)
{
	int i;
	int height = 0;

	if (!topic || !topic->title) {
		return 0;
	}

	height += help_text_height(topic->title, whitecolor) + HELP_TITLE_SPACING;

	for (i = 0; i < topic->block_count; i++) {
		unsigned short color = topic->blocks[i].type == HELP_BLOCK_TITLE ? whitecolor : graycolor;
		int spacing = topic->blocks[i].type == HELP_BLOCK_TITLE ? HELP_TITLE_SPACING : HELP_PARAGRAPH_SPACING;

		height += help_text_height(topic->blocks[i].text, color) + spacing;
	}

	return height;
}

static void help_build_pages(void)
{
	int i;
	int start_y = doty(DOT_HLP) + HELP_PAGE_MARGIN_TOP;
	int content_bottom = doty(DOT_HL2) - HELP_PAGE_MARGIN_BOTTOM;
	int y = start_y;
	int page = 0;
	int pages_for_topics = 0;

	if (help_topic_count > 0) {
		help_topic_pages = xmalloc(sizeof(*help_topic_pages) * (size_t)help_topic_count, MEM_GUI);
	}

	for (i = 0; i < help_topic_count; i++) {
		int height = help_topic_height(&help_topics[i]);

		if (y != start_y && y + height > content_bottom) {
			page++;
			y = start_y;
		}

		help_topic_pages[i] = page;
		y += height;
	}

	if (help_topic_count > 0) {
		pages_for_topics = page + 1;
	}

	help_page_count = 2 + pages_for_topics;
	if (help_page_count < 2) {
		help_page_count = 2;
	}

	help_index_count = help_topic_count;
	if (help_index_count > 0) {
		help_index_titles = xmalloc(sizeof(*help_index_titles) * (size_t)help_index_count, MEM_GUI);
		help_index_pages = xmalloc(sizeof(*help_index_pages) * (size_t)help_index_count, MEM_GUI);
		for (i = 0; i < help_topic_count; i++) {
			help_index_titles[i] = help_topics[i].title;
			help_index_pages[i] = 3 + help_topic_pages[i];
		}
	}
}

static int help_load_from_json(const char *json_str, const char *source_name)
{
	cJSON *root = cJSON_Parse(json_str);
	if (!root) {
		warn("help: Failed to parse %s: %s", source_name, cJSON_GetErrorPtr());
		return -1;
	}

	cJSON *fast_help = cJSON_GetObjectItem(root, "fast_help");
	if (fast_help && cJSON_IsArray(fast_help)) {
		int count = cJSON_GetArraySize(fast_help);
		int i;

		if (count > 0) {
			help_fast_help = xmalloc(sizeof(*help_fast_help) * (size_t)count, MEM_GUI);
			help_fast_help_count = 0;
			for (i = 0; i < count; i++) {
				cJSON *item = cJSON_GetArrayItem(fast_help, i);
				if (item && cJSON_IsString(item)) {
					help_fast_help[help_fast_help_count++] = xstrdup(item->valuestring, MEM_GUI);
				}
			}
		}
	}

	cJSON *topics = cJSON_GetObjectItem(root, "topics");
	if (topics && cJSON_IsArray(topics)) {
		int count = cJSON_GetArraySize(topics);
		int i;
		int valid = 0;

		for (i = 0; i < count; i++) {
			cJSON *item = cJSON_GetArrayItem(topics, i);
			cJSON *title = item ? cJSON_GetObjectItem(item, "title") : NULL;
			if (item && cJSON_IsObject(item) && title && cJSON_IsString(title)) {
				valid++;
			}
		}

		if (valid > 0) {
			help_topics = xmalloc(sizeof(*help_topics) * (size_t)valid, MEM_GUI);
			memset(help_topics, 0, sizeof(*help_topics) * (size_t)valid);
			help_topic_count = 0;
			for (i = 0; i < count; i++) {
				cJSON *item = cJSON_GetArrayItem(topics, i);
				cJSON *title = item ? cJSON_GetObjectItem(item, "title") : NULL;
				cJSON *blocks = item ? cJSON_GetObjectItem(item, "blocks") : NULL;
				int block_count = 0;
				int b;

				if (!item || !cJSON_IsObject(item) || !title || !cJSON_IsString(title)) {
					continue;
				}

				help_topics[help_topic_count].title = xstrdup(title->valuestring, MEM_GUI);

				if (blocks && cJSON_IsArray(blocks)) {
					int total = cJSON_GetArraySize(blocks);
					for (b = 0; b < total; b++) {
						cJSON *block = cJSON_GetArrayItem(blocks, b);
						cJSON *text = NULL;
						if (block && cJSON_IsString(block)) {
							text = block;
						} else if (block && cJSON_IsObject(block)) {
							text = cJSON_GetObjectItem(block, "text");
						}
						if (text && cJSON_IsString(text)) {
							block_count++;
						}
					}

					if (block_count > 0) {
						help_topics[help_topic_count].blocks =
						    xmalloc(sizeof(*help_topics[help_topic_count].blocks) * (size_t)block_count, MEM_GUI);
						help_topics[help_topic_count].block_count = 0;
						for (b = 0; b < total; b++) {
							cJSON *block = cJSON_GetArrayItem(blocks, b);
							cJSON *type = NULL;
							cJSON *text = NULL;
							HelpBlockType block_type = HELP_BLOCK_TEXT;

							if (block && cJSON_IsString(block)) {
								text = block;
							} else if (block && cJSON_IsObject(block)) {
								type = cJSON_GetObjectItem(block, "type");
								text = cJSON_GetObjectItem(block, "text");
							}

							if (!text || !cJSON_IsString(text)) {
								continue;
							}

							if (type && cJSON_IsString(type) && strcmp(type->valuestring, "title") == 0) {
								block_type = HELP_BLOCK_TITLE;
							}

							help_topics[help_topic_count].blocks[help_topics[help_topic_count].block_count].type =
							    block_type;
							help_topics[help_topic_count].blocks[help_topics[help_topic_count].block_count].text =
							    xstrdup(text->valuestring, MEM_GUI);
							help_topics[help_topic_count].block_count++;
						}
					}
				}

				help_topic_count++;
			}
		}
	}

	cJSON_Delete(root);

	help_build_pages();
	return 0;
}

static void help_set_fallback(const char *path)
{
	HelpTopic *topic;
	HelpBlock *block;
	char buf[256];

	snprintf(buf, sizeof(buf), "Help data missing: %s", path ? path : "unknown");

	help_topics = xmalloc(sizeof(*help_topics), MEM_GUI);
	memset(help_topics, 0, sizeof(*help_topics));
	help_topic_count = 1;
	topic = &help_topics[0];
	topic->title = xstrdup("Help", MEM_GUI);
	topic->blocks = xmalloc(sizeof(*topic->blocks), MEM_GUI);
	topic->block_count = 1;
	block = &topic->blocks[0];
	block->type = HELP_BLOCK_TEXT;
	block->text = xstrdup(buf, MEM_GUI);

	help_build_pages();
}

void help_init(void)
{
	char path[64];
	char *json;

	snprintf(path, sizeof(path), "res/config/help_v%d.json", sv_ver);
	json = help_load_file(path, NULL);
	if (!json) {
		warn("help: Failed to read %s", path);
		help_set_fallback(path);
		return;
	}

	if (help_load_from_json(json, path) < 0) {
		help_set_fallback(path);
	}

	xfree(json);
}

int help_index_page_for_entry(int entry)
{
	if (entry < 0 || entry >= help_index_count || !help_index_pages) {
		return 0;
	}
	return help_index_pages[entry];
}

DLL_EXPORT int _do_display_help(int nr)
{
	int x = dotx(DOT_HLP) + 10;
	int y = doty(DOT_HLP) + HELP_PAGE_MARGIN_TOP;
	int content_right = x + HELP_TEXT_WIDTH;
	int content_bottom = doty(DOT_HL2) - HELP_PAGE_MARGIN_BOTTOM;
	int i, b;

	if (nr < 1 || nr > help_page_count) {
		nr = 1;
	}

	if (nr == 1) {
		y = render_text_break(x, y, content_right, whitecolor, 0, "Fast Help");
		y += HELP_FAST_HELP_TITLE_SPACING;
		for (i = 0; i < help_fast_help_count; i++) {
			char buf[4096];

			help_format_text(help_fast_help[i], buf, sizeof(buf));
			y = render_text_break(x, y, content_right, graycolor, 0, buf);
		}
		return y;
	}

	if (nr == 2) {
		int start_y;
		int rows;
		int columns = 2;
		int max_entries;
		int visible;

		y = render_text_break(x, y, content_right, whitecolor, 0, "Help Index");
		y += HELP_INDEX_TITLE_SPACING;
		start_y = y;
		rows = (content_bottom - start_y) / HELP_INDEX_ROW_HEIGHT;
		if (rows < 1) {
			rows = 1;
		}
		max_entries = rows * columns;
		visible = min(help_index_count, max_entries);

		for (i = 0; i < visible; i++) {
			int col = i / rows;
			int row = i % rows;
			int tx = x + col * HELP_INDEX_COL_WIDTH;
			int ty = start_y + row * HELP_INDEX_ROW_HEIGHT;
			char label[128];

			help_truncate_index_title(help_index_titles[i], label, sizeof(label), HELP_INDEX_COL_WIDTH - 16);
			render_text(tx, ty, lightbluecolor, 0, label);
		}
		return y;
	}

	for (i = 0; i < help_topic_count; i++) {
		if (!help_topic_pages || help_topic_pages[i] != nr - 3) {
			continue;
		}

		y = render_text_break(x, y, content_right, whitecolor, 0, help_topics[i].title);
		y += HELP_TITLE_SPACING;

		for (b = 0; b < help_topics[i].block_count; b++) {
			char buf[4096];
			HelpBlock *block = &help_topics[i].blocks[b];
			unsigned short color = block->type == HELP_BLOCK_TITLE ? whitecolor : graycolor;
			int spacing = block->type == HELP_BLOCK_TITLE ? HELP_TITLE_SPACING : HELP_PARAGRAPH_SPACING;

			help_format_text(block->text, buf, sizeof(buf));
			y = render_text_break(x, y, content_right, color, 0, buf);
			y += spacing;
		}
	}

	return y;
}
