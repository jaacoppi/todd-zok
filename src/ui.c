#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <syslog.h>
#include <sys/param.h> // for MIN()
#include <stdlib.h>

#include "ui.h"
#include "globals.h"
#include "skills.h"
#include "character.h"
#include "actions.h"
#include "networking.h"

WINDOW *background_win;
WINDOW *command_win;
WINDOW *skills_win;
WINDOW *game_win;
WINDOW *log_win;
WINDOW *input_win;
WINDOW *fight_stat_win[6];

/* TODO: zokier doesn't like defines, jaacoppi does.. */
/* SKILLS_Y is used to calculate Weapon & Skills -window height */
#define SKILLS_Y 7

void draw_background(int x_size, int y_size);
int y_size, x_size; /* used for bolding titles, must be global */
int fight_statw_width;
int gamew_logw_sep; 

// inits ncurses so we can use wclear() and so on
void init_ncurs()
{
	// locale needs to be initialized for ncurses
	// "" sets "native" locale
	// TODO should we force some locale (UTF-8)
	// telnetd gives us "posix" locale
	char *locale = setlocale(LC_ALL, "");
	if (locale == NULL)
	{
		syslog(LOG_WARNING, "setlocale failed, continuing anyways");
	}

	/* ncurses init stuff */
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);	/* Function Keys  - problems with some terminals */
	curs_set(0); 		/* 0 = invisible, 1 = normal, 2 = really visible */
	refresh();

	/* checks that terminal size is big enough */
	getmaxyx(stdscr, y_size, x_size);
	if (y_size < 24 || x_size < 80)
	{
		printw("This program needs 80x24 characters of screen 	size to run.\n");
		printw("You currently have: %dx%d\n", x_size, y_size);
		printw("Enlarge your screen and press a key or this program might segfault.\n");
		refresh();
		todd_getchar(NULL);
	}

	/* checks that terminal supports color */

	if (has_colors() == FALSE)
	{
		printw("Your terminal does not seem to support color!\n");
		printw("The game will try to use color whenever possible\n");
		printw("You might experience problems\n");
		printw("Press a key to continue\n");
		refresh();
		todd_getchar(NULL);
	}

	/*
	// OLD CODE FOR COLOR STUFF, NOT USED AT THE MOMENT
	start_color();
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_BLUE, COLOR_BLACK);
	syntax for color usage:
	attron(COLOR_PAIR(1));
	wprintw(stdscr, "color");
	attroff(COLOR_PAIR(1));
	*/



}

// inits and draws the actual game screen
void init_ui()
{

	/* initialization of windows */
	/* naming reference:
	   mainbw = outer area, "box"
	   gamebw  = main game "box"
	   gamew = main game area
	   commandw = command listing window
	   skillsw = skills listing window
   */
	background_win = newwin(y_size, x_size, 0, 0);
	draw_background(x_size, y_size);
	wnoutrefresh(background_win);

	// calculate widths
	int remaining_width = x_size;
	remaining_width -= 2; // outer borders
	int cmdw_width = 17;
	remaining_width -= cmdw_width;
	remaining_width -= 2; // margins for cmdw
	remaining_width -= 1; // vertical line between cmdw and gamew
	int skillsw_width = cmdw_width;
	skillsw_width -= 2; // margin for index numbers
	int gamew_width = remaining_width / 2;
	remaining_width -= gamew_width;
	remaining_width -= 1; // vertical line between gamew and logw
	int logw_width = remaining_width;
	int inputw_width = logw_width;
	inputw_width -= 2; // margin for prompt

	// calculate heights
	int skillsw_height = 5;
	int cmdw_height = y_size;
	cmdw_height -= 2; // outer borders
	cmdw_height -= 1; // horiz line between cmdw and skillsw
	cmdw_height -= 1; // one line margin at top
	cmdw_height -= skillsw_height;
	int inputw_height = 1;
	int logw_height = y_size;
	logw_height -= 2; // outer borders
	logw_height -= 1; // horiz line between logw and inputw
	logw_height -= inputw_height;
	int gamew_height = y_size;
	gamew_height -= 2; // outer borders

	command_win = newwin(cmdw_height, cmdw_width, 2, 2);
	skills_win = newwin(skillsw_height, skillsw_width, y_size-(skillsw_height+1), 4);
	game_win = newwin(gamew_height, gamew_width, 1, cmdw_width+4);
	log_win = newwin(logw_height, logw_width, 1, (x_size-logw_width)-1);
	input_win = newwin(inputw_height, inputw_width, y_size-2, (x_size-inputw_width)-1);

	fight_statw_width = (gamew_width/2)-1; // lets leave few characters in the middle

	for (int i = 0; i < 3; i++)
	{
		fight_stat_win[i] = newwin(7, fight_statw_width, 1+(i*7), cmdw_width+4);
		fight_stat_win[i+3] = newwin(7, fight_statw_width, y_size - (1+((i+1)*7)), x_size-(logw_width+fight_statw_width+2));
		/* debugging: Show windows 
		wbkgd(fight_stat_win[i], '0' + i);
		wbkgd(fight_stat_win[i+3], '3' + i);
		box(fight_stat_win[i], 0, 0);
		box(fight_stat_win[i+3], 0, 0);
		wnoutrefresh(fight_stat_win[i]);
		wnoutrefresh(fight_stat_win[i+3]);
		*/
	}

	/* debugging: Show windows 
	wbkgd(command_win, 'C');
	wbkgd(skills_win, 'S');
	wbkgd(game_win, 'G');
	wbkgd(log_win, 'L');
	wbkgd(input_win, 'I');
	wnoutrefresh(command_win);
	wnoutrefresh(skills_win);
	wnoutrefresh(game_win);
	wnoutrefresh(log_win);
	wnoutrefresh(input_win);
	*/
	ncurs_skills(); /* for the lack of a better place updated here */
	doupdate();
}

void draw_background(int x_size, int y_size)
{
	// TODO use same calculations as the actual windows
	// cmd_win uses 20 characters, divide the rest of the screen between game_win and log_win
	int gamew_width = (x_size - 20)/2;
	// the position of the line between game_win and log_win
	gamew_logw_sep = gamew_width + 20;
	wclear(background_win);
	box(background_win, 0, 0);
	// line between cmd_win and game_win
	mvwvline(background_win, 1, 20, ACS_VLINE, y_size-2);
	// line between game_win and log_win
	mvwvline(background_win, 1, gamew_logw_sep, ACS_VLINE, y_size-2);
	// line between cmd_win and skills_win 
	mvwhline(background_win, y_size-SKILLS_Y, 1, ACS_HLINE, 20-1);
	logw_inputw_sep(); // line between logwindow and input window
	// corner characters
	mvwaddch(background_win, 0, 20, ACS_TTEE);
	mvwaddch(background_win, 0, gamew_logw_sep, ACS_TTEE);
	mvwaddch(background_win, y_size-1, 20, ACS_BTEE);
	mvwaddch(background_win, y_size-1, gamew_logw_sep, ACS_BTEE);
	mvwaddch(background_win, y_size-SKILLS_Y, 0, ACS_LTEE); /* Skills window horizontal line */
	mvwaddch(background_win, y_size-SKILLS_Y, 20, ACS_RTEE); /* Skills window horizontal line */
	mvwaddch(background_win, y_size-3, gamew_logw_sep, ACS_LTEE);
	mvwaddch(background_win, y_size-3, x_size-1, ACS_RTEE);

	
	/* 'W' for WEAPON in Weapon & Skills */
	mvwaddch(background_win, (y_size-SKILLS_Y+1), 2, ('W') | A_BOLD);

	// skill numbers for Weapon & Skills
	for (int i = 1; i < 5; i++)
	{
		mvwaddch(background_win, (y_size-SKILLS_Y+1)+i, 2, ('0' + i) | A_BOLD);
	}
	// input prompt
	mvwaddch(background_win, y_size-2, gamew_logw_sep+1, ACS_RARROW | A_BOLD);

	//window titles
	mvwaddstr(background_win, 0, 2, "Actions");
	mvwaddstr(background_win, y_size-7, 2, "Weapon & Skills");
	mvwaddstr(background_win, 0, 22, "GameW");
	mvwaddstr(background_win, 0, gamew_logw_sep+2, "Log");
	mvwaddstr(background_win, y_size-3, gamew_logw_sep+2, "Press TAB for Input");

	// version number
	wattron(background_win, A_DIM);
	mvwprintw(background_win, y_size-1, 20 + (gamew_width/2) - 5, "ToDD %s", REVID);
	wattroff(background_win, A_DIM);
}

void ncurs_location_desc() {
	wclear(game_win);
	mvwprintw(game_win, 0, 0, player.location->description);
	wrefresh(game_win);
}

void ncurs_commands() {
	wclear(command_win);
	for (size_t i = 0; i < player.location->action_count; i++)
	{
		if (player.location->actions[i].description != NULL)
		{
			wprintw(command_win, "%s", player.location->actions[i].description_prefix);
			waddch(command_win, player.location->actions[i].description[0] | A_BOLD | A_UNDERLINE);
			wprintw(command_win, "%s\n", player.location->actions[i].description + 1);
		}
	}
	wrefresh(command_win);
}

/* Prints a message to game window
 * Waits user to press a key */
void ncurs_modal_msg(const char *fmt, ...)
{
	/* this function has a getch(). Clear the command window so people won't get frustrated */
	werase(command_win);
	wrefresh(command_win);
	// TODO do we want to clear the window first?
	va_list argp;
	va_start(argp, fmt);
	vwprintw(game_win, fmt, argp);
	va_end(argp);
	wprintw(game_win, "\n\n%s\n", _("Continue..."));
	wrefresh(game_win);
	todd_getchar(NULL);
	set_player_location(player.location); /* after getch, redraw command_win */
}


/* Prints a message to log window
 * The message is styled as a "system" message */
void ncurs_log_sysmsg(const char *fmt, ...)
{
	scrollok(log_win, 1);
	wprintw(log_win, "-!- ");
	va_list argp;
	va_start(argp, fmt);
	vwprintw(log_win, fmt, argp);
	va_end(argp);
	waddch(log_win, '\n');
	wrefresh(log_win);
	scrollok(log_win, 0);
}

/* Print a message to log window
 * The message is styled as a "chat" message */
void ncurs_log_chatmsg(char *msg, char *source)
{
	scrollok(log_win, 1);
	wprintw(log_win, "%s> %s\n", source, msg);
	wrefresh(log_win);
	scrollok(log_win, 0);
}

/*	Prints descriptions at current player position */
void ncurs_location()
{
	ncurs_location_desc(player);
	ncurs_commands(player);
}

/* Draws character info in a slot defined by index parameter
 * indices:
 * +----+
 * | 0  |
 * | 1 5|
 * | 2 4|
 * |   3|
 * +----+
 */
void ncurs_fightinfo(Character *chr, int index)
{
	WINDOW *win = fight_stat_win[index];
	werase(win);
	box(win, 0, 0);
	int name_counter = 0;
	for (char *c = chr->name; *c != '\0'; c++)
	{
		mvwaddch(win, name_counter/(fight_statw_width-2), (name_counter%(fight_statw_width-2))+1, *c);
		name_counter++;
	}

	// committed turn already? - note that this uses hardcoded values
	if (chr->turnready < 0)
		mvwprintw(win,6,1,"Waiting..");
	else
		mvwprintw(win,6,1,"COMMITTED");

	if (fight_statw_width < 20)
	{
		/* TODO: this is ugly! */
		mvwprintw(win, 1, 1, "%s", chr->weapon->name);
		mvwprintw(win, 2, 1, "%s:%3d", _("HP"), chr->health);
		mvwprintw(win, 2, 5, "%s:%3d", _("Sta"), chr->stamina);

		for (int i = 0; i < 3; i++)
		{
			mvwprintw(win, i+3, 1, "%c%c:%2d", element_names[i][0], element_names[i][1], chr->elements[i]);
		}
		for (int i = 3; i < MIN(ELEM_COUNT, 6); i++) // MIN is not really necessary if we won't have ever 7 elements...
		{
			mvwprintw(win, i, fight_statw_width-6, "%c%c:%2d", element_names[i][0], element_names[i][1], chr->elements[i]);
		}
	}
	else
	{
		mvwprintw(win, 1, 1, "%s (%s)", chr->weapon->name, element_names[chr->weapon->dmg_type]);
		mvwprintw(win, 2, 1, "%s:%3d", _("Health"), chr->health);
		mvwprintw(win, 2, 15, "%11s:%d", _("Stamina"), chr->stamina);
		for (int i = 0; i < 3; i++)
		{
			mvwprintw(win, i+3, 1, "%s:%2d", element_names[i], chr->elements[i]);
		}
		for (int i = 3; i < MIN(ELEM_COUNT, 6); i++) // MIN is not really necessary if we won't have ever 7 elements...
		{
			mvwprintw(win, i, fight_statw_width-9, "%s:%2d", element_names[i], chr->elements[i]);
		}
	}
	wrefresh(win);
}

/* Refreshes the skill window */
void ncurs_skills()
{
	werase(skills_win);
	wprintw(skills_win,"%s\n",player.weapon->name);
	
	for (int i = 0; i < 4; i++)
	{
		wprintw(skills_win, "%s\n", player.skill[i]->name);
	}
	wrefresh(skills_win);
}

/* Prints a list of items to game window
 * returns the index of the item that player chooses
 * or -1 if player cancels or error occures 
 * Parameters:
 * first_item - Pointer to the description of first item
 * stride     - Memory offset between items
 * price_offset - Offset from first_item to integer price
 *                or < 1 if no prices should be printed
 * count      - number of items */
int ncurs_listselect(char **first_item, size_t stride, int price_offset, size_t count)
{
	werase(command_win);
	wrefresh(command_win);

	for (size_t i = 0; i < count; i++)
	{
		// pointer is cast to void and back to calculate the position of next string
		void *base = first_item;
		base += stride*i;
		wprintw(game_win, "%c) %s", i+'a', *(char**)base);
		if (price_offset > 0)
		{
			base += price_offset;
			wprintw(game_win, "\t%d\n", *(int*)base);
		}
		else
		{
			wprintw(game_win, "\n");
		}
	}
	// "Nevermind)
	wprintw(game_win, "x) %s\n", _("Nevermind"));
	wrefresh(game_win);

	unsigned char ch;
	while (true)
	{
		if (!todd_getchar(&ch))
		{
			return -1;
		}

		if (ch == 'x')  
		{
			/* "Nevermind */
			return -1;
		}

		/* only accept numbers between 0 and count */
		if (ch >= 'a' && ch < (char)(('a' + count)&0xFF))
		{
			return ch - 'a';
		}
	}
	return -1; /* control should never reach this point */
}

/* 
if toggle_chat is 0, keypresses are "normal commands"
toggle_chat 1 or 2 means party or global messages
*/
void ncurs_bold_input(int toggle_chat)
{
	/* empty the string. TODO: calculate proper area to empty */
	/* TODO: redraw the border line */
	mvwaddstr(background_win, y_size-3, gamew_logw_sep+2, "                          ");

	/* pressing tab always erase the current line */
	werase(input_win);
	/* note that erasing must be done in todd_getline also */

	logw_inputw_sep(); /* draw the log window / input window separator */
	switch (toggle_chat) 
	{
		case 0: /* INPUT - "normal mode" */
		{

			mvwaddstr(background_win, y_size-3, gamew_logw_sep+2, _("Press TAB for Input"));
			break;
		}

		case 1: /* party chat */
		{
			wattron(background_win,A_BOLD);
			mvwaddstr(background_win, y_size-3, gamew_logw_sep+2, _("Whisper to party members"));
			wattroff(background_win,A_BOLD);
			break;
		}

		case 2: /* global chat */
		{
			wattron(background_win,A_BOLD);
			mvwaddstr(background_win, y_size-3, gamew_logw_sep+2, _("Yell at patrons"));
			wattroff(background_win,A_BOLD);
			break;
		}

		case 999: /* Messageboard - NEVER LOOP THIS WITH A TAB KEYPRESS */
		{
			wattron(background_win,A_BOLD);
			mvwaddstr(background_win, y_size-3, gamew_logw_sep+2, _("Write to messageboard"));
			wattroff(background_win,A_BOLD);
			break;
		}
		default:
			break;

	}
wrefresh(background_win);
wrefresh(input_win);



}



int toggle_chat = 0;
int chat_typing = 0;
void ncurs_chat() 
{
/* TODO: figure out this toggle mess */

	toggle_chat += 1;
	if (toggle_chat >= 3) /* never loop messageboard */
		toggle_chat = 0;

	/* redraw the input line - chat toggle*/
	ncurs_bold_input(toggle_chat);

	char *line = NULL;
	size_t len = 0;
	if (toggle_chat != 0) 
	{
		if (todd_getline(&line, &len, input_win))
		{
			Message msg = create_chat_msg(line, len);
			/* a chat message was succesfully input(ted) */
			if (toggle_chat == 1)
			{
				// party message
				msg = wrap_as_partymsg(msg);
			}
			if (!send_msg(msg))
			{
				ncurs_log_sysmsg(_("Message send failure"));
			}
			del_msg(msg);
		}

		// return to basic input prompt -> press any key for game commands
		// this is done here because if todd_getline returns 0 it means there's an empty line 
		// TODO.. or todd_getline has an error...
		toggle_chat = 0;
		ncurs_bold_input(0); 
		free(line);
	}
}

// line between log_win and input_win
void logw_inputw_sep()
{
	mvwhline(background_win, y_size-3, gamew_logw_sep+1, ACS_HLINE, x_size-(gamew_logw_sep+2));
}



// ascii intro
void intro_ascii()
{
// clear screen. Note that only stdscr is initialized at this point
wclear(stdscr);
// can't use char[24][80] or #defines easily, since the ascii art is full of \ control chars


// read the ascii art from a file
#define ASCII_INTRO "data/intro"

// read the ascii data from a file, defining it here would be problematic because of all the control chars \ and so on

FILE *introfile = fopen(ASCII_INTRO,"r");
if (introfile != NULL) // if the file is not found, don't display it (would segfault)
	{
	char line[80];
	while (fgets(line, sizeof(line), introfile) != NULL) // read line by line until EOF
		wprintw(stdscr, "%s", line);

	fclose(introfile);
	}

// display credits in 
wprintw(stdscr,_("===================================================\n"));
wprintw(stdscr,_("World created by the Creator Gods Zokier & jaacoppi\n"));
wprintw(stdscr,_("===================================================\n"));

wrefresh(stdscr);
}


// clear all the fight windows after a fight
void ncurs_clear_fightwindows()
{
for (int index = 0; index <= 5; index++)
	{
	wclear(fight_stat_win[index]);
	wrefresh(fight_stat_win[index]);
	}
}
