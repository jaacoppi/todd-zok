#include <stdio.h>
#include <stdbool.h>
#include <time.h> /* for random number generator */
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "actions.h"
#include "ui.h"
#include "events.h"
#include "locations.h"
#include "database.h"

extern void set_player_location(Location* loc);
extern int set_player_loc_yesno();

#define PROB(x) (i < (prob_max += x))

// PROB(1) means 1 out 1000. PROB(500) means 500/1000 and so on

int check_corpses()
{

// currently 
//	* always finds the corpse first in the database, boring

	// 1. SQL query for DEAD players
        PGresult *res;
        char *location = itoa(LOC_DEAD);
	char *level = itoa(player.dungeon_lvl); // only check for corpses on this dungeon lvl
	const char *params[2] = {location, level};

	// the database call has "ORDER BY random() LIMIT 1", so we get a random corpse
        res = PQexecPrepared(conn, "get_dead_players", 2, params, NULL, NULL, 0);
        if (PQresultStatus(res) == PGRES_TUPLES_OK)
        {
                int row_count = PQntuples(res);
	
		if (row_count > 0) // found a dead player
		{
		char *corpse_name = PQgetvalue(res,0,0);
		int corpse_money = atoi(PQgetvalue(res,0,1));
		int corpse_weapon = atoi(PQgetvalue(res,0,2));
		int corpse_id = atoi(PQgetvalue(res,0,3));

	        werase(command_win);
	        wrefresh(command_win);
		wclear(game_win);
 
		// TODO: there's a "stumble upon" in random events, use the same text format for added tension
		// can't use ncurs_modal because of the setplayer
		wprintw(game_win,_(
		"You stumble upon a corpse.\n"
		"It seems a fellow adventurer %s (TODO: how do you know the name) is no longer with us.\n\n"
		"Being the lowlife scum you are, you reach out for the purse of the corpse, and find...\n"
		),corpse_name);
		
		wprintw(game_win,_("Continue...\n\n"));
		wrefresh(game_win);
		todd_getchar(NULL);

		wprintw(game_win,_(
		"...%d gold.\n\n"
		"You feel a moment of sadness when looting a corpse, but it passes.\n"
		"\nTODO: what about the weapon? Inventory.."
		), corpse_money);
	
		ncurs_log_sysmsg("%s found a corpse of %s, money %d and weapon #%d",player.name,corpse_name,corpse_money,corpse_weapon);
		player.money += corpse_money;


		ncurs_modal_msg(_("\n\nFeeling a burden on your heart, you decide to take the corpse to the graveyard and bury it."));

		// update the database for the dead player
		// -> money = 0
		// -> location = LOC_DEAD_GRAVEYARD (cannot be looted again
		// TODO: do we need this, could we use PQgetvalue(res,corpse,X) or something?
		char *id = itoa(corpse_id);
		char *graveyard = itoa(LOC_DEAD_GRAVEYARD);
		const char *params[2] = {id,graveyard};
		PGresult *update;
		update = PQexecPrepared(conn, "loot_player", 2, params, NULL, NULL, 0);
		if (PQresultStatus(update) != PGRES_COMMAND_OK)
		{ 
			syslog(LOG_DEBUG,_("Player loot database update failed!\n"));
		}

		PQclear(update);


		// Burying takes you to the graveyard + decreases stamina
		player.stamina -= 5;
		if (player.stamina < 0)
			player.stamina = 0;
		
		set_player_location(&loc_graveyard);
		ac_graveyard_view();

		

		free(id);
		free(graveyard);
		return 1;
		}

        }
        PQclear(res);
	free (location);
	free (level);
 
// no corpses found, return 0
return 0;
}




int check_rnd_events()
{
	// TODO: write the rest of the events, adjust probabilities

	/* calculate probabilities to random events and then switch to the proper function */
	/* this part is only meant to make switching */

	int prob_max = 0;	/* reset PROB macro*/
	int i = rand() % 1000;

	/* events are based on dungeon levels. Some events can occur in many levels */
	switch(player.dungeon_lvl) {
		case 0:
			if (PROB(50)) // finding player corpses
				return check_corpses();

			if (PROB(10)) /* ==> chance is 10 out of 1000 */
				return ev_found_item();

			if (PROB(10)) /* ==> chance is 10 out of 1000 */
				return ev_old_man();
			break;

		case 1:
			if (PROB(50)) // finding player corpses
				return check_corpses();

			if (PROB(10)) /* ==> chance is 10 out of 1000 */
				return ev_bag_of_gold();
			break;

		case 2:
			if (PROB(50)) // finding player corpses
				return check_corpses();

			break;
		// and so on, until lvl 12?
		default: break;
	}
	/* if there's no random event, return 0. All random events should return 1; */
	return 0;
}


/* OLD MAN */
int ev_old_man()
{
	wclear(game_win);
	wprintw(game_win, _("An old man is wandering along the path, looking confused and worried.\n"));
	wprintw(game_win, _("-It seems I have managed to lose my way to the town.\nWould you be so kind to escort me there?\n"));
	wprintw(game_win, _("\nHelp the old man?\n"));
	wrefresh(game_win);
	int rc = set_player_loc_yesno();
	if (rc == 'y')
		ac_ev_oldman_help();
	if (rc == 'n')
		ac_ev_oldman_nohelp();

	return 1; /* no fights after this event */
}


void ac_ev_oldman_help()
{
	ncurs_modal_msg(_("Thank you kind sir! Here take this for your trouble.\n\nThe old man shoves a purse in your hands."));
	ncurs_log_sysmsg(_("You gain 100 gold!"));
	player.money += 100;
	player.stamina -= 5;
		if (player.stamina < 0)
			player.stamina = 0;
	set_player_location(&loc_town); // you end up in the town
}

void ac_ev_oldman_nohelp()
{
	ncurs_modal_msg(_("May the gods curse you."), "Old man"); // just for fun
	ncurs_log_chatmsg(_("May the gods curse you."), "Old man"); // just for fun
	player.stamina++; // this is to balance the stamina-- due to the action
	ac_dungeons();
}



/* A BAG OF GOLD */
int ev_bag_of_gold()
{
	wclear(game_win);
	ncurs_modal_msg(_("You find a bag of gold with 100 gold pieces in it!\n"));
	player.money = player.money +100;
	return 1;
}

/* ITEM FOUND */
int ev_found_item()
{
	int prob_max = 0;
	int i = rand() % 1000;
	bool item_found = false;
	/* random item found: */
	if (PROB(500)) {
		/* found a random item. TODO: check for ther stuff than weapons */
		// do not find "bare hands", last weapon in array
		i = rand() % (WEAPON_COUNT - 1);
		item_found = true;
	}

	/* The drawing routine is done like this to add excitement */
	werase(game_win);
	ncurs_modal_msg(_("Buried under a pile of rocks, you find..."));

	/* print out what you got */
	if (item_found) { /* found an item */
		ncurs_modal_msg(_("For some odd reason, you discard your %s and take the newly found %s"), player.weapon->name, weapons_list[i].name);
		/* TODO do these stuff, do not print todo stuff to player 
		wprintw(game_win, "\nEVENT TODO: randomize for more items once you have them\n");
		wprintw(game_win, "EVENT TODO: make a function that gives player a choice to discard the new weapon\n");
		*/
		player.weapon = &weapons_list[i];
	}
	else
	{
		ncurs_modal_msg(_("NOTHING"));
	}

	return 1;
}

