/*
 *  tenace - bridge hand viewer and editor
 *  Copyright (C) 2005-2007 Christoph Berg <cb@df7cb.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bridge.h"
#include "functions.h"
#include "support.h"
#include "window_board.h"

/* bridge math */

void calculate_target(board *b)
{
	int side = b->declarer % 2;
	b->target[side] = b->level + 6;
	b->target[1 - side] = 14 - b->target[side]; /* 1 more to beat contract */
}

int
card_overtricks (board *b, card c)
{
	assert (b->card_score[c] >= 0);

	return b->card_score[c] - 6 - b->level;
}

int
card_is_good (board *b, card c)
{
	assert (b->card_score[c] >= 0);

	int side = b->current_turn % 2;
	if (side == (b->declarer % 2))
		return b->card_score[c] >= b->target[side];
	else
		return 13 - b->card_score[c] >= b->target[side];

	/* NOT REACHED */
}

/* dealing with boards */

void board_clear(board *b)
{
	b->n_dealt_cards = b->n_played_cards = 0;
	int i;
	for (i = 0; i < 52; i++) {
		b->cards[i] = 0;
		b->dealt_cards[i] = 0;
		b->card_score[i] = -1;
		b->played_cards[i] = -1;
	}
	for (i = 0; i < 4; i++)
		b->hand_cards[i] = 0;
	b->current_turn = seat_mod(b->declarer + 1);

	b->par_score = -1;
	b->par_dec = b->par_suit = b->par_level = b->par_tricks = 0;
}

void board_set_contract(board *b, int level, suit trump, seat declarer, int doubled)
{
	b->level = level;
	b->trumps = trump;
	b->declarer = declarer;
	b->current_turn = seat_mod(declarer + 1);
	b->doubled = doubled;
	calculate_target(b);
}

board *board_new(void)
{
	int i;
	char *names[] = {"West", "North", "East", "South"};
	board *b = malloc(sizeof(board));
	assert(b);

	b->filename = NULL;
	b->name = g_string_new("Board 1");
	for (i = 0; i < 4; i++) {
		b->hand_name[i] = g_string_new(names[i]);
	}

	b->dealer = north;
	board_set_contract(b, 1, NT, south, 0);
	board_clear(b);
	b->vuln[0] = b->vuln[1] = 0;

	b->bidding = calloc(4, sizeof(card));
	assert(b->bidding);
	b->n_bids = 0;
	b->n_bid_alloc = 4;

	return b;
}

void board_free(board *b)
{
	int i;
	assert(b);
	if (b->filename)
		g_string_free(b->filename, TRUE);
	g_string_free(b->name, TRUE);
	for (i = 0; i < 4; i++) {
		g_string_free(b->hand_name[i], TRUE);
	}
	free(b->bidding);
}

/* dealing with cards */

int assert_board(board *b) /* check proper number of cards in hands */
{
	int i;
	for (i = 1; i < 4; i++)
		if (b->hand_cards[0] != b->hand_cards[i])
			return 0;
	return 1;
}

int add_card(board *b, seat s, card c)
/* return: 1 = card added */
{
	if (b->dealt_cards[c] != 0)
		return 0;
	if (b->hand_cards[s-1] == 13)
		return 0;

	b->cards[c] = s;
	b->dealt_cards[c] = s;
	b->n_dealt_cards++;
	b->hand_cards[s-1]++;

	b->par_score = -1;

	return 1;
}

int remove_card(board *b, seat s, card c)
/* return: 1 = card removed */
{
	assert (b->dealt_cards[c] == s);

	b->cards[c] = 0;
	b->dealt_cards[c] = 0;
	b->n_dealt_cards--;
	b->hand_cards[s-1]--;

	b->par_score = -1;

	return 1;
}

void deal_random(board *b)
{
	seat s;
	for (s = west; s <= south; s++) {
		while (b->hand_cards[s-1] < 13) {
			int c = rand() % 52;
			if (b->dealt_cards[c] == 0) {
				int ret = add_card(b, s, c);
				assert(ret);
			}
		}
	}
}

/* playing */

static int has_suit(seat *cards, seat h, suit s)
{
	int i;
	for (i = 0; i < 52; i++)
		if (SUIT(i) == s && cards[i] == h)
			return 1;
	return 0;
}

static void play_card_0(board *b, seat s, card c)
{
	assert (s);
	assert (b->dealt_cards[c] == s);
	assert (b->cards[c] == s);

	b->cards[c] = 0;

	b->played_cards[b->n_played_cards] = c;
	b->n_played_cards++;
}

int play_card(board *b, seat s, card c)
{
	board_statusbar(NULL);

	if (b->cards[c] != b->current_turn) {
		board_statusbar("Not your turn");
		return 0;
	}

	int firstcard = 0;
	card lead = 0;
	if (b->n_played_cards % 4 != 0) { /* must follow suit */
		firstcard = b->n_played_cards - (b->n_played_cards % 4);
		lead = b->played_cards[firstcard];
		if (SUIT(c) != SUIT(lead) && has_suit(b->cards, s, SUIT(lead))) {
			board_statusbar("Please follow suit");
			return 0;
		}
	}

	play_card_0(b, s, c);

	if (b->n_played_cards % 4 == 0) { /* trick complete */
		seat leader = b->dealt_cards[lead];
		card wincard = lead;
		b->current_turn = leader;
		int i;
		for (i = 1; i <= 3; i++) {
			card thiscard = b->played_cards[firstcard + i];
			if ((SUIT(thiscard) == b->trumps && SUIT(wincard) != b->trumps) ||
			    (SUIT(thiscard) == SUIT(wincard) && RANK(thiscard) > RANK(wincard))) {
				wincard = thiscard;
				b->current_turn = seat_mod(leader + i);
			}
		}
		b->tricks[b->current_turn % 2]++;
	} else {
		b->current_turn = seat_mod(b->current_turn + 1);
	}

	return 1;
}

int rewind_card(board *b)
{
	if (b->n_played_cards == 0) {
		board_statusbar("Nothing to undo");
		return 0;
	}

	b->n_played_cards--;

	if (b->n_played_cards % 4 == 3)
		b->tricks[b->current_turn % 2]--;

	card c = b->played_cards[b->n_played_cards];
	assert (b->cards[c] == 0);
	b->current_turn = b->cards[c] = b->dealt_cards[c];

	return 1;
}

void board_rewind(board *b)
{
	while (b->n_played_cards)
		rewind_card(b);
}

int next_card(board *b)
{
	if (b->n_played_cards >= b->n_dealt_cards) {
		board_statusbar("No cards left to play");
		return 0;
	}
	if (b->played_cards[b->n_played_cards] == -1) {
		board_statusbar("Which card should I play?");
		return 0;
	}
	if (b->cards[b->played_cards[b->n_played_cards]] == 0) {
		board_statusbar("Card was already played");
		return 0;
	}
	if (b->cards[b->played_cards[b->n_played_cards]] != b->current_turn) {
		board_statusbar("Card belongs to wrong player");
		return 0;
	}
	return play_card(b, b->current_turn, b->played_cards[b->n_played_cards]);
}

void board_fast_forward(board *b)
{
	while (next_card(b));
}

/* bidding */

void board_append_bid(board *b, card bid)
{
	if (b->n_bids >= b->n_bid_alloc) {
		b->n_bid_alloc <<= 2;
		b->bidding = realloc(b->bidding, b->n_bid_alloc);
		assert(b->bidding);
	}
	b->bidding[b->n_bids++] = bid;
}
