#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdbool.h>
#include <time.h> /* for random number generator */
#include <unistd.h>

#include <zmq.h>
#include <libpq-fe.h>

#include "action.h"
#include "location.h"
#include "locations.h"
#include "networking.h"
#include "ui.h"
#include "weapons.h"
#include "globals.h"
#include "character.h"
#include "database.h"

#define RETRY_LIMIT 3

// ugly globals go here
int playing;
PGconn *conn;

char g_partyname[30]; // needed if party.name is a pointer?

/*
	Asks the player for name
*/
bool get_name()
{
	char *name = NULL;
	size_t name_len = 0;

	// Introductory text
	// you need the \r\n at the start because ZMQ might cause some clutter
	printw(_("\r\nGrowing up, you heard the local bards sing about ancient dungeons.\n"));
	printw(_("The dungeons were filled with unimaginable treasures...and monsters.\n"));
	printw(_("\nThat's if you believed the bards. You are now approaching a village next to the dungeons, about to find out.\n"));

	printw(_("\nAs you approach the gated village, the Gatekeeper yells at you:\n"));
	printw(_("HALT! Who goes there? Annouce yourself: "));
	wrefresh(stdscr);

	// get user input
        if(!todd_getline(&name, &name_len,stdscr))
		{
		syslog(LOG_ERR, "Read error: %s (%s:%d)\r\n", strerror(errno), __FILE__, __LINE__);
		return false;
		}

// not needed since the move to todd_getline instead of getline
//	name[name_len - 1] = 0; // strip trailing newline
	if (strnlen(name, name_len) < name_len - 1)
	{
		// null bytes in name
		syslog(LOG_ERR, "Player name contains \\0\r\n");
		return false;
	}

	player.name = name;
	return true;
}

bool get_id()
{
	bool ret = false;
	const char *params[1] = {player.name};
	PGresult *res;
	res = PQexecPrepared(conn, "get_login_id", 1, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (PQntuples(res) == 1)
		{
			// TODO get integers directly without atoi
			player.id = atoi(PQgetvalue(res, 0, 0));  
			ret = true;
		}
	}
	PQclear(res);
	return ret;
}

bool check_passwd()
{
	bool ret = false;
	char *passwd = NULL;
	size_t passwd_len = 0;

	// the gatekeeper asks for the password
	wprintw(stdscr,_("\nIf you're really %s you'll remember the secret code word we agreed on last time!\n"),player.name);
	wprintw(stdscr, _("Tell me the the secret code word and I'll let you pass: "));
	wrefresh(stdscr);

        if(!todd_getline(&passwd, &passwd_len,NULL))
                {
                syslog(LOG_ERR, "Read error: %s (%s:%d)\r\n", strerror(errno), __FILE__, __LINE__);
                return false;
                }

	char *player_id = itoa(player.id);
	const char *params[2] = {player_id, passwd};
	PGresult *res;
	res = PQexecPrepared(conn, "check_passwd", 2, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (PQntuples(res) != 0)
		{
			ret = true;
		}
	}
	PQclear(res);
	free(passwd);
	free(player_id);
	return ret;
}

void load_player_data(Character *load_plr)
{

        PGresult *res;
	char *player_id = itoa(load_plr->id);
	const char *params[1] = {player_id};
	res = PQexecPrepared(conn, "load_player", 1, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		int row_count = PQntuples(res);
		int col_count = PQnfields(res);
		if (row_count > 0)
		{
			if (row_count > 1)
			{
				syslog(LOG_WARNING, "Duplicate player data found. id = %d, name = %s, row count %d\r\n", load_plr->id, load_plr->name, row_count);
			}
			// load data from first row even if there is multiple rows
			load_plr->location = &loc_town; // TODO location _should_ be fetched from db!
			int col_cursor = 0;
			
			// TODO figure out correct length
			// both for malloc and copy (use memcpy instead of strncpy)
			// TODO free this somewhere?
			load_plr->name = malloc(NAME_MAX_LENGTH);
			strncpy(load_plr->name, PQgetvalue(res,0,col_cursor++), NAME_MAX_LENGTH);


			load_plr->stamina = atoi(PQgetvalue(res, 0, col_cursor++));
			load_plr->experience = atoi(PQgetvalue(res, 0, col_cursor++));
			load_plr->money = atoi(PQgetvalue(res, 0, col_cursor++));
			load_plr->health = atoi(PQgetvalue(res, 0, col_cursor++));
			load_plr->max_health = atoi(PQgetvalue(res, 0, col_cursor++));
			for (int i = 0; i < ELEM_COUNT; i++)
			{
				load_plr->elements[i] = atoi(PQgetvalue(res, 0, col_cursor++));
			}

			/* id contains the weapon id. Loop through struct weapon to find the corresponding weapon */
			int temp_id = atoi(PQgetvalue(res,0,col_cursor++));
			int i;
			for (i = 0; i <= WEAPON_COUNT; i++)
				if (weapons_list[i].index == temp_id)
					break;

			load_plr->weapon = &weapons_list[i];;

			/* Do the same for skills */
			for (int i = 0; i < 4; i++)
			{
			int temp_skill_id = atoi(PQgetvalue(res,0,col_cursor++));
			int temp_i;
			for (temp_i = 0; temp_i <= SKILLS_COUNT; temp_i++)
				if (skills_list[temp_i].index == temp_skill_id)
					break;

			load_plr->skill[i] = &skills_list[temp_i];
			}


			load_plr->dungeon_lvl = atoi(PQgetvalue(res, 0, col_cursor++));
			if (col_cursor != col_count)
			{
				syslog(LOG_WARNING, "col_cursor: %d != col_count: %d\r\n", col_cursor, col_count);
			}
		}
		else
		{
			syslog(LOG_WARNING, "Player data not found. Player id = %d, name = %s\r\n", load_plr->id, load_plr->name);
		}
	}
	else
	{
		syslog(LOG_WARNING, "Player data load failed: %s\r\n", PQresultErrorMessage(res));
	}
	free(player_id);
	PQclear(res);
}

void save_player_data()
{
	char *player_id = itoa(player.id);
	char *stamina = itoa(player.stamina);
	char *experience = itoa(player.experience);
	char *money = itoa(player.money);
	char *health = itoa(player.health);
	char *max_health = itoa(player.max_health);
	char *elem_0 = itoa(player.elements[0]);
	char *elem_1 = itoa(player.elements[1]);
	char *elem_2 = itoa(player.elements[2]);
	char *elem_3 = itoa(player.elements[3]);
	char *elem_4 = itoa(player.elements[4]);

	// TODO get correct indices

	int weapon_id = player.weapon->index;
	int skills_id[3];
	skills_id[0] = player.skill[0]->index;
	skills_id[1] = player.skill[1]->index;
	skills_id[2] = player.skill[2]->index;
	skills_id[3] = player.skill[3]->index;

	char *weapon = itoa(weapon_id);
	char *skill_0 = itoa(skills_id[0]);
	char *skill_1 = itoa(skills_id[1]);
	char *skill_2 = itoa(skills_id[2]);
	char *skill_3 = itoa(skills_id[3]);

	char *dungeon_lvl = itoa(player.dungeon_lvl); /* TODO: permadeath, this shouldn't be needed */
	const char *params[17] = {
		player_id,
		stamina,
		experience,
		money,
		health,
		max_health,
		elem_0,
		elem_1,
		elem_2,
		elem_3,
		elem_4,
		weapon,
		skill_0,
		skill_1,
		skill_2,
		skill_3,
		dungeon_lvl
	};
	PGresult *res;
	res = PQexecPrepared(conn, "save_player", 17, params, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		syslog(LOG_WARNING, "Player data save failed: %s\r\n", PQresultErrorMessage(res));
	}

	const char *logout[1] = { player_id };
	res = PQexecPrepared(conn, "save_player_logout_time", 1, logout, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		syslog(LOG_WARNING, "Player data save failed: %s\r\n", PQresultErrorMessage(res));
	}
	free(player_id);
	free(stamina);
	free(experience);
	free(money);
	free(health);
	free(max_health);
	free(elem_0);
	free(elem_1);
	free(elem_2);
	free(elem_3);
	free(elem_4);
	free(weapon);
	free(skill_0);
	free(skill_1);
	free(skill_2);
	free(skill_3);
	free(dungeon_lvl);
	PQclear(res);
	wprintw(game_win,_("\nThe bards succesfully recorded your heroic deeds today.\n"));
	wrefresh(game_win);
}

bool check_name()
{
	size_t len = 0;
	char *it = player.name;
	for (char c = *it++; c != '\0'; c = *it++)
	{
		len++;
		if (c < 'a' || c > 'z')
			if (c < 'A' || c > 'Z')
				if (c != ' ' && c != '-')
					return false;
	}
	if (len < NAME_MIN_LENGTH)
		return false;
	if (len > NAME_MAX_LENGTH)
		return false;
	return true;
}

bool create_player()
{
	bool ret = false;
	if (!check_name())
	{
		printw(_("TODO: make this part \"realm-like\""));
		printw(_("Invalid name. Allowed characters: A-Z, a-z, dash and space."));
		printw(_("Minimum name length: %d characters.\n"), NAME_MIN_LENGTH);
		wrefresh(stdscr);
		return ret;
	}
	char *passwd = NULL;
	size_t passwd_len = 0;

	// New player joining, let the Gatekeeper to the talking

	printw(_("\nNever heard of you, %s\n"),player.name);
	printw(_("You look like a worthless coward to me. I bet my supper that you are a no good fighter.\n"));
	printw(_("That's right. People from all around the kingdom come here because of the dungeons. "));
	printw(_("Some never leave, you know.\n"));
	printw(_("If you're really serious about meeting an untimely death you should at least talk to Bren first.\n"));
	printw(_("Bren can teach you to take care of yourself. Meet him at his arena in the village.\n"));

	printw(_("\nIn case you run away to your mother and she tells you to come back and die in honor, I want to recognize you.\n"));
	printw(_("Tell me a secret code word so I'll know you the next time you arrive: "));
	wrefresh(stdscr);


	// ask for password
	if(!todd_getline(&passwd, &passwd_len,NULL))
		{
		syslog(LOG_ERR, "Read error: %s (%s:%d)\r\n", strerror(errno), __FILE__, __LINE__);
	 	return false;
		}


// not needed since the move to todd_getline instead of getline
//	passwd[line_len-1] = '\0'; // strip newline
	// TODO ask password for a second time to avoid typos
	const char *params[2] = {player.name, passwd};
	PGresult *res;
	res = PQexecPrepared(conn, "new_login", 2, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		int row_count = PQntuples(res);
		if (row_count == 1)
		{
			player.id = atoi(PQgetvalue(res, 0, 0));
			PQclear(res);
			char *player_id = itoa(player.id);
			const char *params_stats[1] = {player_id};
			res = PQexecPrepared(conn, "new_player_stats", 1, params_stats, NULL, NULL, 0);
			if (PQresultStatus(res) == PGRES_COMMAND_OK)
			{
				ret = true;
			}
			else
			{
				syslog(LOG_WARNING, "Player data create failed: %s\r\n", PQresultErrorMessage(res));
			}
			free(player_id);
		}
		else
		{
			syslog(LOG_WARNING, "Player create failed: no id returned\r\n");
		}
	}
	else
	{
		syslog(LOG_WARNING, "Player create failed: %s\r\n", PQresultErrorMessage(res));
	}
	PQclear(res);
	free(passwd);
	return ret;
}

/*
	Loads player from disk or if player is not found, creates a new player.
*/
bool get_player()
{
	if (!get_name())
	{
		return false;
	}
	if (get_id())
	{
		for (int retries = 0; retries < RETRY_LIMIT; retries++)
		{
			sleep(1);
			if (check_passwd())
			{
				return true;
			}
			wprintw(stdscr,_("\nIncorrect password.\n"));
			wrefresh(stdscr);
		}
		return false;
	}
	else	// get_id failed, that means it's a new player joining!
	{
		return create_player();
	}
}


/*
	Tries to find and call the action user requested
*/
void execute_action(unsigned char cmd_char)
{
	//convert upper case to lower case
	if (cmd_char < 'a' && cmd_char > '9') /* accept letters and numbers */
	{
		cmd_char += 'a' - 'A';
	}

	// search for the corresponding action
	for (size_t i = 0; i < player.location->action_count; i++)
	{
		if (player.location->actions[i].hotkey == cmd_char)
		{
			// execute action
			player.location->actions[i].function();
			break;
		}
	}
}


/*
	Main gameloop here
*/
void enter_game()
{
	// send a join msg to everybody
	send_join_msg();
	ncurs_location();
	playing = true;
	while (playing)
	{
		unsigned char cmd_char;
		bool getch_res = todd_getchar(&cmd_char);
		if (getch_res == false)
		{
			//eof or other read error
			playing = false;
			break;
		}
		execute_action(cmd_char);
	}

}


/*
	Main entry point. Initializes global resources, logs player on 
	and enters game.
*/
int main(int argc, char *argv[])
{
	init_ncurs();	// needed for all the printing, so must be done first
	intro_ascii();
	int return_code = EXIT_FAILURE;
	openlog("ToDD", LOG_PID|LOG_PERROR, LOG_USER);
	srand((unsigned int)time(NULL));

	if (!init_pq())
	{
		goto cleanup;
	}

	if (!init_zmq())
	{
		syslog(LOG_ERR, "ZeroMQ init failure: %s\r\n", zmq_strerror(errno));
		goto cleanup;
	}

	if (!zmq_python_up())
	{
		syslog(LOG_ERR, "Python chat server not responding!\r\n");
		syslog(LOG_ERR, "Ensure that server is running and try again!\r\n");
		goto cleanup;
	}

	if (!get_player())
	{
		syslog(LOG_ERR, "Player auth failure\r\n");
		goto cleanup;
	}

	/* set the player location to "ONLINE" */
	db_player_location(LOC_ONLINE); 

	load_player_data(&player);

	// load_player_data can be run for party members on the run, so..
	// reset player dungeon level to 0 (town) since we're logging in
	player.dungeon_lvl = 0;


	// get player party id
        PGresult *player_partyres;
	char *playerid = itoa(player.id);
	const char *params[1] = {playerid};
        player_partyres = PQexecPrepared(conn, "load_player_party", 1, params, NULL, NULL, 0);
        if (PQresultStatus(player_partyres) == PGRES_TUPLES_OK)
        {
		int row_count = PQntuples(player_partyres);
		if (row_count > 0) // there's parties
                {
			// due to the select statement, this should only return 1 row
			player_party.id = atoi(PQgetvalue(player_partyres,0,0));

			// TODO: if party.name is a pointer, this trick is needed
			strncpy(g_partyname, PQgetvalue(player_partyres,0,1), sizeof(PQgetvalue(player_partyres,0,1)));
			player_party.name = g_partyname;

			// load party member id's, then use load_player_data for them

			// TODO: this assumes that player is always first in the list, not true

			partymember1.id = atoi(PQgetvalue(player_partyres,0,3));
			partymember2.id = atoi(PQgetvalue(player_partyres,0,4));

                }
		else // there's no parties, load defaults
		{
			player_party.id = 0;
			player_party.name = "No party";
		}
        }
	else
	{ // something went wrong, load defaults
			player_party.id = 0;
			player_party.name = "No party";
	}
        PQclear(player_partyres);

	// subscribe to party chat channel 
	sub_party(player_party.id);

	// load the party member details to Character partymember1 and 2
	// ..only if you have partymembers
	if (partymember1.id != 0)
		load_player_data(&partymember1);

	if (partymember2.id != 0)
		load_player_data(&partymember2);


	// load the actual game
	init_ui();
	enter_game();
	
	// on exit, save player data
	save_player_data();

	return_code = EXIT_SUCCESS; // returned from game, success

cleanup:
	// send a quit msg to everybody
	send_quit_msg();

	cleanup_zmq();
	cleanup_pq();
	player.name = NULL; /* without this, free(player.name) segfaults. TODO: figure this one out */
	free(player.name);
	closelog();
	endwin(); /* for curses */
	printf("\n"); /* cleaner exit: user console is undistorted by ncurses stuff */
	return return_code;
}


