/* NOTE: maximum weapon name length is currently 14 characters:
max_weapon_nam
-------------
axe of assault
*/
#include "weapons.h"
#include "skills.h"

Weapons weapons_enemy[3] = {
	{
		0,
		"Claw",
		ELEM_EARTH,
		5,
		0
	},

	{
		1,
		"Fire breath",
		ELEM_FIRE,
		8,
		0
	},

	{
		3,
		"Bite of death",
		ELEM_EARTH,
		10,
		0
	}
};

Weapons weapons_list[WEAPON_COUNT] = {
	{
		0,
		"Stick of doom",
		ELEM_WOOD,
		5,
		50
	},

	{
		1,
		"Cane of pain",
		ELEM_WOOD,
		10,
		100
	},
	{
		2,
		"Barbarian staff",
		ELEM_WOOD,
		30,
		500
	},
	{
		3,
		"Devil's dagger",
		ELEM_METAL,
		4,
		40
	},
	{
		4,
		"Able's sabre",
		ELEM_METAL,
		8,
		80
	},
	{
		5,
		"Death sword",
		ELEM_METAL,
		25,
		400
	},
	{
		999,
		"Bare hands",
		ELEM_EARTH,
		1,
		0
	}
};

