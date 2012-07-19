#ifndef CHARACTER_H
#define CHARACTER_H

#include "element.h"
#include "location.h"
#include "weapons.h"
#include "skills.h"

#define ENEMY_COUNT 6
typedef struct Character Character;
struct Character {
	int id;
	char *name;
	Location *location;
	int stamina;
	int experience;
	int money;
	// TODO add more stats 
	int health;
	int max_health;
	int elements[ELEM_COUNT]; // use enum Element to access
	Weapons *weapon;
	Skills *skill[4];
	int dungeon_lvl; // dungeon_lvl 0 == town, 1 = forest level etc
};

#endif //CHARACTER_H
