#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdbool.h>

#include "combat.h"
#include "actions.h"
#include "element.h"
#include "character.h"
#include "skills.h"
#include "globals.h"
#include "ui.h"
#include "location.h"
#include "locations.h"
#include "database.h"
#include "element.h"

extern void set_player_location(Location* loc);

int fight_check_dead()
{
	/* TODO: figure out the order of checking deaths: attacks are simultaneous. Iniative? */
	// TODO enemy and player codes are almost identical, refactor into a function
	// bool check_chr_dead(Character *chr); or something

	bool enemy_dead;
	bool enemy_dead_elements;
	// loop through all enemies in combat

	for (int i = 0; i <= 2; i++)
		{
		enemy_dead = false;
		enemy_dead_elements = false;

			if (enemy_party.characters[i]->incombat)
				{
					/* check if enemy dies */
					for (size_t j = 0; j < ELEM_COUNT; j++)
						{
							if (enemy_party.characters[i]->elements[j] <= 0) 
							{
								enemy_dead = true;
								enemy_dead_elements = true;
							}
						}
					if (enemy_party.characters[i]->health <= 0)
						enemy_dead = true;
			
				// the current enemy is dead (characters[i])
				if (enemy_dead)	
					{
						enemy_party.characters[i]->incombat = false;
						// TODO: would it look better to display "DEAD" instead of erasing the box?
						werase(fight_stat_win[3+i]);
						wrefresh(fight_stat_win[3+i]);

						int money = 7;
						player.money += money;
				
						if (enemy_dead_elements)
							ncurs_log_sysmsg(_("%s has caused an elemental imbalance in %s"), player.name, enemy_party.characters[i]->name);
					
						ncurs_log_sysmsg(_("%s has killed %s!"), player.name, enemy_party.characters[i]->name);
						// TODO: share money from a kill someway
						ncurs_log_sysmsg(_("%s found %d coins from the body"), player.name, money);

					// if there's no more enemies left, return to dungeons. 
					int alldead = 1;
 					for (int i = 0; i <= 2; i++)
						if (enemy_party.characters[i]->incombat)
 							alldead = 0;

					// if all enemies are dead, the battle is over
					if (alldead)
						{
						player.incombat = false; // don't return to combat any more
						ncurs_log_sysmsg(_("All enemies are slain! The battle is over"));
						return 1;					
						}
					// If there's enemies left, the battle continues (i.e. do nothing)
					}
				}
			}

	/* check if player dies as well */
	// TODO: loop through all players, not just yourself
	bool player_dead = false;
	bool player_dead_elements = false;
	for (size_t i = 0; i < 5; i++)
	{
		if (player.elements[i] <= 0) {
			player_dead = true;
			player_dead_elements = true;
		}
	}
	if (player.health <= 0)
		player_dead = true;

	if (player_dead)
	{
		wclear (game_win);
		if (player_dead_elements) // elements below 0, don't die but faint only
		{
			db_player_location(LOC_FAINTED);
			// TODO: which enemy..
			ncurs_log_sysmsg(_("%s has caused an elemental imbalance in %s"), enemy_party.characters[0]->name, player.name);
			mvwprintw(game_win, 6, 0, _("The world around you starts to spin.\nYou sense a great imbalance inside you."));

			wattron(game_win, A_BOLD);
			wattron(game_win, A_UNDERLINE);
			mvwprintw(game_win, 8, 0, _("You faint. TODO: \"come back in 8 hours??\""));
			wattroff(game_win, A_BOLD);
			wattroff(game_win, A_UNDERLINE);
		}
		else // PERMADEATH
		{
			/* first, set the player location to "DEAD" */
			db_player_location(LOC_DEAD);
			// TODO: which enemy
			ncurs_log_sysmsg(_("%s has killed %s!"), enemy_party.characters[0]->name, player.name);
			mvwprintw(game_win, 6, 0, _("The world fades around you as you fall to the ground, \nbleeding."));
			wattron(game_win, A_BOLD);
			wattron(game_win, A_UNDERLINE);
			mvwprintw(game_win, 8, 0, _("You are dead."));
			wattroff(game_win, A_BOLD);
			wattroff(game_win, A_UNDERLINE);
		}

		// common stuff to death // elemental imbalance
		wrefresh(game_win);
		todd_getchar(NULL);
		playing = false;
	}

	if (enemy_dead || player_dead)
		return 1; /* if enemy / player is dead, don't redraw combat stuff anymore */
	else
		return 0; // redraw combat stuff
}

int create_enemy(Character *enemy)
{
	// TODO: get proper dungeon level
	// ways to do this: 
		// * only party leader can start a fight (follow the leader -mode)
		// * ask players if they want to join the fight
	int dungeon_lvl = 1;

	/* randomly choose an enemy from enemylist, based on player dungeon level */
	int random_enemy = rand() % ENEMY_COUNT;
	// NOTE THAT dungeon level 0 = town, 1 = first level and so on. Therefore, dungeon_lvl - 1;
	memcpy(enemy, &enemylist[dungeon_lvl - 1][random_enemy], sizeof(Character));

	enemy->incombat = 1;
	return 1;
}


void align_elements(Character *dest, Element type)
{
	dest->elements[(type+1) % ELEM_COUNT]++;
	dest->elements[(type+2) % ELEM_COUNT]--;
}

int dmg_calc_blocking(Character *dest, Element dmg_type)
{
	return dest->elements[(dmg_type+3) % ELEM_COUNT];
}

void skill_effect(Character *source, Character *dest, Skills *skill)
{
	ncurs_log_sysmsg("%s used %s on %s",source->name, skill->name, dest->name);
	int dmg = 0;
	/* calculate normal damage done based on skill + weapon */
	dmg += skill->damage;
	dmg += source->weapon->damage;

	/* if the skill type matches the element in the wu xing day cycle, apply bonus damage */
	if (check_wuxing_time(skill->dmg_type))
		{
		ncurs_log_sysmsg(_("%s is in sync with the element cycle and deals bonus damage!"),source->name);
		dmg += skill->damage / 2; // TODO: balance the effect
		}

	/* calculate blocking by enemy*/
	/* blocking elements are based on skill used to attack. TODO: should this be weapon instead? */
	dmg -= dmg_calc_blocking(dest, skill->dmg_type);

	/* calculate damage */
	if (dmg > 0)
	{
		dest->health -= dmg;
		ncurs_log_sysmsg(_("%s did %d damage to %s"), source->name, dmg, dest->name);

		/* ONLY ALIGN ELEMENTS IF DAMAGE WAS DONE */
		/* elements align == player skill type and weapon type are the same */
		/* also, if elements align, changes to enemy elemental balance occur */
		if (skill->dmg_type == source->weapon->dmg_type) {
			ncurs_log_sysmsg(_("%s causes elemental damage to %s"), source->name, dest->name);
			dmg += source->elements[skill->dmg_type];
			align_elements(dest, skill->dmg_type); // TODO is this correct?
		}
	}
	else
	{ /* don't do negative damage */
		ncurs_log_sysmsg(_("%s blocked attack from %s."), dest->name, source->name);
	}
	source->stamina -= skill->ap_cost; /* spend action points on the attack */
}

void use_skill(int keypress)
{
	player.turnready = -1; // when set to [0..3] it means a skill is used this turn
	/* this function is entered with a keypress from ac_fight0 and so on*/
	/* the keypress is used to determine what attack is used */

	/* 1. the player already chose the attack (int keypress), now it's time for the enemy */
	// TODO enemy always uses skill 0

	/* 2. calculate damage */
	/* player does damage */
	/* Wu Xing cycles:
	   Wood causes +FIRE, -EARTH
	   Fire causes +EARTH, -METAL
	   Earth causes +METAL, -WATER
	   Metal causes +WATER, -WOOD
	   Water causes +WOOD, -FIRE
	   */

	// TODO: !succesful_target, return loop here?
	if (keypress <= 4 && strcmp(player.skill[keypress]->name,"Unused") != 0)
	{
		int enemynr = 0;
		for (int i = 0; i <=2; i++)
	        if (enemy_party.characters[i]->incombat)
	                enemynr++;

		// only target if there's more than 1 enemy - enemynr tells us how many we have
		int succesful_target = -1;
		player.combattarget = 0; // reset the target int, by default attack 0
		if (enemynr > 1)
			{
			// Target the attack if it can be targeted
			if (is_targetable(player.skill[keypress]))
				succesful_target = target_attack(player.skill[keypress]);

			if (succesful_target == -1)
				return; // go back to select a different skill? TODO: How?			

			//// end targeting
			// succesful_target now holds the enemy id - store it to your char array
			player.combattarget = succesful_target;
			}
		else // only one enemy, select that one (might be index 0,1 or 2
			{
			for (int i = 0; i <=2; i++)
			        if (enemy_party.characters[i]->incombat)
					{
					player.combattarget = i;
					break;
					}
			}


		// Players attack
		// wait for everyone in the game to commit the skill
	
		// go through all online players. Once a player commits a turn,
		// a TURNREADY is sent
		player.turnready = keypress;
		// if a multiplayer game, go through send_turnready, otherwise directly to combat
		// TODO: will hang if a party member joins game during combat - check for incombat instead of is_online
	//	if (is_online(partymember1.id) || is_online(partymember2.id))
		if (combat_ismulti())
			send_turnready();
		else
			combat_seeifready();
	}
	else
	{
		ncurs_log_sysmsg(_("You dont have a skill in slot %d"), keypress+1);
	}
}


// check if the wuxing time cycle amplifies the used skill
// returns 1 if it does, 0 if not
int check_wuxing_time(Element dmg_element)
{
	// done in the database to keep the multiplayer in sync, localtime() wouldn't suit this
	PGresult *res;
	res = PQexecPrepared(conn, "get_hour", 0, NULL, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_TUPLES_OK)
			syslog(LOG_DEBUG,_("get_hour failed"));

	int hour = atoi(PQgetvalue(res,0,0)); /* return now() - last_logout */
	PQclear(res);


// The real Wu xing cycle has two fire sequences, is this bad for the game balance?
// first time (in medicine theory) is supposed the be empirical organs only,
// second time is supposed to be non-empirical organs only. Can this info be used to balance fire?

// Also, metal would almost never get any bonuses, since few people would play night time
/*
http://www.chuntianacademy.com/wu-xing
03-07 metal
07-11 earth
11-15 fire (first time)
15-19 water
19-23 fire (second time, "non-empirical")
23-03 wood
*/



	switch (hour)
		{
		case 3:
		case 4:
		case 5:
		case 6:
			{
			if (dmg_element == ELEM_METAL)
				return 1;

			break;
			}
		case 7:
		case 8:
		case 9:
		case 10:
			{
			if (dmg_element == ELEM_EARTH)
				return 1;

			break;
			}
		case 11:
		case 12:
		case 13:
		case 14:
			{
			if (dmg_element == ELEM_FIRE)
				return 1;

			break;
			}
		case 15:
		case 16:
		case 17:
		case 18:
			{
			if (dmg_element == ELEM_WATER)
				return 1;

			break;
			}
		case 19:
		case 20:
		case 21:
		case 22:
			{
			if (dmg_element == ELEM_FIRE)
				return 1;

			break;
			}
		case 23:
		case 0:
		case 1:
		case 2:
			{
			if (dmg_element == ELEM_WOOD)
				return 1;

			break;
			}

		default:
			break;
		}


// no bonus
return 0;
}

void combat_seeifready()
{
// loop through all the players in combat
int allready = 1;


//if (is_online(partymember1.id) || is_online(partymember2.id))	// it's multiplayer combat
if (combat_ismulti())	// it's multiplayer combat
	{
	// loop through all the effects of all party members taking part in the fight
	for (int i = 0; i <= 2; i++)
		if (player_party.characters[i]->incombat)
			{
			if (player_party.characters[i]->turnready < 0)
				{
				ncurs_log_sysmsg("still waiting for: %s",player_party.characters[i]->name);
				allready = 0;	// still waiting for a keypress
				}
			}
	}
else	// it's single player combat
	{
	allready = 1;
	}

// COMBAT EFFECTS START HERE
if (allready) // if everyone is ready, do combat stuff
	{
	ncurs_log_sysmsg(_("all players have committed turns, calculating effects"));
	ncurs_log_sysmsg(_("Combat resolution============================"));
	// combat stuff begins here

	// players attack enemies
	for (int i = 0; i <= 2; i++)
		if (player_party.characters[i]->incombat)
			skill_effect(player_party.characters[i], enemy_party.characters[player_party.characters[i]->combattarget],player_party.characters[i]->skill[player_party.characters[i]->turnready]);

	// enemies attack players
	// choose which player to attack
	int dest = 0;
	int players = 0;
	for (int i = 0; i <= 2; i++)
		if (player_party.characters[i]->incombat)
			players++;
	// players now holds the number of players in combat, randomize one of them
	int i = 0;

	// enemies attack here
	for (int j = 0; j <= 2; j++)
	{
		if (enemy_party.characters[j]->incombat)	// only enemies who are in combat attack
		{
			while (i == 0)	// loop until you find an acceptable target, then attack
			{		
				dest = rand() % players;
				// dest holds a number from 0..players in combat
				if (player_party.characters[dest]->incombat) // this player is in combat -> acceptable target
					i = 1;
			}

			// TODO: random enemy skill (enemies should have more than one skill..
			skill_effect(enemy_party.characters[j], player_party.characters[dest], enemy_party.characters[j]->skill[0]);
		}
	}


	ncurs_log_sysmsg(_("============================================="));
	// combat stuff ends here

	// reset turnready
	for (int i = 0; i <= 2; i++)
		if (player_party.characters[i]->incombat)
			player_party.characters[i]->turnready = -1;	

	/* 3. check for dead player/enemy */
	int all_enemies_dead = fight_check_dead();
	/* 4. update stats and display them IF THE ENEMY DIDN'T DIE */
	if (!all_enemies_dead)
		ac_update_fightscreen();
	else
		{
		ncurs_clear_fightwindows();
		ncurs_modal_msg(_("All enemies are slain! The battle is over"));
		// ac_dungeons makes the player go up a dungeon level
		// -> to stay at the same level, decrease the level here
		player.dungeon_lvl--;
		ac_dungeons();
		}
	}
}

void combat_plr_ran(char *nick)
{
// find out which screen to delete from fight window - naturally only affects multiplayer

// do nothing if the running player is you
if (strcmp(nick, player.name) == 0)
	return;

ncurs_log_sysmsg(_("%s ran from a fight. What a coward!"),nick);
int index;
for (index = 0; index <=2; index++)
        if (strcmp(player_party.characters[index]->name, nick) == 0)
                break;

wclear(fight_stat_win[index]);
wrefresh(fight_stat_win[index]);

// set the incombat to false
for (int i = 0; i <= 2; i++)
	if (player_party.characters[i]->name != NULL)
		if (strcmp(player_party.characters[i]->name,nick) == 0)
			player_party.characters[i]->incombat = 0;

}

// returns 1 if combat is multiplayer (party) or 0 when single
int combat_ismulti() {
if (is_online(partymember1.id) || is_online(partymember2.id))
	return 1;

return 0;
}

// returns true if the skill can be targeted
// TODO: implement
bool is_targetable(Skills *skill)
{
return true;
}

int target_attack(Skills *skill)
{
typedef struct enemies enemies;
struct enemies
{
	char *name;
};
enemies enemylist[3];

// create a listselect structure of enemies in combat
int enemynr = 0;
for (int i = 0; i <=2; i++)
	if (enemy_party.characters[i]->incombat)
		{
		enemylist[enemynr].name = enemy_party.characters[i]->name;
		enemynr++;
		}
// enemynr now contains the number of enemies -> number of possible selections
// enemylist now contains all the enemy names (no blanks)


// display it
	wclear(game_win);
	wprintw(game_win, _("Who do you want to target?\n"));
	int selection = ncurs_listselect(&(enemylist[0].name), sizeof(struct enemies), 0, enemynr);
	wclear(game_win);
	ac_update_fightscreen();
	
// TODO: what to do if an enemy dies -> listselect doesn't correspond to actual enemy_party.characters[i] anymore
// the int selection actually means "the Xth player that is incombat

return selection;

}
