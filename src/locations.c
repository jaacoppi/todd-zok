#include <stdio.h>

#include "actions.h"
#include "location.h"

Location loc_town = {
	"You are now standing on the main street of Barathum, the village next to the famous dungeons."
	"\nThere's a path leading leading to the dungeons."
	"\n\nThe adventure awaits...",
	9,
	{
		{
			'd',
			"",
			"Dungeons",
			&ac_dungeons
		},
		{
			'l',
			"",
			"List players",
			&ac_list_players
		},
		{
			'v',
			"",
			"View stats",
			&ac_blank // done through todd_getchar, this just displays text
		},
		{
			't',
			"",
			"Tavern",
			&ac_tavern
		},
		{
			'w',
			"",
			"Warrior arena",
			&ac_warena
		},
		{
			's',
			"",
			"Shop",
			&ac_shop
		},
		{
			'm',
			"",
			"Messageboard",
			&ac_messageboard
		},
		{
			'g',
			"",
			"Graveyard",
			&ac_graveyard
		},
		{
			'q',
			"",
			"Quit",
			&ac_quit
		}

	}
};

Location loc_dungeons_level1 = {
	"You follow a road that is supposed to lead to dungeons. At the end of the road you find a barred gateway with a sign:\n"
	"\n"
	"\t+----------------+\n"
	"\t| Beware of bugs |\n"
	"\t+----------------+\n"
	"\n"
	"As you ponder how large bugs must be to threaten grown  men, you notice faint, strangely compelling, reddish glow emanating from nearby ruins."
	"\n"
	"You may enter the dungeons or look for action in the surrounding forest.",
	4,
	{
		{
			'e',
			"",
			"Enter dungeons",
			&ac_dungeons	// ac_dungeons sets the correct level (wrapper)
		},

		{
			'a',
			"Look for ",
			"Action",
			&ac_dungeons_action
		},
		{
			'g',
			"Examine the ",
			"Glow",
			&ac_dungeons_glow
		},
		{
			'r',
			"",
			"Return to town",
			&ac_return_to_town
		}
	}
};

Location loc_dungeons_level2 = {
	"You enter the dungeons. There are dimly lit torches on the wall. You realize that other people have been here.\n"
	"\nTODO: Finish the description, add something else besides look for action\n",
	2,
	{
		{
			'a',
			"Look for ",
			"Action",
			&ac_dungeons_action
		},
		{
			'r',
			"",
			"Return to fresh air",
			&ac_return_to_town
		}
	}
};


Location loc_tavern = {
	"The tavern is eerlily quiet.\n\nBartender Willie is alphabetically ordering his beer bottles.",
	6,
	{
		{
			't',
			"",
			"Talk to Willie",
			&ac_tavern_bartender
		},
		{
			'l',
			"",
			"List parties",
			&ac_party_list
		},
		{
			'j',
			"",
			"Join party",
			&ac_party_join
		},
		{
			'e',
			"Leav",
			"e your party",
			&ac_party_leave
		},

		{
			'g',
			"",
			"Gather a party",
			&ac_party_gather
		},

		{
			'r',
			"",
			"Return to town",
			&ac_return_to_town
		}
	}
};


Location loc_tavern_bartender = {
	"What do you want?",
	3,
	{
		{
			'g',
			"",
			"Get a room",
			&ac_tavern_room
		},
		{
			'i',
			"",
			"Information",
			&ac_tavern_info
		},
		{
			'r',
			"",
			"Return to tavern",
			&ac_tavern
		}
	}
};

/* used when player gets a room and quits */
Location loc_room_offline = {
	"",
	0,
	{}
};
Location loc_warena = {
	"You enter Bren's Warrior arena.\n\nThe place is populated with warriors-to-be, training in all aspects of fighting.",
	2,
	{
		{
			'a',
			"",
			"Ask for a lesson",
			&ac_warena_skills
		},
		{
			'r',
			"",
			"Return to town",
			&ac_return_to_town
		}
	}
};

Location loc_shop = {
	"You see a scrawny man in one of the shacks that make up most of the town. The man introduces himeself as a\nshopkeeper, but this not like any shop you've ever seen.",
	3,
	{
		{
			'b',
			"",
			"Buy items",
			&ac_shop_buy
		},
		{
			's',
			"",
			"Sell items",
			&ac_shop_sell
		},
		{
			'r',
			"",
			"Return to town",
			&ac_return_to_town
		}
	}
};

Location loc_fight = {
	"",
	6,
	{
		{
			'r',
			"",
			"Run",
			&ac_run_fight
		},
		{
			'v',
			"",
			"View stats",
			&ac_blank // done through todd_getchar, this just displays text
		},

		{
			'1',
			"",
			NULL,
			&ac_fight_0
		},

		{
			'2',
			"",
			"Press 1-4",
			&ac_fight_1
		},
		{
			'3',
			"",
			NULL,
			&ac_fight_2
		},

		{
			'4',
			"",
			NULL,
			&ac_fight_3
		}

	}

};

Location loc_shrine = {
	"In the forest you find a peaceful pond.\n\nIn the middle of it you see a crystal faintly glowing red.\nThe pond is filled with coins.\n\nThere's a sign that says \"1 for 1, 10 for 5\".",
	3,
	{
		{
			'1',
			"Throw ",
			"1 coin",
			&ac_shrine_heal_1
		},
		{
			'5',
			"Throw ",
			"5 coins",
			&ac_shrine_heal_all
		},
		{
			'r',
			"",
			"Return to dungeons",
			&ac_dungeons
		}
	}
};


Location loc_messageboard = {
	"There is a large messageboard in the middle of the town square. It is plastered with layers of old notes. Most of them are weathered completely unintelligible. Beside the board is a small table with a pen and a stack of empty papers.",
	3,
	{
		{
			'v',
			"",
			"View messages",
			&ac_messageboard_view
		},
		{
			'w',
			"",
			"Write a message",
			&ac_messageboard_write
		},
		{
			'r',
			"",
			"Return to town",
			&ac_return_to_town
		}
	}
};

// TODO: Talk to Gravedigger Earl for quests etc
Location loc_graveyard = {
	"The graveyard has an air of calmness and peace.\nHere lie adventurers whose bodies have been recovered.",
	2,
	{
		{
			'v',
			"",
			"View the graves",
			&ac_graveyard_view
		},
		{
			'r',
			"",
			"Return to town",
			&ac_return_to_town
		}
	}
};
Location loc_yesno = {
	"", /* NULL here would mean there be dragons. Always have "" instead */
	2,
	{
		{
			'y',
			"",
			"Yes",
			NULL
		},
		{
			'n',
			"",
			"No",
			NULL
		}
	}	
};
