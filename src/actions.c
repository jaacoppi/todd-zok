#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "locations.h"
#include "networking.h"
#include "globals.h"
#include "events.h"
#include "ui.h"
#include "weapons.h"
#include "skills.h"
#include "character.h"
#include "party.h"
#include "combat.h"
#include "database.h"

void set_player_location(Location* loc)
{
	player.location = loc;
	ncurs_location();
	ncurs_commands(); // TODO: this is drawn twice. Find out where. Without this line, 'v'iew stats (global) doesn't update commands
}

/* a modified set_player_location, doesn't draw description -> doesn't empty the screen*/
/* this function handles the keypress check: only returns if Y or N is pressed */
int set_player_loc_yesno() {
	wattron(game_win,A_BOLD);
	wprintw(game_win,_("Yes/No\n"));
	wattroff(game_win,A_BOLD);
	wrefresh(game_win);
	player.location = &loc_yesno;
	ncurs_commands();

	int loop = 1;
	while (loop) 
	{
		unsigned char keypress;
		todd_getchar(&keypress);
		if (keypress == 'y') 
			return 'y';

		if (keypress == 'n') 
			return 'n';		
	} 

return 0;
}

// this function takes the player to the dungeons
// the function can be accessed in three ways:
//	* from town to dungeons
//	* from dungeon level to another
//	* by pressing Run in a fight
// Dungeon level 0 == town
void ac_dungeons()
{
	switch(player.dungeon_lvl)
	{
		case 0:		// this is always from town to dungeon entrance
		{
			// make sure that player has atleast 1 skill before going to the dungeons
			// TODO: what if the player loses all the skills in combat and return to the dungeons main screen..

			int emptyskills = 0;
			for (int i = 0; i < 4; i++)	// If all skills are called "Unused", don't allow to enter dungeons
				if (strcmp(player.skill[i]->name, "Unused") == 0)
					emptyskills++;

			if (emptyskills == 4) // all skills are unused
			{
			wclear(game_win);
			// multiline message
			ncurs_modal_msg(_(
				"As you walk the path leading to the dungeons, you come by a squirrel.\n"
				"The squirrel throws a pine cone at you.\n"
				"\n"
				"You realize you know nothing about fighting, even with squirrels.\n"
				"\n"
				"The Gatekeeper did mention Bren and his warrior arena.."
			));
			}
			else
			{
				player.dungeon_lvl = 1;
				// TODO: the text should be different coming from town or coming from dungeon level 2
				set_player_location(&loc_dungeons_level1);
			}

			break;
		}
		case 1: 
		{
			player.dungeon_lvl = 2;
			set_player_location(&loc_dungeons_level2);

			break;
		}
		default:
			break;

	}	
}


void ac_dungeons_action()
{
	if (player.stamina > 0)
	{
		player.stamina--;

		// check if you stumble upon...
			// a player corpse
			// a random event
		if (check_rnd_events() != 1) {
			set_player_location(&loc_fight);
			create_enemy();
			ncurs_log_sysmsg(_("You encounter a %s!"), enemy.name);

			ncurs_fightinfo(&player, 0);
			ncurs_fightinfo(&enemy, 3);
		}
	}
	else
	{
		ncurs_log_sysmsg(_("You feel too tired to fight"));
	}
}

/* helper functions so I don't need to rewrite that much code */
void ac_fight_0() {
	use_skill(0);
}

void ac_fight_1() {
	use_skill(1);
}

void ac_fight_2() {
	use_skill(2);
}

void ac_fight_3() {
	use_skill(3);
}

void ac_dungeons_glow()
{
	set_player_location(&loc_shrine);
}

void ac_shrine_heal_1()
{
	if (player.health < player.max_health)
	{
		if (player.money > 0)
		{
			player.health++;
			player.money--;
			ncurs_log_sysmsg(_("%s was healed for 1 hitpoint"),player.name);
			ncurs_modal_msg(_("\nYou flip the coin into the pond, wishing for world peace."));
		}
		else
		{
			ncurs_modal_msg(_("\nYou search your pockets for coin, but find none."));
		}
	}
	else /* hp full already */
	{
		player.money--;
		ncurs_modal_msg(_("\nYou flip a coin in the pond.\nA crow catches the coin mid-air. Damn.."));
		ncurs_log_sysmsg(_("\nA crow stole a coin from %s"),player.name);
	}
}

void ac_shrine_heal_all()
{
	if (player.health < player.max_health)
	{
		if (player.money >= 5)
		{
			player.health += 10;
			if (player.health > player.max_health)
				player.health = player.max_health;
			player.money -= 5;
			ncurs_log_sysmsg(_("%s was healed for 10 hitpoints"),player.name);
			ncurs_modal_msg(_("\nYou flip 5 coins into the pond, wishing for world peace."));
		}
		else
		{
			ncurs_modal_msg(_("\nYou search your pockets for coin, but find none."));
		}
	}
	else /* hp full already */
	{
		player.money--;
		ncurs_log_sysmsg(_("A crow stole a coin from %s"),player.name);
		ncurs_modal_msg(_("\nYou flip a coin in the pond.\nA crow catches the coin mid-air. Damn.."));
	}
}

void ac_list_players()
{
	werase(game_win);
	wprintw(game_win, _("Players roaming the fields:\n\n"));
	wrefresh(game_win);
        PGresult *res;
        res = PQexecPrepared(conn, "list_online_players", 0, NULL, NULL, NULL, 0);

	wattron(game_win, A_BOLD);
        // TODO: error handling
        if (PQresultStatus(res) == PGRES_TUPLES_OK)
        {
                int row_count = PQntuples(res);
                for (int i = 0; i < row_count; i++) // loop through all the players
                {
		char *player_name = PQgetvalue(res,i,0);
		int location = atoi(PQgetvalue(res,i,1));
		if (location == LOC_ONLINE) // only list online players
			wprintw(game_win, "%s\n",player_name);
		}
	wattroff(game_win, A_BOLD);
	wrefresh(game_win);
	
 	}
	PQclear(res);

}

void ac_view_stats()
{
	// It looks ugly here, but correct ingame
	werase(game_win);
	wrefresh(game_win);
	wprintw(game_win,"%8s: %10s\n", _("Name"), player.name);
	wprintw(game_win,"%8s: %10d\n", _("Stamina"), player.stamina);
	wprintw(game_win,"%8s: %10d\n", _("XP"), player.experience);
	wprintw(game_win,"%8s: %10d\n", _("Money"), player.money);
	wprintw(game_win,"%8s: %7d/%2d\n", _("Health"), player.health,player.max_health);
	wattron(game_win,A_BOLD);
	wprintw(game_win,"\n%s:\n", _("Elements"));
	wattroff(game_win,A_BOLD);
	for (int i = 0; i < ELEM_COUNT; i++)
	{
		wprintw(game_win,"%8s: %10d\n", element_names[i], player.elements[i]);
	}
	wprintw(game_win,"\n");
	
	wprintw(game_win,"%8s: %10s (%s)\n", _("Weapon"), player.weapon->name,element_names[player.weapon->dmg_type]);
	for (int i = 0; i < 4; i++)
	{
		wprintw(game_win,"%6s %d: %10s\n", _("Skill"), i+1, player.skill[i]->name);
	}
	wprintw(game_win,"\n");


	// only display party stats if the player is in a party
	
	if (player_party.id != 0)
	{
		wprintw(game_win, "Party id: %d\n", player_party.id);
		wprintw(game_win, "Party name: %s\n", player_party.name);
	}
	else
		wprintw(game_win, "Player is not in a party");

	wrefresh(game_win);

	/* if stats are viewed during a fight, getch() and display char info again */
	/* TODO: player and enemy hardcoded to 0 and 3 */
	if (player.location == &loc_fight) {
		wprintw(game_win,"\n\n<MORE>");
		wrefresh(game_win);
		todd_getchar(NULL);
		werase(game_win);
		wrefresh(game_win);
		ncurs_fightinfo(&player, 0);
		ncurs_fightinfo(&enemy, 3);
	}
}

void ac_tavern()
{
	set_player_location(&loc_tavern);
}

void ac_tavern_bartender()
{
	set_player_location(&loc_tavern_bartender);
}

void ac_tavern_room()
{
	werase(game_win);
	wprintw(game_win,_("TODO: add player location to the database\n"));
	wprintw(game_win,_("TODO: pay for the room\n"));
	wprintw(game_win,_("TODO: possible random events: robbers / prostitutes\n"));
	wprintw(game_win,"\n");

	wprintw(game_win,_("Would you like to rent a room?\n"));
	wrefresh(game_win);

	/* TODO: testing yesno -dialog, make this better if need be */
	int keypress = set_player_loc_yesno();
	if (keypress == 'y') 
	{
		/* TODO: find out why modal_msg doesn't work here */
		wprintw(game_win,_("You leave your weapon by your bed and go to sleep..\n"));
		wprintw(game_win,_("Continue...\n"));
		wrefresh(game_win);
		getch();
		/* TODO: set_player_location wclears game_win, does it matter? */
		/* you could set_player_location in save_player_data (move the bard thing earlier */
		/* etc etc */
		set_player_location(&loc_room_offline);
		playing = false;

		/* store info to database: player_stats.location should now be LOC_OFFLINE_ROOM */
		/* this knowledge is used by dailybot */

		db_player_location(LOC_OFFLINE_ROOM);

	}

	if (keypress == 'n') 
	{
	ncurs_modal_msg(_("Don't waste my time again!"));
	set_player_location(&loc_tavern_bartender);
	} 
}

void ac_tavern_info()
{
	/* TODO:
	information about
	1. other players
	2. stuff in the world (tutorial/help: bartender knows everything!)
	3. random stuff
	*/
	werase(game_win);
	ncurs_modal_msg("TODO: Implement\nNOTE THAT STATS HAVE BEEN UPDATED");
 	player.health = player.max_health;
	player.elements[0] = 5;
	player.elements[1] = 5;
	player.elements[2] = 5;
	player.elements[3] = 5;
	player.elements[4] = 5;
	player.money += 100;
	player.stamina = 100;
 
}


// list current parties
void ac_party_list()
{
	werase(game_win);

        PGresult *res;
        res = PQexecPrepared(conn, "list_parties", 0, NULL, NULL, NULL, 0);
        if (PQresultStatus(res) == PGRES_TUPLES_OK)
        {
		int row_count = PQntuples(res);
		if (row_count > 0) // means there's parties
			{
			for (int i = 0; i < row_count; i++)
				{
				wprintw(game_win,"#%d: %s\n", atoi(PQgetvalue(res,i,0)), PQgetvalue(res,i,1));
				wprintw(game_win,_("Members: %s	TODO: the rest..\n\n"),PQgetvalue(res,i,2));
				}
			} 

		else
			wprintw(game_win, _("No parties..\n"));
	}
	wrefresh(game_win);

}

// joins an existing party
void ac_party_join()
{
// TODO: see if there's room in a party
// IF yes: join

// IF no: explain to the user
}

void ac_party_gather()
{

	ncurs_log_sysmsg("What would you like your party to be called?");
	char *line = NULL;
	size_t len = 0;
	wrefresh(input_win); // this is here to move the visible cursor to input_win instead of log_win
	if(!todd_getline(&line, &len)) return;

	player_party.name = line;

	// insert a new party into databse, with an automatic id number and the only member being player
	char *playerid = itoa(player.id);
        const char *params[2] = {player_party.name, playerid};
        PGresult *res;
        res = PQexecPrepared(conn, "new_party", 2, params, NULL, NULL, 0);
        if (PQresultStatus(res) == PGRES_TUPLES_OK)
        {
	        int partyid = atoi(PQgetvalue(res, 0, 0));
		set_party(partyid);
		ncurs_log_sysmsg("Subscribed to %d", player_party.id);

		// TODO: make this a chatmsg by Willie?
		ncurs_log_sysmsg(_("Your party shall be called %s"), player_party.name);
	}
	PQclear(res);
	// TODO: else -> error handling
}

void ac_warena()
{
	set_player_location(&loc_warena);
}

void ac_warena_skills()
{
	/* The function is exactly the same than in shop_buy, could these be combined? */
	wprintw(game_win,"\n%s\n", _("Bren can teach you one of these skills:"));
	int selection = ncurs_listselect(&(skills_list[0].name), sizeof(Skills), 0, SKILLS_COUNT-1);
	if (selection >= 0)
	{
		int slot = check_for_skill_slots(selection);
		if (slot >= 0)
		{
			player.skill[slot] = &skills_list[selection];
			ncurs_skills();
			ncurs_log_sysmsg(_("%s learned a new skill: %s"), player.name, player.skill[slot]->name);
			ncurs_modal_msg(_("YOU JUST LEARNED: %s"), player.skill[slot]->name);
		}
		else
		{
			ncurs_modal_msg(_("Your loss, buddy!"));
		}
	}
	else
	{
		ncurs_modal_msg(_("Your loss, buddy!"));
	}
}	

void ac_shop()
{
	set_player_location(&loc_shop);
}

void ac_shop_buy()
{
	werase(command_win);
	wrefresh(command_win);
	wprintw(game_win,"\n%s\n", _("\nThe poor man is selling these items:\n\n"));
	wattron(game_win, A_BOLD);
	wprintw(game_win,"Weapon\t\t\tPrice\n");
	wattroff(game_win, A_BOLD);
	wprintw(game_win,"\n");

 	int selection = ncurs_listselect(&(weapons_list[0].name), sizeof(Weapons), (void*)&(weapons_list[0].price) - (void*)&(weapons_list[0].name), WEAPON_COUNT-1);
	if (selection >= 0)
	{ 	
		/* see if player has the money for it */
		if (player.money >= weapons_list[selection].price) {
			player.weapon = &weapons_list[selection];
			player.money = player.money - weapons_list[selection].price;
			ncurs_log_sysmsg(_("%s bought %s for %d"),player.name,player.weapon->name,player.weapon->price);
			ncurs_modal_msg(_("YOU JUST BOUGHT: %s"), player.weapon->name);
			ncurs_skills(); /* update Weapon & Skills -listing */
		}
		else
		{
			ncurs_modal_msg(_("Come back when you have the money for it!"));
		}

	}
	else
	{
		ncurs_modal_msg(_("May the gods curse you for wasting my time, mutters the old man."));
	}
}

void ac_shop_sell()
{
	if (player.weapon->index == 999) // it's bare hands, forget it
	{
	ncurs_modal_msg(_("\n\nAre you really thinking of selling me your hands?\nI don't think I have any use for them."));
	set_player_location(&loc_shop);
	return;
	}
	
	wclear(game_win);
	// currently always offer 60% of the weapon price
	int price = player.weapon->price * 0.60;
	wprintw(game_win,_("The old man is willing to buy your %s for %d\n"),player.weapon->name,price);
	wrefresh(game_win);
	int keypress = set_player_loc_yesno();
	
	if (keypress == 'y') 
	{
		ncurs_modal_msg(_("Nice doing business with you!"));
		player.money += price;
		
		/* find out the correct index for Bare hands */
		int i;
		for (i = 0; i <= WEAPON_COUNT; i++)
			if (weapons_list[i].index == 999)
				break;
		player.weapon = &weapons_list[i]; /* bare hands */
		ncurs_skills();
	}
	else // 'n'
	{
		ncurs_modal_msg(_("May the gods curse you for wasting my time, mutters the old man."));
	}
 
set_player_location(&loc_shop);
}

void ac_messageboard()
{
	set_player_location(&loc_messageboard);
}

void ac_messageboard_view()
{
	wclear(game_win);
	PGresult *res;
	res = PQexecPrepared(conn, "view_messageboard", 0, NULL, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		int row_count = PQntuples(res);
		// int col_count = PQnfields(res); /* NOT USED ANYWHERE, IS THIS NEEDED? */
		for (int i = 0; i < row_count; i++)
		{
			wprintw(game_win, "%s // ", PQgetvalue(res, i, 0));
			wprintw(game_win, "%s\n", PQgetvalue(res, i, 1));
			wprintw(game_win, "%s ", PQgetvalue(res, i, 2));
			wprintw(game_win, "\n\n");
		}
	}
	wrefresh(game_win);
	PQclear(res);
}

extern char* itoa(int i);
void ac_messageboard_write()
{
ncurs_bold_input(999);
/* echo, noecho and wgetnstr(input_win) enable users to see what they write */
	int len = 80;
	char *line = malloc(len); // more dynamic memory allocation would be nice
	echo();
	if (wgetnstr(input_win,line, len) != ERR) // TODO a better way to get input
	{
		char *player_id = itoa(player.id);
		const char *params[2] = {player_id, line};
		PGresult *res;
		res = PQexecPrepared(conn, "write_to_messageboard", 2, params, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK)
		{
			syslog(LOG_WARNING, "Messageboard write failed: %s", PQresultErrorMessage(res));
		}
		PQclear(res);
		free(player_id);
	}
	free(line);
	noecho();
	ncurs_bold_input(0);
	
	werase(game_win);
	ncurs_modal_msg(_("Your message has been written.\nYou start to wonder if it was worth it."));
}

void ac_graveyard()
{
	set_player_location(&loc_graveyard);


}

// High score list, called graveyard in game
void ac_graveyard_view()
{
	wclear(game_win);
	wprintw(game_win,_("Here lie the following adventurers:\n\n"));
	wattron(game_win, A_BOLD);
	wprintw(game_win,"Name		TODO: rank high scores\n");
	wattroff(game_win, A_BOLD);

	const char *graveyard = itoa(LOC_DEAD_GRAVEYARD);
	const char *params[1] = {graveyard};
	PGresult *res;
	res = PQexecPrepared(conn, "view_graveyard", 1, params, NULL, NULL, 0);

        if (PQresultStatus(res) == PGRES_TUPLES_OK)
        {
                int row_count = PQntuples(res);
                for (int i = 0; i < row_count; i++) // loop through all the players
                {
			wprintw(game_win, "%s\n",PQgetvalue(res,i,0));
		}
	wattroff(game_win, A_BOLD);
	wrefresh(game_win);
	
 	}
	PQclear(res);

	wrefresh(game_win);

}

// the name is currently misleading: returns -1 dungeon levels
void ac_return_to_town()
{
	switch (player.dungeon_lvl)
	{
		case 0: // already in town, return from Tavern, shop and so on

		case 1: // to town
		{
		player.dungeon_lvl = 0;
		set_player_location(&loc_town);
		break;
		}

		case 2: // to first dungeon level
		{
		player.dungeon_lvl = 1;
		set_player_location(&loc_dungeons_level1);
		break;
		}

		default:
			break;
	}
}


void ac_quit()
{
	werase(game_win);
	/* using modal_msg here would force the user to waste a keypress */
	wprintw(game_win,_("You leave the town, wondering what treasures you left behind in the dungeons\n"));
	wrefresh(game_win);

	/* set player location in database to LOC_OFFLINE_FIELDS */
	db_player_location(LOC_OFFLINE_FIELDS);
	playing = false;
}

// DO NOTHING.
// Used for view_stats when it needs to be displayed on-screen but it's actually done in todd_getchar
void ac_blank()
{
}
