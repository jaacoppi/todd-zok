#ifndef GLOBALS_H
#define GLOBALS_H

#include <libpq-fe.h>
#include <ncurses.h>
#include <libintl.h>
#include "character.h"
#include "input.h"
#include "party.h"
#include "combat.h"

#define _(x) gettext(x)
#define NAME_MIN_LENGTH 4
#define NAME_MAX_LENGTH 16
#define STAMINA_MAX 100	/* game configuration variable. Sleeping in a room recovers 72 in 24 hours */

extern int init_pq();
extern void cleanup_pq();
extern void set_party();
extern char *itoa();

// ugly globals go here
extern int playing;
extern void *push_socket;
extern void *chat_socket;
extern void *party_socket;
extern void *ctrl_socket;
extern void *zmq_context;
extern Character player;
extern Party player_party;
extern Character partymember1;
extern Character partymember2;
extern int randomseed;
extern Character enemy;
extern PGconn *conn;

extern char g_partyname[30];
extern int partyid_global;
extern Weapons weapons_list[];
extern Weapons weapons_enemy[];
extern Character enemylist[2][ENEMY_COUNT];
extern Skills skills_list[];
extern Skills unused_skill;
extern WINDOW *game_win;
extern WINDOW *input_win;
extern WINDOW *background_win;
extern WINDOW *skills_win;
extern WINDOW *command_win;
extern int y_size;
extern int gamew_logw_sep;

extern bool is_online (int id);
#endif
