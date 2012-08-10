#ifndef PARTY_H
#define PARTY_H

void ac_party_gather();
void ac_party_list();
void ac_party_join();
int update_party(int party_id, char *player1, char *player2, char* player3);
bool unsub_party(unsigned int id);
bool sub_party(unsigned int id);
void set_party(unsigned int id);
int return_party_member_names(int partyid, char *member1, char *member2, char *member3);
bool is_partymember(char *name);
int party_askseed();

typedef struct Party Party;
struct Party {
       int id;
       char *name;
       Character *characters[3];
};

#endif //PARTY_H

