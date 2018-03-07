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

static vehiclehandle_t selected_veh;

vehicle_manager_t::vehicle_manager_t(player_t *player_) :
	gui_frame_t( translator::translate("vehicle_manager"), player_),
	player(player_),
	scrolly_vehicle_descs(&cont_veh_desc),
	scrolly_vehicles(&cont_veh)
{
	// Vehicle desc list
	cont_veh_desc.set_size(scr_size(200, 40));
	scrolly_vehicle_descs.set_pos(scr_coord(5, 14 + SCL_HEIGHT + D_BUTTON_HEIGHT * 2 + 4 + 2 * LINESPACE + 2));
	scrolly_vehicle_descs.set_show_scroll_x(true);
	scrolly_vehicle_descs.set_scroll_amount_y(40);
	scrolly_vehicle_descs.set_visible(false);
	add_component(&scrolly_vehicle_descs);

	// Vehicle list
	cont_veh.set_size(scr_size(200, 40));
	scrolly_vehicles.set_pos(scr_coord(VEHICLE_NAME_COLUMN_WIDTH + 5, 14 + SCL_HEIGHT + D_BUTTON_HEIGHT * 2 + 4 + 2 * LINESPACE + 2));
	scrolly_vehicles.set_show_scroll_x(true);
	scrolly_vehicles.set_scroll_amount_y(40);
	scrolly_vehicles.set_visible(false);
	add_component(&scrolly_vehicles);

	// resize button
	set_min_windowsize(scr_size(488, 300));
	set_windowsize(scr_size(488, 300));
	set_resizemode(diagonal_resize);
	resize(scr_coord(0, 0));
	resize(scr_coord(192, 126)); // suitable for 4 buttons horizontally and 4 convoys vertically

	build_vehicle_lists();

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
	////if (comp == &scl) {
	//	if (line_scrollitem_t *li = (line_scrollitem_t *)scl.get_element(v.i)) {
	//		//update_lineinfo(li->get_line());
	//	}
	//	else {
	//		// no valid line
	//		//update_lineinfo(linehandle_t());
	//	}
	//	//selected_line[selected_tab] = line;
	//	//selected_cnv = line; // keep these the same in overview
	//}
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

void vehicle_manager_t::build_vehicle_lists()
{
	vehicle_descs.clear();
	if (veh_im.empty())
	{
		/*
		* The next block calculates upper bounds for the sizes of the vectors.
		* If the vectors get resized, the vehicle_map becomes invalid, therefore
		* we need to resize them before filling them.
		*/
		if (veh_im.empty())
		{
			int loks = 0, waggons = 0, pax = 0, electrics = 0;
			way_type = waytype_t::road_wt;
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type))
			{
				if (info->get_engine_type() == vehicle_desc_t::electric && (info->get_freight_type() == goods_manager_t::passengers || info->get_freight_type() == goods_manager_t::mail))
				{
					electrics++;
				}
				else if (info->get_freight_type() == goods_manager_t::passengers || info->get_freight_type() == goods_manager_t::mail)
				{
					pax++;
				}
				else if (info->get_power() > 0 || info->get_total_capacity() == 0)
				{
					loks++;
				}
				else
				{
					waggons++;
				}
				vehicle_descs.append(info);
			}
			veh_im.resize(pax + electrics + loks + waggons);
			vehicle_descs.resize(pax + electrics + loks + waggons);
		}
	}
	veh_im.clear();

	


	{
		int i, icnv = 0;
		icnv = vehicle_descs.get_count();
		// display all vehicle models
		cont_veh_desc.remove_all();

		int ypos = 0;
		for (i = 0; i < icnv; i++) {
			gui_vehicle_desc_info_t* const cinfo = new gui_vehicle_desc_info_t(vehicle_descs.get_element(i));
			cinfo->set_pos(scr_coord(0, ypos));
			cinfo->set_size(scr_size(400, 40));
			vehicle_desc_infos.append(cinfo);
			cont_veh_desc.add_component(cinfo);
			ypos += 40;
		}
		cont_veh_desc.set_size(scr_size(500, ypos));
	}

	{
		int i, icnv = 0;
		icnv = vehicles.get_count();
		// display all vehicle models
		cont_veh.remove_all();

		vehicles.resize(icnv);
		int ypos = 0;
		for (i = 0; i < icnv; i++) {
			gui_vehicle_info_t* const cinfo = new gui_vehicle_info_t(vehicles.get_element(i));
			cinfo->set_pos(scr_coord(0, ypos));
			cinfo->set_size(scr_size(400, 40));
			vehicle_infos.append(cinfo);
			cont_veh.add_component(cinfo);
			ypos += 40;
		}
		cont_veh.set_size(scr_size(500, ypos));
	}
}

uint32 vehicle_manager_t::get_rdwr_id()
{
	return magic_vehicle_manager_t+player->get_player_nr();
}


// Here we model up each entries in the lists:
// We start with the vehicle_desc entries, ie vehicle models:

gui_vehicle_desc_info_t::gui_vehicle_desc_info_t(vehicle_desc_t* veh)
{
	this->veh = veh;
	player_nr = welt->get_active_player_nr();

	filled_bar.set_pos(scr_coord(2, 33));
	filled_bar.set_size(scr_size(100, 4));
	draw(scr_coord(0,0));
}

/**
* Events werden hiermit an die GUI-components
* gemeldet
* @author Hj. Malthaner
*/
bool gui_vehicle_desc_info_t::infowin_event(const event_t *ev)
{

		if (IS_LEFTRELEASE(ev)) {
			return true;
		}
		else if (IS_RIGHTRELEASE(ev)) {
			return true;
		}

	return false;
}


/**
* Draw the component
* @author Hj. Malthaner
*/
void gui_vehicle_desc_info_t::draw(scr_coord offset)
{
	clip_dimension clip = display_get_clip_wh();
	if (!((pos.y + offset.y) > clip.yy || (pos.y + offset.y) < clip.y - 32)) {
		// name, use the convoi status color for redraw: Possible colors are YELLOW (not moving) BLUE: obsolete in convoi, RED: minus income, BLACK: ok
		int max_x = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6, veh->get_name(), ALIGN_LEFT, COL_BLACK, true) + 2;

		// we will use their images offsets and width to shift them to their correct position
		// this should work with any vehicle size ...
		const int xoff = max(190, max_x);
		int left = pos.x + offset.x + xoff + 4;
		scr_coord_val x, y, w, h;
		const image_id image = veh->get_base_image();
		//display_get_base_image_offset(image, &x, &y, w, h);
		display_base_img(image, left - x, pos.y + offset.y + 13 - y - h / 2, player_nr, false, true);
		left += (w * 2) / 3;

		// since the only remaining object is the loading bar, we can alter its position this way ...
		filled_bar.draw(pos + offset + scr_coord(xoff, 0));
	}
}




// Actual vehicles:


gui_vehicle_info_t::gui_vehicle_info_t(vehiclehandle_t veh)
{
	this->veh = veh;

	filled_bar.set_pos(scr_coord(2, 33));
	filled_bar.set_size(scr_size(100, 4));
}

/**
* Events werden hiermit an die GUI-components
* gemeldet
* @author Hj. Malthaner
*/
bool gui_vehicle_info_t::infowin_event(const event_t *ev)
{
	if (veh.is_bound()) {
		if (IS_LEFTRELEASE(ev)) {
			veh->show_info();
			return true;
		}
		else if (IS_RIGHTRELEASE(ev)) {
			//welt->get_viewport()->change_world_position(veh->get_pos());
			return true;
		}
	}
	return false;
}


/**
* Draw the component
* @author Hj. Malthaner
*/
void gui_vehicle_info_t::draw(scr_coord offset)
{
	clip_dimension clip = display_get_clip_wh();
	if (!((pos.y + offset.y) > clip.yy || (pos.y + offset.y) < clip.y - 32) && veh.is_bound()) {
		// name, use the convoi status color for redraw: Possible colors are YELLOW (not moving) BLUE: obsolete in convoi, RED: minus income, BLACK: ok
		int max_x = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6, veh->get_name(), ALIGN_LEFT, COL_BLACK, true) + 2;
		{
			int w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + LINESPACE, translator::translate("Gewinn"), ALIGN_LEFT, SYSCOL_TEXT, true) + 2;
			char buf[256];

			// only show assigned line, if there is one!
			if (veh->get_convoi()->in_depot()) {
				const char *txt = translator::translate("(in depot)");
				int w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + 2 * LINESPACE, txt, ALIGN_LEFT, SYSCOL_TEXT, true) + 2;
				max_x = max(max_x, w);
			}
			else if (veh->get_convoi()->get_line().is_bound()) {
				int w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + 2 * LINESPACE, translator::translate("Line"), ALIGN_LEFT, SYSCOL_TEXT, true) + 2;
				w += display_proportional_clip(pos.x + offset.x + 2 + w + 5, pos.y + offset.y + 6 + 2 * LINESPACE, veh->get_convoi()->get_line()->get_name(), ALIGN_LEFT, veh->get_convoi()->get_line()->get_state_color(), true);
				max_x = max(max_x, w + 5);
			}
		}
		{
			// we will use their images offsets and width to shift them to their correct position
			// this should work with any vehicle size ...
			const int xoff = max(190, max_x);
			int left = pos.x + offset.x + xoff + 4;
			scr_coord_val x, y, w, h;
			const image_id image = veh->get_loaded_image();
			//display_get_base_image_offset(image, &x, &y, w, h);
			display_base_img(image, left - x, pos.y + offset.y + 13 - y - h / 2, veh->get_owner()->get_player_nr(), false, true);
			left += (w * 2) / 3;

			// since the only remaining object is the loading bar, we can alter its position this way ...
			filled_bar.draw(pos + offset + scr_coord(xoff, 0));
		}
	}
}