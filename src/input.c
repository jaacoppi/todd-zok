#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <zmq.h>
#include <ncurses.h>
#include "globals.h"

#include "actions.h"
#include "ui.h"
#include "networking.h"

extern int chat_typing;
extern int toggle_chat;

void parse_and_print_chatmsg(char *msg)
{
	char *prefix = strtok_r(NULL, "|", &msg);

	if (strcmp(prefix, CHATMSG_PREFIX) == 0) {
		char *nick = strtok_r(NULL, "|", &msg);
		ncurs_log_chatmsg(msg, nick);
	}
}

void parse_ctrlmsg(char *msg)
{
	char *prefix = strtok_r(NULL, "|", &msg);
	prefix = strtok_r(NULL, "|", &msg);

	if (strcmp(prefix, JOINMSG) == 0) {
		char *joining_plr = strtok_r(NULL, "|", &msg);
		ncurs_log_sysmsg(_("%s has entered the realm"), joining_plr);
	}

	if (strcmp(prefix, QUITMSG) == 0) {
		char *leaving_plr = strtok_r(NULL, "|", &msg);
		ncurs_log_sysmsg(_("%s has left the realm"), leaving_plr);
	}
}

bool todd_getchar(unsigned char *c)
{
	zmq_pollitem_t items [4];
	items[0].socket = chat_socket;
	items[0].events = ZMQ_POLLIN;
	items[1].socket = NULL;
	items[1].fd = fileno(stdin);
	items[1].events = ZMQ_POLLIN;
	items[2].socket = party_socket;
	items[2].events = ZMQ_POLLIN;
	items[3].socket = ctrl_socket;
	items[3].events = ZMQ_POLLIN;
	/* Poll for events indefinitely */
	do
	{
		int rc = zmq_poll (items, 4, -1);
		if (rc < 0)
		{
			return false;
		}
		if (items[0].revents & ZMQ_POLLIN)
		{
			char *msg = try_recv_msg(chat_socket);
			parse_and_print_chatmsg(msg);
			free(msg);
		}
		if (items[2].revents & ZMQ_POLLIN)
		{
			char *msg = try_recv_msg(party_socket);
			// TODO real prefix parsing. avoid buffer overrun
			parse_and_print_chatmsg(msg+11); // skip partyprefix
			free(msg);
		}

		if (items[3].revents & ZMQ_POLLIN)
		{
			char *msg = try_recv_msg(ctrl_socket);
			parse_ctrlmsg(msg); 
			free(msg);
		}


	} while (!(items[1].revents & ZMQ_POLLIN));
	unsigned char dummy;
	if (c == NULL) c = &dummy;

	/* if no events from network, return a local getch */
	int rc = read(fileno(stdin), c, sizeof(char));

	/* toggle chat window */
	if (rc == 1)
	{
		if (*c == '\t')
		{
			// this is a dirty hack; input_win is only NULL when logging in
			// when logging in, don't toggle chat_typing (would cause random *** to appear when writing your username
			if (input_win != NULL)	
			{
				if (chat_typing == 0) /* if not typing yet, goto chat toggle */
				{
					ncurs_chat(player);
				}
				else
				{
					chat_typing = 0; /* if typing already, cancel it */
					ncurs_chat(player);
				}
			}
		}



		if (*c == 'v') // v for View Stats
		{
		if (chat_typing == 0) // don't view stats when in line input mode
		{
			ac_view_stats();
			wclear(command_win);
			wrefresh(command_win);
			// empty commands and wait for a getch
			ncurs_modal_msg(""); // only "continue" and getch

			set_player_location(player.location);
		}
		}
		// no special keys pressed, input as normal
		return true;
	}
	else	
	{
		return false;
	}
}


// WINDOW *echowindow points to the window that processes the input
// basically it's input_win during the game and stdscr before the game screen is loaded
bool todd_getline(char **line, size_t *len, WINDOW *echowindow)
{
	// curs_set displays a nice cursor for user convenience
	curs_set(1);
	chat_typing = 1;
	bool ret = false;
	size_t buf_len = 20;
	*line = malloc(buf_len);
	*len = 0;
	unsigned char c;
	do
	{
		bool rc = todd_getchar(&c);
		if (!rc || c == '\t')	
		{
		// pressing TAB (\t) when in input mode is supposed to toggle chat
		//
		// however, pressing TAB when logging in (typing name or password)
		// would cause the program to quit
		// -> don't accept TAB when logging in

		// echowindow == input_win either when 
		//	todd_getline is called with input_win
		// OR   they are both NULL (when logging in)
		//	therefore, those two cases can't be true at the same time
		if (echowindow == input_win && input_win != NULL)
			{		
				free(*line);
				*line = NULL;
				*len = 0;
				goto cleanup;
	
			}
		continue;	// don't accept TAB unless in input_win, just skip it

		}
		if (c == '\b') // it's a backspace, go back a character
		{
			// if echowindow is NULL it means we're asking for the password
			// in this case, make backspace work
			int password = 0;
			if (echowindow == NULL)
				{
				password = 1;
				echowindow = stdscr;
				}

			// don't backspace on an empty string or the pointer will cause a segfault
			if ((*len) != 0) 
			{
				// if it's a multibyte (scandic letters, § and so on)
				// len -1 and len -2 have negative values
				int multibyte = 0;
				if ((*line)[*len - 1] < 0)
					multibyte = 1;

				int y,x;
				// get current position of cursor to y and x
				getyx(echowindow,y,x);
				// move the cursor left by one
				wmove(echowindow, y,x-1);
				// blank it from screen and from buffer
				wechochar(echowindow, ' ');
				(*line)[*len] = '\0';
				(*len)--;
				if (multibyte) // there's two chars in buffer, not one
				{
				(*line)[*len] = '\0';
				(*len)--;
				}

				// by calling wechochar, the cursor moves right. move it back
				wmove(echowindow, y,x-1);
				wrefresh(echowindow);

			// if this was in the password field, change echowindow back to original value
			if (password)
				echowindow = NULL;
			}
		}
		else	// it's just a normal character
		{
			if (buf_len <= *len)
			{
				buf_len += 20;
				*line = realloc(*line, buf_len);
			}

			(*line)[*len] = c;
			(*len)++;
			// echo the character, except when echowindow is NULL echo a * to stdscr
			// this is a hack: echowindow is NULL only when asking for a password
			// also, don't echo a \r
			if (echowindow == NULL && c != '\r')
				wechochar(stdscr, '*');
			else
			if (c != '\r')	// without this, todd_getline would eat the "Halt! who goes there" -message 
					// when asking for player name
				wechochar(echowindow, c);
		}

	} while (c != '\r');

	(*len)--; // strip trailing newline
	(*line)[*len] = '\0'; // insert null terminator

	curs_set(0);

	if (*len == 0) // it's an empty string..
		return false;
	// else, return true
	ret = true;

cleanup:
	if (echowindow == input_win)
		{
		werase(echowindow);
		wrefresh(echowindow);
		}

	chat_typing = 0;
	curs_set(0);

	return ret;
}
