#ifndef COMBAT_H
#define COMBAT_H

#include "element.h"

int create_enemy();
void use_skill(int keypress);
int check_wuxing_time(Element dmg_element);
void combat_seeifready();
void combat_plr_ran(char *nick);
int combat_ismulti();
#endif //COMBAT_H
