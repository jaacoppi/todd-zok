#ifndef COMBAT_H
#define COMBAT_H

#include "element.h"
#include "skills.h"

int create_enemy();
void use_skill(int keypress);
int check_wuxing_time(Element dmg_element);
void combat_seeifready();
void combat_plr_ran(char *nick);
int combat_ismulti();
bool is_targetable(Skills *skill);
int target_attack(Skills *skill);
#endif //COMBAT_H
