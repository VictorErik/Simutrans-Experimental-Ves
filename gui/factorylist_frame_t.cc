/*
 * Copyright (c) 1997 - 2003 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

/*
 * Factory list window
 * @author Hj. Malthaner
 */

#include "factorylist_frame_t.h"

#include "../dataobj/translator.h"

/**
 * This variable defines the sort order (ascending or descending)
 * Values: 1 = ascending, 2 = descending)
 * @author Markus Weber
 */
bool factorylist_frame_t::sortreverse = false;

/**
 * This variable defines by which column the table is sorted
 * Values: 0 = Station number
 *         1 = Station name
 *         2 = Waiting goods
 *         3 = Station type
 * @author Markus Weber
 */
factorylist::sort_mode_t factorylist_frame_t::sortby = factorylist::by_name;

// filter by within current player's network
bool factorylist_frame_t::filter_fab = false;

const char *factorylist_frame_t::sort_text[factorylist::SORT_MODES] = {
	"Fabrikname",
	"Input",
	"Output",
	"Produktion",
	"Rating",
	"Power",
	"Sector"
};

factorylist_frame_t::factorylist_frame_t() :
	gui_frame_t( translator::translate("fl_title") ),
	sort_label(translator::translate("hl_txt_sort")),
	stats(sortby,sortreverse,filter_fab),
	scrolly(&stats)
{
	sort_label.set_pos(scr_coord(BUTTON1_X, 2));
	add_component(&sort_label);

	sortedby.init(button_t::roundbox, "", scr_coord(BUTTON1_X, 14), scr_size(D_BUTTON_WIDTH,D_BUTTON_HEIGHT));
	sortedby.add_listener(this);
	add_component(&sortedby);

	sorteddir.init(button_t::roundbox, "", scr_coord(BUTTON2_X, 14), scr_size(D_BUTTON_WIDTH,D_BUTTON_HEIGHT));
	sorteddir.add_listener(this);
	add_component(&sorteddir);

	filter_within_network.init(button_t::square_state, "Within own network", scr_coord(BUTTON3_X + D_H_SPACE, 14));
	filter_within_network.set_tooltip("Show only connected to own transport network");
	filter_within_network.add_listener(this);
	filter_within_network.pressed = filter_fab;
	add_component(&filter_within_network);

	// name buttons
	sortedby.set_text(sort_text[get_sortierung()]);
	sorteddir.set_text(get_reverse() ? "hl_btn_sort_desc" : "hl_btn_sort_asc");

	scrolly.set_pos(scr_coord(0, 14+D_BUTTON_HEIGHT+2));
	scrolly.set_scroll_amount_y(LINESPACE+1);
	add_component(&scrolly);

	set_windowsize(scr_size(D_DEFAULT_WIDTH, D_TITLEBAR_HEIGHT+18*(LINESPACE+1)+14+D_BUTTON_HEIGHT+2+1));
	set_min_windowsize(scr_size(D_DEFAULT_WIDTH, D_TITLEBAR_HEIGHT+4*(LINESPACE+1)+14+D_BUTTON_HEIGHT+2+1));

	set_resizemode(diagonal_resize);
	resize(scr_coord(0,0));
}



/**
 * This method is called if an action is triggered
 * @author Markus Weber/Volker Meyer
 */
bool factorylist_frame_t::action_triggered( gui_action_creator_t *comp,value_t /* */)
{
	if(comp == &sortedby) {
		set_sortierung((factorylist::sort_mode_t)((get_sortierung() + 1) % factorylist::SORT_MODES));
		sortedby.set_text(sort_text[get_sortierung()]);
		stats.sort(get_sortierung(),get_reverse(), get_filter_fab());
		stats.recalc_size();
	}
	else if(comp == &sorteddir) {
		set_reverse(!get_reverse());
		sorteddir.set_text(get_reverse() ? "hl_btn_sort_desc" : "hl_btn_sort_asc");
		stats.sort(get_sortierung(),get_reverse(), get_filter_fab());
		stats.recalc_size();
	}
	else if (comp == &filter_within_network) {
		filter_fab = !filter_fab;
		filter_within_network.pressed = filter_fab;
		stats.sort(get_sortierung(), get_reverse(), get_filter_fab());
		stats.recalc_size();
	}
	return true;
}


/**
 * resize window in response to a resize event
 * @author Hj. Malthaner
 * @date   16-Oct-2003
 */
void factorylist_frame_t::resize(const scr_coord delta)
{
	gui_frame_t::resize(delta);
	// window size - titlebar - offset (header)
	scr_size size = get_windowsize()-scr_size(0,D_TITLEBAR_HEIGHT+14+D_BUTTON_HEIGHT+2+1);
	scrolly.set_size(size);
}
