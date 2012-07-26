#ifndef COMBAT_H
#define COMBAT_H

#include "element.h"

int create_enemy(int level, int random_enemy);
void use_skill(int keypress);
int check_wuxing_time(Element dmg_element);

#endif //COMBAT_H
