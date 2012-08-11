/* globals.c - shared stuff with ToDD and dailybot */
#include "globals.h"
Character player;
Character partymember1;
Character partymember2;
Character enemy1;
Character enemy2;
Character enemy3;

int randomseed;

Party player_party = {
	.id = 0,	// NOTE that currently 0 means no party
	.name = "",	// "" or NULL or what? must be printable in view stats
	.characters = {
		&player,
		&partymember1,
		&partymember2
	}
};

Party enemy_party = {	// enemy party only needs enemy characters
	.characters = {
		&enemy1,
		&enemy2,
		&enemy3
	}
};
