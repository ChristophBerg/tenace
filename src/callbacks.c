#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "bridge.h"
#include "file.h"
#include "functions.h"
#include "main.h"
#include "solve.h"
#include "window_board.h"
#include "window_card.h"
#include "window_line_entry.h"

static GtkWidget *window_imps = NULL;
static GtkWidget *window_info = NULL;

void
on_neu1_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	// FIXME add new board
	board_clear(b);
	card_window_update(b->dealt_cards);
	show_board(b, REDRAW_HANDS | REDRAW_PLAY);
}


void
on_open1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_load_dialog(b);
}

void
on_speichern1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_save_dialog(b, 0);
}


void
on_speichern_unter1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_save_dialog(b, 1);
}


void
on_beenden1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	exit(0);
}


void
on_ausschneiden1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_kopieren1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_einf__gen1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_l__schen1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_info1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (!window_info) {
		window_info = create_window_info ();
		GtkWidget *info_label = lookup_widget (window_info, "info_label");
		gtk_label_set_markup (GTK_LABEL (info_label), "<b>tenace</b> " VERSION"\n\n(C) 2007 Christoph Berg &lt;cb@df7cb.de&gt;");
		gtk_widget_show (window_info);
	}
}


gboolean
on_window_hand_delete_event            (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
	exit(1);
}


void
on_radiotoolbutton_west_clicked        (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	new_card_seat = west;
}


void
on_radiotoolbutton_north_clicked       (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	new_card_seat = north;
}


void
on_radiotoolbutton_east_clicked        (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	new_card_seat = east;
}


void
on_radiotoolbutton_south_clicked       (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	new_card_seat = south;
}


void
on_toolbutton_card_wipe_clicked        (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_clear(b);
	card_window_update(b->dealt_cards);
	show_board(b, REDRAW_HANDS | REDRAW_TRICKS);
}


void
on_toolbutton_card_random_clicked      (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	deal_random(b);
	card_window_update(b->dealt_cards);
	show_board(b, REDRAW_HANDS);
}


void
on_button_hand_save_clicked            (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_save_dialog(b, 0);
}


void
on_declarer_west1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_declarer (west);
}


void
on_declarer_north1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_declarer (north);
}


void
on_declarer_east1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_declarer (east);
}


void
on_declarer_south1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_declarer (south);
}


void
on_contract_no_trump1_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_trumps(NT);
}


void
on_contract_spades1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_trumps(spade);
}


void
on_contract_hearts1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_trumps(heart);
}


void
on_contract_diamonds1_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_trumps(diamond);
}


void
on_contract_clubs1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_trumps(club);
}


void
on_level1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_level (1);
}


void
on_level2_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_level (2);
}


void
on_level3_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_level (3);
}


void
on_level4_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_level (4);
}


void
on_level5_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_level (5);
}


void
on_level6_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_level (6);
}


void
on_level7_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_level (7);
}

void
on_level_doubled0_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_doubled (0);
}

void
on_level_doubled1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_doubled (1);
}


void
on_level_redoubled1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_doubled (2);
}

void
on_board1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
}



void
on_double_dummy1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_dds(b);
	hilight_dd_scores(b);
	show_board(b, REDRAW_HANDS | REDRAW_DD);
}


void
on_parscore1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	parscore(b);
}


void
on_handbutton_par_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	parscore(b);
}


void
on_vuln_none_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_vuln (0, 0);
}


void
on_vuln_ns_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_vuln (1, 0);
}


void
on_vuln_ew_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_vuln (0, 1);
}


void
on_vuln_all_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_vuln (1, 1);
}


void
on_set_par1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	b->trumps = b->par_suit;
	b->level = b->par_level;
	b->declarer = b->par_dec;
	b->current_turn = seat_mod(b->declarer + 1);
	b->doubled = b->par_tricks < b->par_level + 6;
	show_board(b, REDRAW_CONTRACT | REDRAW_NAMES);
}


void
on_imp_table1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (!window_imps) {
		window_imps = create_window_imps ();
		gtk_widget_show (window_imps);
	}
}


void
on_imps_ok_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
	if (window_imps) {
		gtk_widget_destroy(GTK_WIDGET(window_imps));
		window_imps = NULL;
	}
}


gboolean
on_window_imps_delete_event            (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
	window_imps = NULL;
	return FALSE;
}


void
on_info_ok_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
	if (window_info) {
		gtk_widget_destroy(GTK_WIDGET(window_info));
		window_info = NULL;
	}
}

gboolean
on_window_info_delete_event            (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
	window_info = NULL;
	return FALSE;
}



void
on_rewind_button_clicked               (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	rewind_card(b); /* extra call to show warning if there's nothing to do */
	board_rewind(b);
	show_board(b, REDRAW_HANDS | REDRAW_PLAY);
}



void
on_button_back_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	rewind_card(b);
	show_board(b, REDRAW_HANDS | REDRAW_PLAY);
}


void
on_button_next_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	next_card(b);
	show_board(b, REDRAW_HANDS | REDRAW_PLAY);
}


void
on_button_fast_forward_clicked         (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_fast_forward(b);
	show_board(b, REDRAW_HANDS | REDRAW_DD);
}


void
on_handbutton_gib_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_dds(b);
	hilight_dd_scores(b);
	show_board (b, REDRAW_HANDS | REDRAW_DD);
}


void
on_button_dd_toggled                   (GtkToggleToolButton *toggletoolbutton,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	run_dd = gtk_toggle_tool_button_get_active(toggletoolbutton);
	show_board (b, REDRAW_HANDS | REDRAW_DD);
}


void
on_pos_north_south_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	pos_score_for = 0;
}


void
on_pos_declarer_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	pos_score_for = 1;
}


void
on_pos_current_lead_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	pos_score_for = 2;
}


void
on_deal_clear_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_clear(b);
	card_window_update(b->dealt_cards);
	show_board(b, REDRAW_HANDS | REDRAW_PLAY);
}


void
on_deal_random_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	deal_random(b);
	card_window_update(b->dealt_cards);
	show_board(b, REDRAW_HANDS);
}


void
on_deal_line_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	window_line_entry_init(b);
}


void
on_line_entry_activate                 (GtkEntry        *entry,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_set_from_line_entry(b);
}


void
on_line_entry_ok_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
	board *b = CUR_BOARD;
	board_set_from_line_entry(b);
}


gboolean
on_window_line_entry_delete_event      (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
	window_line_entry = NULL;
	return FALSE;
}



void
on_dealer_west1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_dealer (west);
}


void
on_dealer_north1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_dealer (north);
}


void
on_dealer_east1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_dealer (east);
}


void
on_dealer_south1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	board_set_dealer (south);
}

