/*
 * Copyright (c) 1997 - 2004 Hj. Malthaner
 *
 * Line management
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include <stdio.h>

#include "messagebox.h"
#include "schedule_list.h"
#include "times_history.h"
#include "vehicle_manager.h"
#include "components/gui_convoiinfo.h"
#include "line_item.h"

#include "../simcolor.h"
#include "../simdepot.h"
#include "../simhalt.h"
#include "../simworld.h"
#include "../simevent.h"
#include "../display/simgraph.h"
#include "../simskin.h"
#include "../simconvoi.h"
#include "../vehicle/simvehicle.h"
#include "../gui/simwin.h"
#include "../simlinemgmt.h"
#include "../simmenu.h"
#include "../utils/simstring.h"
#include "../player/simplay.h"
#include "../gui/line_class_manager.h"

#include "../bauer/vehikelbauer.h"

#include "../dataobj/schedule.h"
#include "../dataobj/translator.h"
#include "../dataobj/livery_scheme.h"
#include "../dataobj/environment.h"

#include "../boden/wege/kanal.h"
#include "../boden/wege/maglev.h"
#include "../boden/wege/monorail.h"
#include "../boden/wege/narrowgauge.h"
#include "../boden/wege/runway.h"
#include "../boden/wege/schiene.h"
#include "../boden/wege/strasse.h"


#include "karte.h"

#define VEHICLE_NAME_COLUMN_WIDTH ((D_BUTTON_WIDTH*3)+11+4)
#define SCL_HEIGHT (15*LINESPACE)

static convoihandle_t selected_cnv;

vehicle_manager_t::vehicle_manager_t(player_t *player_) :
	gui_frame_t( translator::translate("vehicle_manager"), player_),
	player(player_),
	scl(gui_scrolled_list_t::listskin)
{


	// init scrolled list
	scl.set_pos(scr_coord(0, 1));
	scl.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 11 - 4, SCL_HEIGHT - 18));
	scl.set_highlight_color(player->get_player_color1() + 1);
	scl.add_listener(this);

	// resize button
	set_min_windowsize(scr_size(488, 300));
	set_windowsize(scr_size(488, 300));
	set_resizemode(diagonal_resize);
	resize(scr_coord(0, 0));
	resize(scr_coord(192, 126)); // suitable for 4 buttons horizontally and 4 convoys vertically

	build_vehicle_list(0);
}


vehicle_manager_t::~vehicle_manager_t()
{
}


/**
 * Mouse clicks are hereby reported to the GUI-Components
 */
bool vehicle_manager_t::infowin_event(const event_t *ev)
{

	return gui_frame_t::infowin_event(ev);
}


bool vehicle_manager_t::action_triggered( gui_action_creator_t *comp, value_t v )           // 28-Dec-01    Markus Weber    Added
{
	if (comp == &scl) {
		if (line_scrollitem_t *li = (line_scrollitem_t *)scl.get_element(v.i)) {
			//update_lineinfo(li->get_line());
		}
		else {
			// no valid line
			//update_lineinfo(linehandle_t());
		}
		//selected_line[selected_tab] = line;
		//selected_cnv = line; // keep these the same in overview
	}
	return true;
}



void vehicle_manager_t::draw(scr_coord pos, scr_size size)
{
	gui_frame_t::draw(pos, size);
}


void vehicle_manager_t::display(scr_coord pos)
{

}


void vehicle_manager_t::set_windowsize(scr_size size)
{
	gui_frame_t::set_windowsize(size);

}


void vehicle_manager_t::build_vehicle_list(int filter)
{
	sint32 sel = -1;
	vector_tpl<line_scrollitem_t *>selected_lines;

		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player) {
			selected_lines.append(new line_scrollitem_t(cnv));
		}
	}

	FOR(vector_tpl<line_scrollitem_t*>, const i, selected_lines) {
		scl.append_element(i);
		if (cnv == i->get_cnv()) {
			sel = scl.get_count() - 1;
		}
	}

	scl.set_selection(sel);
	scl.sort(0, NULL);

	old_line_count = player->simlinemgmt.get_line_count();

}

uint32 vehicle_manager_t::get_rdwr_id()
{
	return magic_vehicle_manager_t+player->get_player_nr();
}

