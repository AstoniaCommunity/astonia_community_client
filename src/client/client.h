/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 */
#include "./dll.h"
#include <time.h>

#define MAXCHARS 2048

#define MAXQUEST 100

#define MAXEF 64

#define CMF_LIGHT      (1 + 2 + 4 + 8)
#define CMF_VISIBLE    16
#define CMF_TAKE       32
#define CMF_USE        64
#define CMF_INFRA      128
#define CMF_UNDERWATER 256

#define VERSION 0x030100

typedef enum {
	V_HP = 0,
	V_ENDURANCE = 1,
	V_MANA = 2,
	V_WIS = 3,
	V_INT = 4,
	V_AGI = 5,
	V_STR = 6,
	V_ARMOR = 7,
	V_WEAPON = 8,
	V_LIGHT = 9,
	V_SPEED = 10,
	V_PULSE = 11,
	V_DAGGER = 12,
	V_HAND = 13,
	V_STAFF = 14,
	V_SWORD = 15,
	V_TWOHAND = 16,
	V_ARMORSKILL = 17,
	V_ATTACK = 18,
	V_PARRY = 19,
	V_WARCRY = 20,
	V_TACTICS = 21,
	V_SURROUND = 22,
	V_BODYCONTROL = 23,
	V_SPEEDSKILL = 24,
	V_BARTER = 25,
	V_PERCEPT = 26,
	V_STEALTH = 27,
	V_BLESS = 28,
	V_HEAL = 29,
	V_FREEZE = 30,
	V_MAGICSHIELD = 31,
	V_FLASH = 32,
	V_FIREBALL = 33,
	V_REGENERATE = 35,
	V_MEDITATE = 36,
	V_IMMUNITY = 37,
	V_DEMON = 38,
	V_DURATION = 39,
	V_RAGE = 40,
	V_COLD = 41,
	V_PROFESSION = 42,

	V_ILLEGAL = -1
} vval_t;

#define V3_PROFBASE  43
#define V35_PROFBASE 50
#define V_PROFBASE   (*game_v_profbase)

#define P3_MAX  20
#define P35_MAX 10

#define V35_MAX (V35_PROFBASE + P35_MAX)
#define V_MAX   200

#define CL_NOP            1
#define CL_MOVE           2
#define CL_SWAP           3
#define CL_TAKE           4
#define CL_DROP           5
#define CL_KILL           6
#define CL_CONTAINER      7
#define CL_TEXT           8
#define CL_USE            9
#define CL_BLESS          10
#define CL_FIREBALL       11
#define CL_HEAL           12
#define CL_MAGICSHIELD    13
#define CL_FREEZE         14
#define CL_RAISE          15
#define CL_USE_INV        16
#define CL_FLASH          17
#define CL_BALL           18
#define CL_WARCRY         19
#define CL_LOOK_CONTAINER 20
#define CL_LOOK_MAP       21
#define CL_LOOK_INV       22
#define CL_LOOK_CHAR      23
#define CL_LOOK_ITEM      24
#define CL_GIVE           25
#define CL_SPEED          26
#define CL_STOP           27
#define CL_TAKE_GOLD      28
#define CL_DROP_GOLD      29
#define CL_JUNK_ITEM      30

#define CL_TICKER         33
#define CL_CONTAINER_FAST 34
#define CL_FASTSELL       35
#define CL_LOG            36
#define CL_TELEPORT       37
#define CL_PULSE          38
#define CL_PING           39
#define CL_GETQUESTLOG    40
#define CL_REOPENQUEST    41

#define PAC_IDLE        0
#define PAC_MOVE        1
#define PAC_TAKE        2
#define PAC_DROP        3
#define PAC_KILL        4
#define PAC_USE         5
#define PAC_BLESS       6
#define PAC_HEAL        7
#define PAC_FREEZE      8
#define PAC_FIREBALL    9
#define PAC_BALL        10
#define PAC_MAGICSHIELD 11
#define PAC_FLASH       12
#define PAC_WARCRY      13
#define PAC_LOOK_MAP    14
#define PAC_GIVE        15
#define PAC_BERSERK     16

#define V3_INVENTORYSIZE 110
#define V3_CONTAINERSIZE (V3_INVENTORYSIZE)

#define V35_INVENTORYSIZE 70
#define V35_CONTAINERSIZE 130

#define MAX_INVENTORYSIZE (V3_INVENTORYSIZE)
#define MAX_CONTAINERSIZE (V35_CONTAINERSIZE)

DLL_EXPORT extern int _inventorysize, _containersize;

#define IF_USE         (1 << 4)
#define IF_WNHEAD      (1 << 5) // can be worn on head
#define IF_WNNECK      (1 << 6) // etc...
#define IF_WNBODY      (1 << 7)
#define IF_WNARMS      (1 << 8)
#define IF_WNBELT      (1 << 9)
#define IF_WNLEGS      (1 << 10)
#define IF_WNFEET      (1 << 11)
#define IF_WNLHAND     (1 << 12)
#define IF_WNRHAND     (1 << 13)
#define IF_WNCLOAK     (1 << 14)
#define IF_WNLRING     (1 << 15)
#define IF_WNRRING     (1 << 16)
#define IF_WNTWOHANDED (1 << 17) // two-handed weapon, fills both WNLHAND & WNRHAND

#define MAPDX (DIST * 2 + 1)
#define MAPDY (DIST * 2 + 1)
#define MAXMN (MAPDX * MAPDY)

#define QF_OPEN 1
#define QF_DONE 2

#define QLF_REPEATABLE (1u << 0)
#define QLF_XREPEAT    (1u << 1)

#define MAXSHRINE 256

struct quest {
	unsigned char done : 6;
	unsigned char flags : 2;
};

struct shrine_ppd {
	unsigned int used[MAXSHRINE / 32];
	unsigned char continuity;
};

struct player {
	char name[80];
	sprite_id_t csprite;
	stat_t level;
	unsigned short c1, c2, c3;
	unsigned char clan;
	unsigned char pk_status;
};

struct cef_generic {
	unsigned int nr;
	int type;
};

struct cef_shield {
	int nr;
	int type;
	char_id_t cn;
	uint32_t start;
};

struct cef_strike {
	int nr;
	int type;
	char_id_t cn;
	int x, y; // target
};

struct cef_ball {
	int nr;
	int type;
	uint32_t start;
	int frx, fry; // high precision coords
	int tox, toy; // high precision coords
};

struct cef_fireball {
	int nr;
	int type;
	uint32_t start;
	int frx, fry; // high precision coords
	int tox, toy; // high precision coords
};

struct cef_edemonball {
	int nr;
	int type;
	uint32_t start;
	int base;
	int frx, fry; // high precision coords
	int tox, toy; // high precision coords
};

struct cef_flash {
	int nr;
	int type;
	char_id_t cn;
};

struct cef_explode {
	int nr;
	int type;
	uint32_t start;
	unsigned int base;
};

struct cef_warcry {
	int nr;
	int type;
	char_id_t cn;
	int stop;
};

struct cef_bless {
	int nr;
	int type;
	char_id_t cn;
	uint32_t start;
	uint32_t stop;
	int strength;
};

struct cef_heal {
	int nr;
	int type;
	char_id_t cn;
	uint32_t start;
};

struct cef_freeze {
	int nr;
	int type;
	char_id_t cn;
	uint32_t start;
	uint32_t stop;
};

struct cef_burn {
	int nr;
	int type;
	char_id_t cn;
	int stop;
};

struct cef_mist {
	int nr;
	int type;
	uint32_t start;
};

struct cef_pulse {
	int nr;
	int type;
	uint32_t start;
};

struct cef_pulseback {
	int nr;
	int type;
	char_id_t cn;
	int x, y;
};

struct cef_potion {
	int nr;
	int type;
	char_id_t cn;
	uint32_t start;
	uint32_t stop;
	int strength;
};

struct cef_earthrain {
	int nr;
	int type;
	int strength;
};

struct cef_earthmud {
	int nr;
	int type;
};

struct cef_curse {
	int nr;
	int type;
	char_id_t cn;
	uint32_t start;
	uint32_t stop;
	int strength;
};

struct cef_cap {
	int nr;
	int type;
	char_id_t cn;
};

struct cef_lag {
	int nr;
	int type;
	char_id_t cn;
};

struct cef_firering {
	int nr;
	int type;
	char_id_t cn;
	uint32_t start;
};

struct cef_bubble {
	int nr;
	int type;
	int yoff;
};

union ceffect {
	struct cef_generic generic;
	struct cef_shield shield;
	struct cef_strike strike;
	struct cef_ball ball;
	struct cef_fireball fireball;
	struct cef_flash flash;
	struct cef_explode explode;
	struct cef_warcry warcry;
	struct cef_bless bless;
	struct cef_heal heal;
	struct cef_freeze freeze;
	struct cef_burn burn;
	struct cef_mist mist;
	struct cef_potion potion;
	struct cef_earthrain earthrain;
	struct cef_earthmud earthmud;
	struct cef_edemonball edemonball;
	struct cef_curse curse;
	struct cef_cap cap;
	struct cef_lag lag;
	struct cef_pulse pulse;
	struct cef_pulseback pulseback;
	struct cef_firering firering;
	struct cef_bubble bubble;
};

struct complex_sprite {
	unsigned int sprite;
	unsigned short c1, c2, c3, shine;
	unsigned char cr, cg, cb;
	unsigned char light, sat;
	unsigned char scale;
};

struct map {
	// from map & item
	unsigned short int gsprite; // background sprite
	unsigned short int gsprite2; // background sprite
	unsigned short int fsprite; // foreground sprite
	unsigned short int fsprite2; // foreground sprite

	unsigned int isprite; // item sprite
	unsigned short ic1, ic2, ic3;

	unsigned int flags; // see CMF_

	// character
	unsigned int csprite; // character base sprite
	unsigned int cn; // character number (for commands)
	unsigned char cflags; // character flags
	unsigned char action; // character action, duration and step
	unsigned char duration;
	unsigned char step;
	unsigned char dir; // direction the character is facing
	unsigned char health; // character health (in percent)
	unsigned char mana;
	unsigned char shield;
	// 15 bytes

	// effects
	unsigned int ef[4];

	unsigned char sink; // sink characters on this field
	int value; // testing purposes only
	int mmf; // more flags
	char rlight; // real client light - 0=invisible 1=dark, 14=normal (15=bright can't happen)
	struct complex_sprite rc;

	struct complex_sprite ri;

	struct complex_sprite rf;
	struct complex_sprite rf2;
	struct complex_sprite rg;
	struct complex_sprite rg2;

	char xadd; // add this to the x position of the field used for c sprite
	char yadd; // add this to the y position of the field used for c sprite
};

struct skill {
	char name[80];
	int base1, base2, base3;
	int cost; // 0=not raisable, 1=skill, 2=attribute, 3=power
	int start; // start value, pts up to this value are free
};

#define CL_MAX_SURFACE 32

struct client_surface {
	unsigned int xres : 14;
	unsigned int yres : 14;
	unsigned int type : 4;
};

struct client_info {
	unsigned int skip;
	unsigned int idle;
	unsigned int vidmemtotal;
	unsigned int vidmemfree;
	unsigned int systemtotal;
	unsigned int systemfree;
	struct client_surface surface[CL_MAX_SURFACE];
};

DLL_EXPORT extern struct map map[MAPDX * MAPDY];
DLL_EXPORT extern struct map map2[MAPDX * MAPDY];

DLL_EXPORT extern uint16_t value[2][V_MAX];
DLL_EXPORT extern int *game_v_max;
DLL_EXPORT extern int *game_v_profbase;
DLL_EXPORT extern uint32_t item[MAX_INVENTORYSIZE];
DLL_EXPORT extern uint32_t item_flags[MAX_INVENTORYSIZE];
DLL_EXPORT extern stat_t hp;
DLL_EXPORT extern stat_t mana;
DLL_EXPORT extern stat_t rage;
DLL_EXPORT extern stat_t endurance;
DLL_EXPORT extern stat_t lifeshield;
DLL_EXPORT extern uint32_t experience;
DLL_EXPORT extern uint32_t experience_used;
DLL_EXPORT extern uint32_t gold;
DLL_EXPORT extern tick_t tick;
extern int lasttick; // ticks in inbuf
extern int q_size;
extern uint64_t last_tick_received_time; // SDL_GetTicks() when last server tick batch was received
extern uint64_t tick_receive_interval; // Time between server tick batch arrivals (ms)

DLL_EXPORT extern unsigned int cflags; // current item (item under mouse cursor) flags
DLL_EXPORT extern unsigned int csprite; // and sprite

DLL_EXPORT extern int con_cnt;
DLL_EXPORT extern int con_type;
DLL_EXPORT extern char con_name[80];
DLL_EXPORT extern uint32_t container[MAX_CONTAINERSIZE];
DLL_EXPORT extern uint32_t price[MAX_CONTAINERSIZE];
DLL_EXPORT extern uint32_t itemprice[MAX_CONTAINERSIZE];
DLL_EXPORT extern uint32_t cprice;
DLL_EXPORT extern int protocol_version;

extern int looklevel;
DLL_EXPORT extern uint32_t mirror, newmirror;
extern int may_teleport[64 + 32];
DLL_EXPORT extern int pspeed; // 0=ill 1=stealth 2=normal 3=fast

DLL_EXPORT extern char username[40];
DLL_EXPORT extern char password[16];

DLL_EXPORT extern union ceffect ceffect[MAXEF];
DLL_EXPORT extern unsigned char ueffect[MAXEF];
DLL_EXPORT extern struct player player[MAXCHARS];

DLL_EXPORT extern uint16_t originx;
DLL_EXPORT extern uint16_t originy;

extern int sockstate;
extern time_t socktimeout;
DLL_EXPORT extern char *target_server;
extern uint16_t target_port;
extern int kicked_out;

extern char **game_skilldesc;
extern struct skill *game_skill;

DLL_EXPORT extern uint16_t act;
DLL_EXPORT extern uint16_t actx;
DLL_EXPORT extern uint16_t acty;

DLL_EXPORT extern uint32_t lookinv[12];
DLL_EXPORT extern uint32_t looksprite, lookc1, lookc2, lookc3;
DLL_EXPORT extern char look_name[80];
DLL_EXPORT extern char look_desc[1024];

DLL_EXPORT extern uint32_t realtime;
DLL_EXPORT extern uint32_t mil_exp;
extern unsigned int display_gfx;
extern uint32_t display_time;

DLL_EXPORT extern char pent_str[7][80];

DLL_EXPORT extern struct quest quest[];
DLL_EXPORT extern struct shrine_ppd shrine;

void cmd_text(char *text);
DLL_EXPORT map_index_t mapmn(unsigned int x, unsigned int y);
int find_cn_ceffect(int cn, int skip);
int find_ceffect(unsigned int fn);
DLL_EXPORT int level2exp(int level);
DLL_EXPORT int exp2level(int val);
DLL_EXPORT int raise_cost(int v, int n);
void cmd_move(int x, int y);
void cmd_swap(int with);
void cmd_use_inv(int with);
void cmd_take(int x, int y);
void cmd_look_map(int x, int y);
void cmd_look_item(int x, int y);
void cmd_look_inv(int pos);
void cmd_look_char(unsigned int cn);
void cmd_use(int x, int y);
void cmd_drop(int x, int y);
void cmd_speed(int mode);
void cmd_log(char *text);
void cmd_stop(void);
void cmd_kill(unsigned int cn);
void cmd_give(unsigned int cn);
void cmd_some_spell(int spell, int x, int y, unsigned int chr);
void cmd_raise(int vn);
void cmd_con(int pos);
void cmd_look_con(int pos);
void cmd_drop_gold(void);
void cmd_take_gold(uint32_t amount);
void cmd_junk_item(void);
void cmd_getquestlog(void);
void cmd_reopen_quest(int nr);
void cmd_fastsell(int with);
void cmd_con_fast(int pos);
void cmd_teleport(int nr);

int poll_network(void);
tick_t next_tick(void);
int do_tick(void);
void cl_client_info(struct client_info *ci);
void cl_ticker(void);
int close_client(void);
int is_char_ceffect(int type);

extern int change_area;
extern int login_done;
extern unsigned int unique;
extern unsigned int usum;

// Version 3.5 overlay text
#define MAXOTEXT 10

struct otext {
	char *text;
	tick_t time;
	int type;
};
extern struct otext otext[MAXOTEXT];

typedef enum {
	V3_HP = V_HP,
	V3_ENDURANCE = V_ENDURANCE,
	V3_MANA = V_MANA,
	V3_WIS = V_WIS,
	V3_INT = V_INT,
	V3_AGI = V_AGI,
	V3_STR = V_STR,
	V3_ARMOR = V_ARMOR,
	V3_WEAPON = V_WEAPON,
	V3_LIGHT = V_LIGHT,
	V3_SPEED = V_SPEED,
	V3_PULSE = V_PULSE,
	V3_DAGGER = V_DAGGER,
	V3_HAND = V_HAND,
	V3_STAFF = V_STAFF,
	V3_SWORD = V_SWORD,
	V3_TWOHAND = V_TWOHAND,
	V3_ARMORSKILL = V_ARMORSKILL,
	V3_ATTACK = V_ATTACK,
	V3_PARRY = V_PARRY,
	V3_WARCRY = V_WARCRY,
	V3_TACTICS = V_TACTICS,
	V3_SURROUND = V_SURROUND,
	V3_BODYCONTROL = V_BODYCONTROL,
	V3_SPEEDSKILL = V_SPEEDSKILL,
	V3_BARTER = V_BARTER,
	V3_PERCEPT = V_PERCEPT,
	V3_STEALTH = V_STEALTH,
	V3_BLESS = V_BLESS,
	V3_HEAL = V_HEAL,
	V3_FREEZE = V_FREEZE,
	V3_MAGICSHIELD = V_MAGICSHIELD,
	V3_FLASH = V_FLASH,
	V3_FIREBALL = V_FIREBALL,
	V3_REGENERATE = V_REGENERATE,
	V3_MEDITATE = V_MEDITATE,
	V3_IMMUNITY = V_IMMUNITY,
	V3_DEMON = V_DEMON,
	V3_DURATION = V_DURATION,
	V3_RAGE = V_RAGE,
	V3_COLD = V_COLD,
	V3_PROFESSION = V_PROFESSION
} v3_t;

typedef enum {
	V35_HP = 0,
	V35_ENDURANCE = 1,
	V35_MANA = 2,
	V35_WIS = 3,
	V35_INT = 4,
	V35_AGI = 5,
	V35_STR = 6,
	V35_ARMOR = 7,
	V35_WEAPON = 8,
	V35_OFFENSE = 9,
	V35_DEFENSE = 10,
	V35_LIGHT = 11,
	V35_SPEED = 12,
	V35_DAGGER = 13,
	V35_HAND = 14,
	V35_STAFF = 15,
	V35_SWORD = 16,
	V35_TWOHAND = 17,
	V35_ATTACK = 18,
	V35_PARRY = 19,
	V35_WARCRY = 20,
	V35_TACTICS = 21,
	V35_SURROUND = 22,
	V35_SPEEDSKILL = 23,
	V35_BARTER = 24,
	V35_PERCEPT = 25,
	V35_STEALTH = 26,
	V35_BLESS = 27,
	V35_HEAL = 28,
	V35_FREEZE = 29,
	V35_MAGICSHIELD = 30,
	V35_FLASH = 31,
	V35_FIRE = 32,
	V35_REGENERATE = 33,
	V35_MEDITATE = 34,
	V35_IMMUNITY = 35,
	V35_DEMON = 36,
	V35_DURATION = 37,
	V35_RAGE = 38,
	V35_COLD = 39,
	V35_PROFESSION = 40,

	V35_ILLEGAL = -1
} v35_t;

vval_t v_val(svval_t v);
svval_t sv_val(vval_t v);
void set_v35_inventory(void);
