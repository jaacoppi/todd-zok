#include <zmq.h>
#include <syslog.h>
#include <string.h>

#include "globals.h"
#include "ui.h"

// update party members as given in parameters. Return 1 if succesful, calling function handles printing
// called when joining a party and when leaving a party
int update_party(int party_id, char *player1, char *player2, char* player3)
{
	// player1, player2 and player3 now hold the party member names
	// update the stuff in database
	char *partyid = itoa(party_id);
	const char *params[4] = {partyid, player1, player2, player3};

	PGresult *update_party;
	update_party = PQexecPrepared(conn, "update_party", 4, params, NULL, NULL, 0);
	if (PQresultStatus(update_party) == PGRES_COMMAND_OK)
		return 1;	// succesful, return 1;

PQclear(update_party);
return 0;
}

bool unsub_party(unsigned int id)
{
	size_t len = 0;
	char buf[12]; // 'pXXXXXXXXXX\0' = 12 chars
	len = snprintf(buf, 12, "p%010d", id);
	if (zmq_setsockopt(party_socket, ZMQ_UNSUBSCRIBE, buf, len))
	{
		syslog(LOG_WARNING, "Party unsubscription failed: %s\r\n", zmq_strerror(errno));
		return false;
	}
	return true;
}

bool sub_party(unsigned int id)
{
	size_t len = 0;
	char buf[12]; // 'pXXXXXXXXXX\0' = 12 chars
	len = snprintf(buf, 12, "p%010d", id);
	if (zmq_setsockopt(party_socket, ZMQ_SUBSCRIBE, buf, len))
	{
		syslog(LOG_WARNING, "Party subscription failed: %s\r\n", zmq_strerror(errno));
		return false;
	}
	return true;
}


/*
 * Unsubscribe from current party in party socket and subscribe to new party id
 */
void set_party(unsigned int id)
{
	if (!unsub_party(player_party.id))
	{
		return;
	}
	if (!sub_party(id))
	{
		return;
	}
	player_party.id = id;
}


// returns true if parameter name is a partymember
// returns false if not
bool is_partymember(char *name)
{
// if the player doesn't have partymembers, the name values are NULL
// -> don't strcmp NULL values
if (partymember1.name != NULL)
	if (strcmp(name,partymember1.name) == 0)
		return true;

if (partymember2.name != NULL)
	if (strcmp(name, partymember2.name) == 0)
		return true;

return false;
}
