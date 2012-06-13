#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdbool.h>
#include <time.h> /* for random number generator */
#include <unistd.h>

#include <zmq.h>
#include <libpq-fe.h>

#include "action.h"
#include "location.h"
#include "player.h"
#include "locations.h"
#include "networking.h"
#include "ui.h"

#define NAME_QUERY "What's your name, adventurer?  "
#define WELCOME_NEW "The bards have not heard of you before.\nWelcome to Tales of Deep Dungeons, "
#define WELCOME_OLD "Welcome back to Tales of Deep Dungeons, "
#define RETRY_LIMIT 3


// ugly globals go here
int playing;
void *push_socket = NULL;
void *chat_socket = NULL;
void *zmq_context = NULL;
Player player;
PGconn *conn;

char *itoa(int i)
{
	char *str = malloc(20); // should be enough?
	snprintf(str, 20, "%d", i);
	return str;
}

/*
	Asks the player for name
*/
bool get_name()
{
	char *name = NULL;
	size_t name_len = 0;
	fputs(NAME_QUERY, stdout);
	ssize_t line_len = getline(&name, &name_len, stdin);
	if (line_len < 0)
	{
		syslog(LOG_ERR, "Read error: %s (%s:%d)", strerror(errno), __FILE__, __LINE__);
		return false;
	}
	name_len = line_len;
	name[name_len - 1] = 0; // strip trailing newline
	if (strnlen(name, name_len) < name_len - 1)
	{
		// null bytes in name
		syslog(LOG_ERR, "Player name contains \\0");
		return false;
	}

	player.name = name;
	return true;
}

bool get_id()
{
	bool ret = false;
	const char *params[1] = {player.name};
	PGresult *res;
	res = PQexecPrepared(conn, "get_login_id", 1, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (PQntuples(res) == 1)
		{
			// TODO get integers directly without atoi
			player.id = atoi(PQgetvalue(res, 0, 0));  
			ret = true;
		}
	}
	PQclear(res);
	return ret;
}

bool check_passwd()
{
	bool ret = false;
	char *passwd = NULL;
	size_t passwd_len = 0;
	printf("Welcome number %d, enter password: ", player.id);
	ssize_t line_len = getline(&passwd, &passwd_len, stdin);
	passwd[line_len-1] = '\0'; // strip newline
	char *player_id = itoa(player.id);
	const char *params[2] = {player_id, passwd};
	PGresult *res;
	res = PQexecPrepared(conn, "check_passwd", 2, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_TUPLES_OK)
	{
		if (PQntuples(res) != 0)
		{
			ret = true;
		}
	}
	PQclear(res);
	free(passwd);
	free(player_id);
	return ret;
}

void load_player_data()
{
	// TODO fetch data from DB
	player.location = &loc_town;
	player.action_points = 10;
	player.experience = 0;
	player.money = 10;
	player.wood = 5;
	player.fire = 5;
	player.earth = 5;
	player.metal = 5;
	player.water = 5;
	player.weapon_index = 0;
}

bool create_player()
{
	bool ret = false;
	char *passwd = NULL;
	size_t passwd_len = 0;
	printf("Welcome new player, enter password: ");
	ssize_t line_len = getline(&passwd, &passwd_len, stdin);
	passwd[line_len-1] = '\0'; // strip newline
	// TODO ask password for a second time to avoid typos
	const char *params[2] = {player.name, passwd};
	PGresult *res;
	res = PQexecPrepared(conn, "new_login", 2, params, NULL, NULL, 0);
	if (PQresultStatus(res) == PGRES_COMMAND_OK)
	{
		ret = true;
	}
	PQclear(res);
	free(passwd);
	return ret;
}

/*
	Loads player from disk or if player is not found, creates a new player.
*/
bool get_player()
{
	if (!get_name())
	{
		return false;
	}
	if (get_id())
	{
		for (int retries = 0; retries < RETRY_LIMIT; retries++)
		{
			if (check_passwd())
			{
				return true;
			}
			puts("Incorrect password.");
			sleep(1);
		}
		return false;
	}
	else
	{
		return create_player();
	}
}


/*
	Tries to find and call the action user requested
*/
void execute_action(char cmd_char)
{
	//convert upper case to lower case
	if (cmd_char < 'a' && cmd_char > '9') /* accept letters and numbers */
	{
		cmd_char += 'a' - 'A';
	}

	// search for the corresponding action
	for (size_t i = 0; i < player.location->action_count; i++)
	{
		if (player.location->actions[i].hotkey == cmd_char)
		{
			// execute action
			player.location->actions[i].function();
			break;
		}
	}
}

void init_ncurses() {
	init_ui();
}


/*
	Main gameloop here
*/
void enter_game()
{
	ncurs_location();
	playing = true;
	while (playing)
	{
		int getch_res = getch();
		if (getch_res == ERR)
		{
			//eof or other read error
			playing = false;
			break;
		}
		execute_action(getch_res & 0xFF);
	}
}

/*
	Initializes ZeroMQ context and sockets
*/
bool init_zmq()
{
	if (!(zmq_context = zmq_init(1)))
	{
		return false;
	}
	if (!(push_socket = zmq_socket(zmq_context, ZMQ_PUSH)))
	{
		return false;
	}
	if (zmq_connect(push_socket, "tcp://localhost:5558"))
	{
		return false;
	}
	if (!(chat_socket = zmq_socket(zmq_context, ZMQ_SUB)))
	{
		return false;
	}
	if (zmq_connect(chat_socket, "tcp://localhost:5559"))
	{
		return false;
	}
	if (zmq_setsockopt(chat_socket, ZMQ_SUBSCRIBE,
			CHATMSG_PREFIX, sizeof(CHATMSG_PREFIX)-2)) // strip trailing space
	{
		return false;
	}
	int linger_time = 250; // pending messages linger for 250 ms if socket is closed
	if (zmq_setsockopt(chat_socket, ZMQ_LINGER,
			&linger_time, sizeof(linger_time))) // strip trailing space
	{
		syslog(LOG_WARNING, "Can not set ZMQ_LINGER: %s", zmq_strerror(errno));
	}
	return true;
}

void cleanup_zmq()
{
	zmq_close(chat_socket);
	zmq_close(push_socket);
	zmq_term(zmq_context);
}

bool init_pq()
{
	PGresult *res = NULL;
    conn = PQconnectdb("dbname=todd user=todd");
    if (PQstatus(conn) != CONNECTION_OK)
	{
		goto pq_cleanup;
	}
	res = PQprepare(conn, "get_login_id", "select id from player_logins where name = $1;", 1, NULL);
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
	res = PQprepare(conn, "new_login", "insert into player_logins (name, passwd) values ($1, crypt(cast($2 as text), gen_salt('bf')));", 2, NULL);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		goto pq_cleanup;
	}
	PQclear(res);
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

/*
	Main entry point. Initializes global resources, logs player on 
	and enters game.
*/
int main(int argc, char *argv[])
{
	int return_code = EXIT_FAILURE;
	openlog("ToDD", LOG_PID|LOG_PERROR, LOG_USER);

	if (!init_pq())
	{
		goto cleanup;
	}

	if (!init_zmq())
	{
		syslog(LOG_ERR, "ZeroMQ init failure: %s", zmq_strerror(errno));
		goto cleanup;
	}
	srand((unsigned int)time(NULL));

	if (!get_player())
	{
		syslog(LOG_ERR, "Player auth failure");
		goto cleanup;
	}
	load_player_data();

	init_ncurses();
	enter_game();

	return_code = EXIT_SUCCESS; // returned from game, success

cleanup:
	cleanup_zmq();
	cleanup_pq();
	free(player.name);
	closelog();
	endwin(); /* for curses */
	return return_code;
}
