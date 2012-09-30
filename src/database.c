#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdbool.h>
#include <time.h> /* for random number generator */
#include <unistd.h>

#include <zmq.h>
#include <libpq-fe.h>

#include "globals.h"
#include "action.h"
#include "location.h"
#include "locations.h"
#include "networking.h"
#include "ui.h"
#include "weapons.h"
#include "database.h"
#include "character.h"

int init_pq()
{
	PGresult *res = NULL;
    conn = PQconnectdb("dbname=todd user=todd");
    if (PQstatus(conn) != CONNECTION_OK)
	{
		goto pq_cleanup;
	}

	// stuff to do when setting up the database:
	// all functions called initdb_something
	// NOTE: these need to be first in the file or the game functions won't find the tables

	// dropall drops everything
	// createschema creates the schema that was dropped by dropall
	// schemaowner sets schema permissions (just in case)
	// player_logins holds the login info
	// player_stats holds the stats for players
	// messageboard for .. messages
	// parties for .. parties
	

	// drop all tables by destroying the public schema
	res = PQprepare(conn, "initdb_dropall", "drop schema if exists public cascade;" , 0, NULL);	
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	// recreate schema dropped by dropall
	res = PQprepare(conn, "initdb_createschema", "create schema public;" , 0, NULL);	
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	res = PQprepare(conn, "initdb_schemaowner", "ALTER schema public OWNER TO todd;" , 0, NULL);	
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	// recreate pgcrypto
	res = PQprepare(conn, "initdb_extension_pgcrypto", "create extension pgcrypto;" , 0, NULL);	
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	// player_logins
	res = PQprepare(conn, "initdb_player_logins", "create table player_logins (id serial primary key , name varchar not null unique, passwd text not null,last_logout timestamp);", 0, NULL);	
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);


	// player_stats
	res = PQprepare(conn, "initdb_player_stats", "create table player_stats (id integer references player_logins(id), location integer default 0, stamina integer default 100, money integer default 40, health integer default 10, max_health integer default 10, wood integer default 3, fire integer default 3, earth integer default 3, metal integer default 3, water integer default 3, weapon integer default 999, skill_0 integer default 999, skill_1 integer default 999, skill_2 integer default 999, skill_3 integer default 999, dungeon_level integer default 0);", 0, NULL);	
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	// messageboard
	res = PQprepare(conn, "initdb_messageboard", "create table messageboard (id serial primary key, timestamp timestamp default now() not null, player_id integer references player_logins(id) on update cascade not null, body text not null);", 0, NULL);	
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	// parties
	res = PQprepare(conn, "initdb_parties", "create table parties (partyid serial primary key, name varchar not null unique, player1 integer references player_logins(id), player2 integer references player_logins(id), player3 integer references player_logins(id));", 0, NULL);	
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);


	//// stuff with the game



	res = PQprepare(conn, "get_login_id", "select id from player_logins where name = $1;", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	res = PQprepare(conn, "is_online_by_id", "select location from player_stats where id = ($1);", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	PQclear(res);
	res = PQprepare(conn, "check_passwd", "select true from player_logins where id = $1 and passwd = crypt(cast($2 as text), passwd);", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear(res);
	res = PQprepare(conn, "new_login", "insert into player_logins (name, passwd) values ($1, crypt(cast($2 as text), gen_salt('bf'))) returning player_logins.id;", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	PQclear(res);
	res = PQprepare(conn, "save_player", "update player_stats set (stamina, money, health, max_health, wood, fire, earth, metal, water, weapon, skill_0, skill_1, skill_2, skill_3, dungeon_level) = ($2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16) where id = $1;", 16, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	res = PQprepare(conn, "player_onlinestatus", "update player_stats set (location) = ($2) where id = $1;", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}


	res = PQprepare(conn, "save_player_logout_time", "update player_logins set (last_logout) = (now()) where id = $1;", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	
	/* select id, last_logout time and stamina */
	res = PQprepare(conn, "load_player_logout_time_stamina", "select player_logins.id, player_logins.last_logout, player_stats.location, player_stats.stamina from player_logins, player_stats WHERE player_logins.id = player_stats.id;", 0, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	PQclear(res);
	res = PQprepare(conn, "update_stamina", "update player_stats set (stamina) = ($2) where id = $1;", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	PQclear(res);
	res = PQprepare(conn, "calc_hours", "select now() - last_logout FROM player_logins WHERE id = $1;", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	PQclear(res);
	res = PQprepare(conn, "new_player_stats", "insert into player_stats (id) values ($1);", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear(res);
	res = PQprepare(conn, "load_player", "select player_logins.name, stamina, money, health, max_health, wood, fire, earth, metal, water, weapon, skill_0, skill_1, skill_2, skill_3, dungeon_level from player_stats, player_logins where player_logins.id = ($1)  AND player_logins.id = player_stats.id;", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear(res);
	res = PQprepare(conn, "view_messageboard", "select name, timestamp, body from messageboard, player_logins where player_logins.id = messageboard.player_id order by messageboard.id desc limit 10;", 0, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear(res);
	res = PQprepare(conn, "write_to_messageboard", "insert into messageboard (player_id, body) values ($1, $2);", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear(res);

	// player_stats.id = player_logins.id is a hack to remove duplicates, there might be a better way
	res = PQprepare(conn, "list_online_players", "select player_logins.name,player_stats.location from player_logins,player_stats WHERE player_stats.id = player_logins.id;", 0, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear(res);

	res = PQprepare(conn, "get_dead_players", "select player_logins.name, player_stats.money,player_stats.weapon,player_stats.id from player_logins, player_stats where player_stats.location = $1 and player_stats.dungeon_level = $2 and player_logins.id = player_stats.id ORDER BY random() LIMIT 1;", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear(res);

	// update the database for dead player stats after looting
	// TODO: currently only loots money
	res = PQprepare(conn, "loot_player", "update player_stats set (money)=(0), (location)=($2) where id = $1;", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	PQclear(res);

	// Graveyard, AKA high scores
	res = PQprepare(conn, "view_graveyard", "select player_logins.name, player_stats.location FROM player_logins, player_stats WHERE player_stats.id = player_logins.id AND location = $1;", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	PQclear(res);

	// get current hour (used for wuxing cycle)
	res = PQprepare(conn, "get_hour", "select extract(hour from now());", 0, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}

	PQclear(res);

	// create a new party
	res = PQprepare(conn, "new_party", "insert into parties (name, player1, player2, player3) values ($1, $2, NULL, NULL) returning parties.partyid;", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	// list parties - returns player ids, use party_get_names to find out corresponding names
	// TODO: make this a single SQL statement, if you can
	res = PQprepare(conn, "list_parties", "select parties.partyid, parties.name, parties.player1, parties.player2, parties.player3 FROM parties;", 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	res = PQprepare(conn, "party_get_names", "select player_logins.name from player_logins, parties where player_logins.id = any(array[parties.player1, player2, player3]) and partyid = ($1);" , 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	// update party member names
	res = PQprepare(conn, "update_party", "update parties set (player1, player2, player3) = ($2, $3, $4) WHERE partyid = ($1) ;", 4, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	// load player party data at startup
	res = PQprepare(conn, "load_player_party", "select parties.partyid, parties.name, parties.player1, parties.player2, parties.player3 FROM parties WHERE parties.player1 = ($1) OR parties.player2 = ($1) OR parties.player3 = ($1);", 1, NULL);	
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);


	res = PQprepare(conn, "party_destroy", "DELETE FROM parties WHERE partyid = ($1);" , 1, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear (res);

	return true;

pq_cleanup:
	syslog(LOG_ERR, "Postgres init error: %s", PQerrorMessage(conn));
	if (res != NULL)
		PQclear(res);
	return false;
}

void cleanup_pq()
{
    PQfinish(conn);
}

char *itoa(int i)
{
        char *str = malloc(20); // should be enough?
        snprintf(str, 20, "%d", i);
        return str;
}

/* sets the player location in the database. Knowledge is used by dailybot, at the moment ToDD has no use for this info */
void db_player_location(int location) 
{
	PGresult *res;
	char *player_id = itoa(player.id);
	char *player_location = itoa(location);

	const char *locparams[2] = {player_id, player_location };
	res = PQexecPrepared(conn, "player_onlinestatus", 2, locparams, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
		{
		syslog(LOG_WARNING, "Player online status save failed: %s", PQresultErrorMessage(res));
		}

}
