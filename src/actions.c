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
#include "actions.h"

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
			// party_call_to_arms call all online partymembers to join the fight automatically
			// if you're alone, it does nothing
			party_call_to_arms();

			// currently the client who sends the call to arms also receives it, so everyone will be at ac_fightscreen() soon
		}
	}
	else
	{
		ncurs_log_sysmsg(_("You feel too tired to fight"));
	}
}

// TODO: rename
void ac_fightscreen()
{
	// since the party should have the same random seed, all party members will end up with the same random enemy/enemies

	// for now, only allow multiple enemies on party combat
	// TODO: balance the game
	if (combat_ismulti())
		{
		// random 1-3
		int enemy_number = rand() % 2;
		// create enemies
		for (int i = 0; i <= enemy_number; i++)
			{
			create_enemy(enemy_party.characters[i]);
			ncurs_log_sysmsg(_("You encounter a %s!"), enemy_party.characters[i]->name);
			}
		}
	else	// single player, only one enemy
		{
		create_enemy(enemy_party.characters[0]);
		ncurs_log_sysmsg(_("You encounter a %s!"), enemy_party.characters[0]->name);
		}

	// set the incombat mode for everybody online
	for (int i = 0; i <= 2; i++)
		if (is_online(player_party.characters[i]->id))
			player_party.characters[i]->incombat = true;


	// for commands - must be before ac_update_fightscreen (clears the screen)
	set_player_location(&loc_fight);

	// display fighters
	ac_update_fightscreen();
	
}

void ac_update_fightscreen()
{
// display stats for the players / enemies participating

for (int i = 0; i <= 2; i++)
	if (enemy_party.characters[i]->incombat)
		ncurs_fightinfo(enemy_party.characters[i], 3+i);     

for (int i = 0; i <= 2; i++)
	if (player_party.characters[i]->incombat)
		ncurs_fightinfo(player_party.characters[i], i);     


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
		wprintw(game_win, _("Party name: %s\n"), player_party.name);
		wprintw(game_win, _("Party members:\n"));

		// print out party member names and <ONLINE> if they are
		if (partymember1.id != 0)
			{
			wprintw(game_win, "%s",partymember1.name);
			if (is_online(partymember1.id))
				wprintw(game_win, _(" <ONLINE>"));
			wprintw(game_win, "\n");
			}

		if (partymember2.id != 0)
			{
			wprintw(game_win, "%s",partymember2.name);
			if (is_online(partymember2.id))
				wprintw(game_win, _(" <ONLINE>"));
			wprintw(game_win, "\n");
			}
	}
	else
		wprintw(game_win, "Player is not in a party");

	// debug: rand test
	for (int i = 0; i < 10; i++)
		wprintw(game_win, "%d / ", rand() % 1000);

	wrefresh(game_win);

	/* if stats are viewed during a fight, getch() and display char info again */
	if (player.location == &loc_fight) {
		wprintw(game_win,"\n\n<MORE>");
		wrefresh(game_win);
		todd_getchar(NULL);
		werase(game_win);
		wrefresh(game_win);
		ac_update_fightscreen(); // draw all combatants again
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
	wprintw(game_win, _("List of parties:\n\n"));

        PGresult *res;
        res = PQexecPrepared(conn, "list_parties", 0, NULL, NULL, NULL, 0);
        if (PQresultStatus(res) == PGRES_TUPLES_OK)
        {
		int row_count = PQntuples(res);
		if (row_count > 0) // means there's parties
			{
			for (int i = 0; i < row_count; i++)
				{
					int partyid = atoi(PQgetvalue(res,i,0));
					wprintw(game_win,"#%d: %s\n", partyid, PQgetvalue(res,i,1));
					wprintw(game_win,_("Members: "));

					// TODO: simplify this
					// list_parties provides us the player ids, not names..
					// get player names with another query

					PGresult *names;
					char *party_id = itoa(partyid);
					const char *params[1] = {party_id};
					names = PQexecPrepared(conn, "party_get_names", 1, params, NULL, NULL, 0);

					if (PQresultStatus(names) == PGRES_TUPLES_OK) // there's party members, this should always be valid
			                {
						int members = PQntuples(names);
						if (members > 0)
							for (int i = 0; i < members; i++)
								{
								wprintw(game_win, "%s", PQgetvalue(names,i,0));
								if (i < (members - 1))
									wprintw(game_win, ", ");

								}

	                                }
			                PQclear(names);
	                        }
			}
		else
			wprintw(game_win, _("No parties..\n"));
	}
	wrefresh(game_win);
	PQclear(res);
}

// joins an existing party
void ac_party_join()
{
// First, see if you're already in a party
if (player_party.id != 0)
	{
	werase(game_win);
	ncurs_modal_msg(_("Betraying your party will be punished by death.."));
	return;
	}
// 0. print "parties that have room"
werase(game_win);
wprintw(game_win, _("List of parties that have room:\n"));

// 1. get a list of parties (partyid, name and all members)
// AND store it to a temporary struct
// a temporary struct that helps us in organising stuff
struct tempstuff
{
	int id;
	char *name;
	int correct_row; 	// corresponds to PQgetvalue(res,correct_row,0);
	int isfull;
	char *player1;
	char *player2;
	char *player3;
	
};
struct tempstuff temp_struct[99];

int got_space = 0; // used by listselect

        PGresult *res;
	int row_count;
        res = PQexecPrepared(conn, "list_parties", 0, NULL, NULL, NULL, 0);
        if (PQresultStatus(res) == PGRES_TUPLES_OK)
        {
                row_count = PQntuples(res);
                if (row_count > 0) // means there's parties
		{
			// 2. change the list to a listselect structure
			// store the info to a temporary struct
			for (int i = 0; i < row_count; i++)
			{
				// only show parties with room to join
				// this could be done in the database, I guess
				if (PQgetisnull(res,i,2) || PQgetisnull(res,i,3) || PQgetisnull(res,i,4)) 
				{ // at least 1 NULL space in the party

					temp_struct[got_space].isfull = 0;	// actually not utilized later in the code
					
					temp_struct[got_space].id = atoi(PQgetvalue(res,i,0));
					temp_struct[got_space].name = PQgetvalue(res,i,1);

					// players default to NULL
					temp_struct[got_space].player1 = NULL;
					temp_struct[got_space].player2 = NULL;
					temp_struct[got_space].player3 = NULL;

					if (!PQgetisnull(res,i,2))  // player1
						temp_struct[got_space].player1 = PQgetvalue(res,i,2);

					if (!PQgetisnull(res,i,3))
						temp_struct[got_space].player2 = PQgetvalue(res,i,3);
	
					if (!PQgetisnull(res,i,4))
						temp_struct[got_space].player3 = PQgetvalue(res,i,4);


					got_space++; // advance the list used by listselect
				}
			}
		}
	}

	// 3. call ncurs_listselect and make the player choose a party
	// note that this only shows parties with space to join..
        int selection = ncurs_listselect(&(temp_struct[0].name), sizeof(struct tempstuff), 0, got_space);

        if (selection >= 0)
        { // joins the selected party
		// get the proper party id - this should always be a party with vacant space
		// temp_struct[selection].playerX holds the player names. Go through them and add player.name to the first NULL spot

		char *player1 = temp_struct[selection].player1;
		char *player2 = temp_struct[selection].player2;
		char *player3 = temp_struct[selection].player3;

		// put the player to the first NULL place in parties database
		if (player1 == NULL)
			player1 = itoa(player.id);
		else
		if (player2 == NULL)
			player2 = itoa(player.id);
		else	// this has to be vacant, then
			player3 = itoa(player.id);
		
		int rc = update_party(temp_struct[selection].id,player1,player2,player3);
		if (rc) // succesful database operation
			{
		        // print a message to user
		        ncurs_log_sysmsg(_("%s is  now in party %s!\n"), player.name, temp_struct[selection].name);
		        ncurs_modal_msg(_("You are now part of %s!\n"), temp_struct[selection].name);
			player_party.id =   temp_struct[selection].id;
			// remember, player_party.name is a pointer, it must point to a valid location
			strncpy(g_partyname, temp_struct[selection].name, sizeof(temp_struct[selection].name));
			player_party.name = g_partyname;
			}
		else
			syslog(LOG_DEBUG,_("ERROR: update_party()"));

        }	// selection wasn't a proper party number
        else	
        {
                ncurs_modal_msg(_("Nobody wants you, anyway.."));
        }

	PQclear(res);
}

void ac_party_gather()
{

	ncurs_log_sysmsg("What would you like your party to be called?");
	char *line = NULL;
	size_t len = 0;
	wrefresh(input_win); // this is here to move the visible cursor to input_win instead of log_win
	if(!todd_getline(&line, &len, input_win)) return;

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

void ac_party_leave()
{
char *player1, *player2, *player3;
if (player_party.id == 0) 
	{
	ncurs_log_sysmsg(_("You're not in a party.."));
	return;
	}
	// 2. loop list_parties until you come across the proper id
        PGresult *res;
        res = PQexecPrepared(conn, "list_parties", 0, NULL, NULL, NULL, 0);
        if (PQresultStatus(res) == PGRES_TUPLES_OK)
        {
		int row_count = PQntuples(res);
		if (row_count > 0) // means there's parties
		{
			for (int i = 0; i < row_count; i++)
			{
				int row_partyid = atoi(PQgetvalue(res,i,0));
				if (row_partyid == player_party.id) // the database row matches player partyid, leave this party
				{
					// set the correct names for current party members

					// player names can be NULL in databse
					// and they need to be null when writing them back
					if (PQgetisnull(res,i,2))
						player1 = NULL;
	   				else
						player1 = PQgetvalue(res,i,2);
	
					if (PQgetisnull(res,i,3))
						player2 = NULL;
	   				else
						player2 = PQgetvalue(res,i,3);

					if (PQgetisnull(res,i,4))
						player3 = NULL;
	   				else
						player3 = PQgetvalue(res,i,4);

					// 3. get the party member ids, change yours to NULL, don't touch others
					// TODO: if all are null, destroy party

					// atoi(NULL) causes a segfault, avoid it! No "atoi(player3)"!
					// Note that this is a pitfall of writing null values instead 0 in the database...

					if (player1 != NULL)
						if (atoi(player1) == player.id)
							player1 = NULL;

					if (player2 != NULL)
						if (atoi(player2) == player.id)
							player2 = NULL;

					if (player3 != NULL)
						if (atoi(player3) == player.id)
							player3 = NULL;

					// 4. update_party
					int rc = update_party(player_party.id,player1,player2,player3);
					if (rc)
						{
						werase(game_win);
						ncurs_log_sysmsg(_("You have just left your party."));

						// check if everyone left the party, i.e. it's empty
						// if it is, trash the party
						if (player1 == NULL && player2 == NULL && player3 == NULL)
							{
							PGresult *destroyparty;
							char *partyid = itoa(player_party.id);
							const char *params[1] = {partyid};
							destroyparty = PQexecPrepared(conn, "party_destroy", 1, params, NULL, NULL, 0);
						        if (PQresultStatus(destroyparty) == PGRES_COMMAND_OK) 
								{
								ncurs_log_sysmsg(_("party %s is no more."), PQgetvalue(res,i,1));
								ncurs_modal_msg(_("You leave %s as the last member."), PQgetvalue(res,i,1));
								}	
							else	
								syslog(LOG_DEBUG,_("error when destroying a party!"));
							}
						else // coming here means the party isn't trashed, it's only you leaving
							ncurs_modal_msg(_("You have just left your party.\nYou will be missed by your party members, in good and in bad."));

		

						player_party.id = 0;
						}
					else
						syslog(LOG_DEBUG,_("ERROR: update_party()"));

					break; // exit loop since the right party was found already
				}
			}
		}
	}	
 
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

// print a syslog message, inform party members, etc
void ac_run_fight()
{
player.incombat = false;
ncurs_log_sysmsg(_("You ran from a fight, coward!"));
if (is_online(partymember1.id) || !is_online(partymember2.id)) // send a message when in multiplayer
	send_plr_ran(player);

// return to proper dungeon level
// ac_return_to_town actually takes you down a level, so increase by one first
player.dungeon_lvl++;
ac_return_to_town();
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

		case 3: 
		{
			player.dungeon_lvl = 2;
			set_player_location(&loc_dungeons_level2);
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


