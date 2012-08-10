#ifndef ACTIONS_H
#define ACTIONS_H

#include "location.h"
#include "networking.h"

void ac_blank();
void ac_dungeons();
void ac_dungeons_action();
void ac_dungeons_glow();
void ac_dungeons_enter();
void ac_shrine_heal_1();
void ac_shrine_heal_all();
void ac_fightscreen();
void ac_update_fightscreen();
void ac_fight_0();
void ac_fight_1();
void ac_fight_2();
void ac_fight_3();
void ac_list_players();
void ac_view_stats();
void ac_tavern();
void ac_tavern_bartender();
void ac_tavern_room();
void ac_tavern_info();
void ac_warena();
void ac_warena_skills();
void ac_shop();
void ac_shop_buy();
void ac_shop_sell();
void ac_messageboard();
void ac_messageboard_view();
void ac_messageboard_write();
void ac_graveyard();
void ac_graveyard_view();
void ac_return_to_town();
void ac_run_fight();
void ac_quit();
void ac_party_gather();
void ac_party_list();
void ac_party_join();
void ac_party_leave();


void set_player_location(Location* loc);

int set_player_loc_yesno();
#endif //ACTIONS_H
