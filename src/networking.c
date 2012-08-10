#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <zmq.h>

#include "networking.h"
#include "globals.h"

Message create_chat_msg(char *body, size_t len)
{
	Message msg;
	size_t buf_len = sizeof(CHATMSG_PREFIX) + sizeof('|') + NAME_MAX_LENGTH + sizeof('|') + len + sizeof('\0');
	char *buf = malloc(buf_len);
	msg.body_len = snprintf(buf, buf_len, "%s|%s|%s", CHATMSG_PREFIX, player.name, body) + 1; // +1 null terminator
	msg.body = buf;
	return msg;
}

// send a join message to everybody
void send_join_msg()
{
        char msg_out[40];
        size_t len = snprintf(&msg_out[0], 40, "|%s|%s", JOINMSG, player.name)+1 + NAME_MAX_LENGTH;
	Message msg = create_ctrl_msg(msg_out, len);
	send_msg(msg);
        del_msg(msg);
	return;
}

// this currently works for 1 enemy only
// basically informs other players that the enemy is enemylist[dungeon_level][enemyid]
void party_call_to_arms()
{
	// a call to arms yell contains:
	// FIGHTMSG prefix + random seed
	
	// first, set the random seed correctly
	// must be done here so random events and other stuff don't mess up the randoms
	// the player who call people to join the fight sets the random seed and gives it to everyone

	randomseed = (unsigned int) time(NULL); // randomseed is a global holding the seed
	srand(randomseed);

        char msg_out[40];
        size_t len = snprintf(&msg_out[0], 40, "|%s|%d", FIGHTMSG,randomseed)+1;
	Message msg = create_ctrl_msg(msg_out, len);
	send_msg(msg);
        del_msg(msg);
	return;

}

// send a quit message to everybody
void send_quit_msg()
{

        char msg_out[40];
        size_t len = snprintf(&msg_out[0], 40, "|%s|%s", QUITMSG, player.name)+1 + NAME_MAX_LENGTH;
	Message msg = create_ctrl_msg(msg_out, len);
	send_msg(msg);
        del_msg(msg);
	return;
}

// send a join message to everybody
void send_turnready()
{
        char msg_out[40];
        size_t len = snprintf(&msg_out[0], 40, "|%s|%s|%d", TURNREADY, player.name, player.turnready) +1 + NAME_MAX_LENGTH;
	Message msg = create_ctrl_msg(msg_out, len);
	msg = wrap_as_partymsg(msg);
	send_msg(msg);
        del_msg(msg);
	return;
}

// when a player runs from a fight
void send_plr_ran()
{
        char msg_out[40];
        size_t len = snprintf(&msg_out[0], 40, "|%s|%s", RUNMSG, player.name) +1 + NAME_MAX_LENGTH;
	Message msg = create_ctrl_msg(msg_out, len);
	msg = wrap_as_partymsg(msg);
	send_msg(msg);
        del_msg(msg);
	return;
}

Message create_ctrl_msg(char *body, size_t len)
{
	Message msg;
	size_t buf_len = sizeof(CTRLMSG_PREFIX) + sizeof('|') + len + sizeof('\0');
	char *buf = malloc(buf_len);
	msg.body_len = snprintf(buf, buf_len, "%s|%s", CTRLMSG_PREFIX, body) + 1; // +1 null terminator
	msg.body = buf;
	return msg;
}

bool send_msg(Message msg)
{
	return zmq_sendmsg(msg.body, msg.body_len);
}

Message wrap_as_partymsg(Message msg)
{
	Message wrapped;
	size_t buf_len = msg.body_len + 12; // TODO no hardcoding/magic numbers
	char *buf = malloc(buf_len);
	wrapped.body_len = snprintf(buf, buf_len, "p%010d|%s", player_party.id, msg.body);
	wrapped.body = buf;
	del_msg(msg);
	return wrapped;
}

void del_msg(Message msg)
{
	free(msg.body);
	msg.body = NULL;
}

/* handles the actual sending of messages */
bool zmq_sendmsg(char *buf, int buf_len) {
	bool ret = true;
	size_t blob_len = strnlen(buf, buf_len);
	int rc;
	zmq_msg_t zmq_message;
	zmq_msg_init_size (&zmq_message, blob_len);
	char *blob = zmq_msg_data(&zmq_message);
	memcpy(blob, buf, blob_len);
	rc = zmq_send (push_socket, &zmq_message, 0);
	if (rc != 0)
	{
		ret = false;
		syslog(LOG_ERR, "zmq_sendmsg failure: %s", zmq_strerror(errno));
	}
	zmq_msg_close (&zmq_message);
	return ret;
}

char *try_recv_msg(void *sock)
{
    zmq_msg_t message;
    zmq_msg_init (&message);
    if (zmq_recv (sock, &message, ZMQ_NOBLOCK))
	{
		syslog(LOG_WARNING, "ZMQ receive failure: %s", zmq_strerror(errno));
        return (NULL);
	}
    int size = zmq_msg_size (&message);
    char *string = malloc (size + 1);
    memcpy (string, zmq_msg_data (&message), size);
    zmq_msg_close (&message);
    string [size] = 0;
    return (string);
}


// ugly globals go here
void *push_socket = NULL;
void *chat_socket = NULL;
void *party_socket = NULL;
void *ctrl_socket = NULL;
void *zmq_context = NULL;
bool zmq_python_up();
 

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
	if (!(party_socket = zmq_socket(zmq_context, ZMQ_SUB)))
	{
		return false;
	}
	if (zmq_connect(party_socket, "tcp://localhost:5559"))
	{
		return false;
	}

	// control
	if (!(ctrl_socket = zmq_socket(zmq_context, ZMQ_SUB)))
	{
		return false;
	}
	if (zmq_connect(ctrl_socket, "tcp://localhost:5559"))
	{
		return false;
	}



	if (zmq_setsockopt(chat_socket, ZMQ_SUBSCRIBE, CHATMSG_PREFIX, sizeof(CHATMSG_PREFIX)-1)) // strip null terminator
	{
		return false;
	}

	if (zmq_setsockopt(ctrl_socket, ZMQ_SUBSCRIBE, CTRLMSG_PREFIX, sizeof(CTRLMSG_PREFIX)-1)) // strip null terminator
	{
		return false;
	}

	if (zmq_setsockopt(chat_socket, ZMQ_SUBSCRIBE, DEBUGMSG_PREFIX, sizeof(DEBUGMSG_PREFIX)-1)) // strip null terminator 
	{
		return false;
	}

	int linger_time = 250; // pending messages linger for 250 ms if socket is closed
	if (zmq_setsockopt(push_socket, ZMQ_LINGER, &linger_time, sizeof(linger_time)))
		syslog(LOG_WARNING, "Can not set ZMQ_LINGER: %s\r\n", zmq_strerror(errno));

	return true;
}

void cleanup_zmq()
{
	zmq_close(chat_socket);
	zmq_close(push_socket);
	zmq_close(party_socket);
	zmq_close(ctrl_socket);
	zmq_term(zmq_context);
}


/* Function checks if python chat server is running. It must be! */
bool zmq_python_up()
{
	/* send a magic line, if you don't receive it, python server doesn't work correctly */
	#define MAGIC "ToDD-MAGIC321"

	char *msg = NULL;
	int token = rand();
	// TODO figure out correct length
	char msg_out[40];
	size_t len = snprintf(&msg_out[0], 40, "%s:%x", MAGIC, token)+1;
	zmq_pollitem_t items [2];
	items[0].socket = ctrl_socket;
	items[0].events = ZMQ_POLLIN;

		Message msg_foo = create_ctrl_msg(msg_out, len);
	// retry three times, sometimes zmq is slow to start
	for (int i = 0; i < 3; i++)
	{
		send_msg(msg_foo);
		int rc = zmq_poll (items, 1, 1000000);
		if (rc < 0)
		{
			syslog(LOG_WARNING, "ZMQ poll failure: %s\r\n", zmq_strerror(errno));
			continue;
		}
		else if (rc != 1)
		{
			syslog(LOG_WARNING, "ZMQ poll failure: no events\r\n");
			continue;
		}
		if (items[0].revents & ZMQ_POLLIN)
			msg = try_recv_msg(ctrl_socket);

		if (msg == NULL)
		{
			syslog(LOG_WARNING, "ZMQ recv failure: %s\r\n", zmq_strerror(errno));
			continue;
		}
		// TODO check the length of the msg
		// maybe strtok should be used instead of +sizeof(DEBUGMSG_PREFIX)
		if (strcmp(msg_out,msg+sizeof(CTRLMSG_PREFIX)) != 0) /* TODO???*/
		{
			syslog(LOG_WARNING, "ERROR received %s\r\n", msg);
			del_msg(msg_foo);
			continue;
		}
		del_msg(msg_foo);
		return true;
	}
	syslog(LOG_WARNING, "ZMQ chat test retry count exceeded\r\n");
	return false;
}

