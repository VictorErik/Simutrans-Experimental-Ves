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

vehicle_manager_t::vehicle_manager_t(player_t *player_) :
	gui_frame_t( translator::translate("vehicle_manager"), player_),
	player(player_)
{
}


//vehicle_manager_t::~vehicle_manager_t()
//{
//	delete last_schedule;
//	// change line name if necessary
//	rename_line();
//}

//
///**
// * Mouse clicks are hereby reported to the GUI-Components
// */
//bool vehicle_manager_t::infowin_event(const event_t *ev)
//{
//
//	return gui_frame_t::infowin_event(ev);
//}
//
//
//bool vehicle_manager_t::action_triggered( gui_action_creator_t *comp, value_t v )           // 28-Dec-01    Markus Weber    Added
//{
//	
//	return true;
//}

//
//
//void vehicle_manager_t::draw(scr_coord pos, scr_size size)
//{
//	
//}
//
//
//void vehicle_manager_t::display(scr_coord pos)
//{
//
//}
//
//
//void vehicle_manager_t::set_windowsize(scr_size size)
//{
//	gui_frame_t::set_windowsize(size);
//
//}
//
//
//void vehicle_manager_t::build_line_list(int filter)
//{
//
//}
//
//uint32 vehicle_manager_t::get_rdwr_id()
//{
//	return magic_vehicle_manager_t+player->get_player_nr();
//}

