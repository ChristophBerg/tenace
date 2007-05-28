#ifndef BRIDGE_H
#define BRIDGE_H

#include <glib.h>
#include <gtk/gtk.h>

typedef enum seat_e {
	west = 1,
	north,
	east,
	south,
} seat;

/* cards: SA = 51, C2 = 0, bids: 1C = 5, 7NT = 39 */
typedef enum suit_e {
	NT = 4,
	spade = 3,
	heart = 2,
	diamond = 1,
	club = 0,
} suit;

typedef enum rank_e {
	card2 = 0,
	card3,
	card4,
	card5,
	card6,
	card7,
	card8,
	card9,
	card10,
	cardJ,
	cardQ,
	cardK,
	cardA,
	cardX = 0x80,
	claim_rest = 0x81,
	bid_pass = 0,
	bid_x = 1,
	bid_xx = 2,
} rank;

typedef int card;

#define SUIT(c) ((int)((int)(c) / 13))
#define RANK(c) ((c) % 13)

#define LEVEL(c) ((int)((int)(c) / 5))
#define DENOM(c) ((c) % 5)

typedef struct board_t {
	GString *name;
	GString *filename;
	int vuln[2]; /* 0 = NS, 1 = EW */
	seat dealer;

	seat cards[52]; // 0 = not dealt
	int n_dealt_cards;
	seat dealt_cards[52]; // 0 = not dealt
	int hand_cards[4];

	GString *hand_name[4];
	seat declarer;
	suit trumps;
	int level;
	int doubled;

	int n_played_cards;
	card played_cards[52];
	seat current_turn;
	int tricks[2]; /* 0 = NS, 1 = EW */

	card *bidding;
	int n_bids;
	int n_bid_alloc;

	int card_score[52];
	int best_score;
	int target[2]; /* sum might be less than 13 for partial deals */

	int par_score; /* -1 = other par_ fields invalid */
	int par_dec, par_suit, par_level, par_tricks;
	int par_arr[4][5];

	GtkWidget *win; // window showing this board
} board;

/*
 * prototypes
 */

void board_statusbar(GtkWidget *win, char *text);
void calculate_target(board *b);
void board_clear(board *b);
void board_set_contract(board *b, int level, suit trump, seat declarer, int doubled);
board *board_new(void);
void board_free(board *b);

int assert_board(board *b);
int add_card(board *b, seat s, card c);
int remove_card(board *b, seat s, card c);
void deal_random(board *b);

int play_card(board *b, seat s, card c);
int rewind_card(board *b);
void board_rewind(board *b);
int next_card(board *b);
void board_fast_forward(board *b);

void board_append_bid(board *b, card bid);

#endif
