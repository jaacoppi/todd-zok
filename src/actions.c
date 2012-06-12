#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "player.h"
#include "locations.h"
#include "enemy.h"
#include "networking.h"
#include "globals.h"
#include "events.h"
#include "ui.h"

// globals are nasty?
Enemy enemy;

int check_rnd_events() {

	int i = rand() % 1000;
	/* calculate probabilities to random events and then switch to the proper function */
	/* this part is only meant to make switching */

#define PROB(x) (i < (prob_max += x))
    int prob_max = 0;
    // TODO: write the rest of the events, adjust probabilities
	if (PROB(10)) /* ==> chance is 10 out of 1000 */
		return ev_old_man();

	if (PROB(10)) /* ==> chance is 10 out of 1000 */
		return ev_old_man();

	if (PROB(10)) /* ==> chance is 10 out of 1000 */
		return ev_old_man();

	if (PROB(2)) /* ==> chance is 2 out of 1000 */
		return ev_old_man();
#undef PROB
	/* if there's no random event, return 0. All random events should return 1; */
	return 0;
}

void create_enemy()
{
	enemy.health = (rand() % 15) + 5;
	if (enemy.health < 15)
	{
		enemy.name = "Feeble goblin";
	}
	else
	{
		enemy.name = "Fierce goblin";
	}
}

void set_player_location(Location* loc)
{
	player.location = loc;
	ncurs_location();
}

void ac_dungeons()
{
	set_player_location(&loc_dungeons);
}

void ac_dungeons_action()
{
	if (player.action_points > 0)
	{
		player.action_points--;

		if (check_rnd_events() != 1) {/* return 1 => a random event occurred, don't fight */
			set_player_location(&loc_fight);
			create_enemy();
			werase(gamew);
			wprintw(gamew,"You encounter a %s!\n\n", enemy.name);
			mvwprintw(gamew,2,0,"Your hitpoints: %d/%d\n",player.health,player.max_health);
			mvwprintw(gamew,3,0,"Hitpoints for %s: %d\n",enemy.name,enemy.health);
			wrefresh(gamew);
		}
	}
	else
	{
			werase(gamew);
			wprintw(gamew,"You feel too tired to fight\n");
			wrefresh(gamew);
	}
}
void ac_fight_fight()
{
	/* player hits */
	int damage = 3; // chosen by fair dice roll

	mvwprintw(gamew,5,0,"You hit %s with %d points of damage\n", enemy.name, damage);

	enemy.health -= damage;

	/* update current hitpoints */
	mvwprintw(gamew,3,0,"Hitpoints for %s: %d\n",enemy.name,enemy.health);

	if (enemy.health <= 0)
	{
		mvwprintw(gamew,8,0,"%s is slain!\n", enemy.name);
		int money = 7;
		int exp = 10;
		wprintw(gamew,"You find %d coins on the corpse, and gain %d experience\n", money, exp);
		player.money += money;
		player.experience += exp;
		mvwprintw(gamew,12,0,"<MORE>");
		wrefresh(gamew);
		getch(),
		set_player_location(&loc_dungeons);
	}
	else
	{
		damage = 2; // goblins are bit weaker than you
		mvwprintw(gamew,6,0,"%s hits you with %d points of damage\n", enemy.name, damage);
		player.health -= damage;
		mvwprintw(gamew,2,0,"Your hitpoints: %d/%d\n",player.health,player.max_health);
		if (player.health <= 0)
		{
			mvwprintw(gamew,8,0,"The world fades around you as you fall to the ground,\nbleeding.");
			wattron(gamew,A_BOLD);
			wattron(gamew,A_UNDERLINE);
			mvwprintw(gamew,11,0,"You are dead.");
			wattroff(gamew,A_BOLD);
			wattroff(gamew,A_UNDERLINE);
			playing = false;
		}
	}
wrefresh(gamew);
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
			puts("The coin disappears into the slot with a clink. The crystal brightens for a moment and you step away from it feeling slightly better.");
		}
		else
		{
			puts("You search your pockets for coin, but find none.");
		}
	}
	else
	{
		puts("You try to insert a coin into the slot but to your surprise the crystal repels your coin.");
	}
}

void ac_shrine_heal_all()
{
	int money = player.money;
	money -= player.max_health - player.health;
	player.health = player.max_health;
	if (money < 0)
	{
		player.health += money;
		money = 0;
	}
	player.money = money;
	player.location = &loc_dungeons;
	puts("The crystal dazes you with blindingly bright light. As you regain control, you notice that all your wounds have mended.");
}

void ac_list_players()
{
	ncurs_msg("\nThe wind is howling in the empty streets of this god forsaken town. You are all alone here.");

}

void ac_view_stats()
{
	ncurs_stats(player);
}

void ac_tavern()
{
	set_player_location(&loc_tavern);
}

void ac_tavern_shout()
{
	puts("What do you want to yell?");
	int len = 80;
	char *line = malloc(len); // more dynamic memory allocation would be nice
	if (getnstr(line, len) != ERR)
	{
		send_chatmsg(line, strlen(line));
	}
	free(line);
}

void ac_shop()
{
	set_player_location(&loc_shop);
}

void ac_shop_buy()
{
	ncurs_msg("The poor man has nothing to sell to you.");
}

void ac_shop_sell()
{
	ncurs_msg("The poor man has no coins to buy anything from you.");

}

void ac_return_to_town()
{
	set_player_location(&loc_town);
}

void ac_quit()
{
	ncurs_msg("Bye.");
	playing = false;
}

