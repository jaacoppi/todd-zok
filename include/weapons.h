#ifndef WEAPONS_H
#define WEAPONS_H

#include "element.h"

#define WEAPON_COUNT 7 // this count should alse include "bare hands"

typedef struct Weapons Weapons;
struct Weapons {
	int index;	/* is index used anywhere? Can it be removed? */
	char *name;
	Element dmg_type;
	int damage;
	int price;	/* is the shop the only place to buy / sell weapons ?? */
};

#endif //PLAYER_H
 
