/*
 * Copyright (c) 1997 - 2004 Hj. Malthaner
 *
 * Line management
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include <stdio.h>
#include <algorithm>

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

#define VEHICLE_NAME_COLUMN_WIDTH ((D_BUTTON_WIDTH*4)+11+4)
#define VEHICLE_NAME_COLUMN_HEIGHT (250)
#define RIGHT_HAND_COLUMN (D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH + 10)
#define SCL_HEIGHT (15*LINESPACE)

//static linehandle_t selected_line[simline_t::MAX_LINE_TYPE];
static uint16 tabs_to_index[8];
static uint8 max_idx = 0;
static uint8 selected_tab = 0;

const char *vehicle_manager_t::sort_text_desc[SORT_MODES_DESC] =
{
	"name",
	"intro_year",
	"amount"
};
const char *vehicle_manager_t::sort_text[SORT_MODES] =
{
	"name",
	"intro_year",
	"amount"
};

vehicle_manager_t::sort_mode_desc_t vehicle_manager_t::sortby_desc = by_desc_name;
vehicle_manager_t::sort_mode_t vehicle_manager_t::sortby = by_name;

vehicle_manager_t::vehicle_manager_t(player_t *player_) :
	gui_frame_t( translator::translate("vehicle_manager"), player_),
	player(player_),
	lb_amount_of_vehicle_descs(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_amount_of_vehicles(NULL, SYSCOL_TEXT, gui_label_t::left),
	scrolly_vehicle_descs(&cont_veh_desc),
	scrolly_vehicles(&cont_veh)
{

	int y_pos = 5;

	scr_coord dummy_coords(D_MARGIN_LEFT, D_MARGIN_TOP);

	bt_show_only_owned.init(button_t::square_state, translator::translate("show_only_owned_vehicles"), dummy_coords, scr_size(D_BUTTON_WIDTH * 2, D_BUTTON_HEIGHT));
	bt_show_only_owned.add_listener(this);
	bt_show_only_owned.set_tooltip(translator::translate("If_unticked_it_shows_all_available_vehicles"));
	bt_show_only_owned.pressed = true;
	bt_show_only_owned.set_pos(scr_coord(10, y_pos));
	add_component(&bt_show_only_owned);

	y_pos += D_BUTTON_HEIGHT;

	// tab panel
	tabs.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	tabs.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 11 - 4, SCL_HEIGHT));
	max_idx = 0;
	// now add all specific tabs
	if (maglev_t::default_maglev) {
		tabs.add_tab(&dummy, translator::translate("Maglev"), skinverwaltung_t::maglevhaltsymbol, translator::translate("Maglev"));
		tabs_to_index[max_idx++] = waytype_t::maglev_wt;
	}
	if (monorail_t::default_monorail) {
		tabs.add_tab(&dummy, translator::translate("Monorail"), skinverwaltung_t::monorailhaltsymbol, translator::translate("Monorail"));
		tabs_to_index[max_idx++] = waytype_t::monorail_wt;
	}
	if (schiene_t::default_schiene) {
		tabs.add_tab(&dummy, translator::translate("Train"), skinverwaltung_t::zughaltsymbol, translator::translate("Train"));
		tabs_to_index[max_idx++] = waytype_t::track_wt;
	}
	if (narrowgauge_t::default_narrowgauge) {
		tabs.add_tab(&dummy, translator::translate("Narrowgauge"), skinverwaltung_t::narrowgaugehaltsymbol, translator::translate("Narrowgauge"));
		tabs_to_index[max_idx++] = waytype_t::narrowgauge_wt;
	}
	if (!vehicle_builder_t::get_info(tram_wt).empty()) {
		tabs.add_tab(&dummy, translator::translate("Tram"), skinverwaltung_t::tramhaltsymbol, translator::translate("Tram"));
		tabs_to_index[max_idx++] = waytype_t::tram_wt;
	}
	if (strasse_t::default_strasse) {
		tabs.add_tab(&dummy, translator::translate("Truck"), skinverwaltung_t::autohaltsymbol, translator::translate("Truck"));
		tabs_to_index[max_idx++] = waytype_t::road_wt;
	}
	if (!vehicle_builder_t::get_info(water_wt).empty()) {
tabs.add_tab(&dummy, translator::translate("Ship"), skinverwaltung_t::schiffshaltsymbol, translator::translate("Ship"));
tabs_to_index[max_idx++] = waytype_t::water_wt;
	}
	if (runway_t::default_runway) {
		tabs.add_tab(&dummy, translator::translate("Air"), skinverwaltung_t::airhaltsymbol, translator::translate("Air"));
		tabs_to_index[max_idx++] = waytype_t::air_wt;
	}
	selected_tab = tabs_to_index[tabs.get_active_tab_index()];
	tabs.add_listener(this);
	add_component(&tabs);

	y_pos += D_BUTTON_HEIGHT + 6;

	combo_sorter_desc.clear_elements();
	for (int i = 0; i < SORT_MODES_DESC; i++)
	{
		combo_sorter_desc.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(sort_text_desc[i]), SYSCOL_TEXT));
	}
	combo_sorter_desc.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	combo_sorter_desc.set_size(scr_size(D_BUTTON_WIDTH * 2, D_BUTTON_HEIGHT));
	combo_sorter_desc.set_focusable(true);
	combo_sorter_desc.set_selection(sortby_desc);
	combo_sorter_desc.set_highlight_color(1);
	combo_sorter_desc.set_max_size(scr_size(D_BUTTON_WIDTH * 2, LINESPACE * 5 + 2 + 16));
	combo_sorter_desc.add_listener(this);
	add_component(&combo_sorter_desc);

	y_pos += D_BUTTON_HEIGHT * 2;

	// Vehicle desc list
	cont_veh_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	scrolly_vehicle_descs.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	scrolly_vehicle_descs.set_show_scroll_x(true);
	scrolly_vehicle_descs.set_scroll_amount_y(40);
	scrolly_vehicle_descs.set_visible(true);
	scrolly_vehicle_descs.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	add_component(&scrolly_vehicle_descs);

	// Vehicle list
	cont_veh.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	//scrolly_vehicles.set_pos(scr_coord(VEHICLE_NAME_COLUMN_WIDTH + 20, y_pos));
	scrolly_vehicles.set_pos(scr_coord(RIGHT_HAND_COLUMN, y_pos));
	scrolly_vehicles.set_show_scroll_x(true);
	scrolly_vehicles.set_scroll_amount_y(40);
	scrolly_vehicles.set_visible(true);
	scrolly_vehicles.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	add_component(&scrolly_vehicles);

	y_pos += VEHICLE_NAME_COLUMN_HEIGHT;

	// Label telling the amount of vehicles
	lb_amount_of_vehicle_descs.set_pos(scr_coord(10, y_pos));
	add_component(&lb_amount_of_vehicle_descs);

	lb_amount_of_vehicles.set_pos(scr_coord(RIGHT_HAND_COLUMN, y_pos));
	add_component(&lb_amount_of_vehicles);

	y_pos += D_BUTTON_HEIGHT;

	selected_index = 0;

	build_vehicle_list();
	build_desc_list();

	// Sizes:
	const int min_width = 500;
	const int min_height = VEHICLE_NAME_COLUMN_HEIGHT + 50;

	set_min_windowsize(scr_size(min_width, min_height));
	set_windowsize(scr_size(min_width, min_height));
	set_resizemode(diagonal_resize);
	//resize(scr_coord(0, 0));
	resize(scr_coord(min_width, min_height)); // suitable for 4 buttons horizontally and 4 convoys vertically

}


vehicle_manager_t::~vehicle_manager_t()
{
}



/**
 * Mouse clicks are hereby reported to the GUI-Components
 */
bool vehicle_manager_t::infowin_event(const event_t *ev)
{
	if (IS_LEFTRELEASE(ev)) {

		//return true;
	}
	else if (IS_RIGHTRELEASE(ev)) {
		//return true;
	}
	return gui_frame_t::infowin_event(ev);
}


bool vehicle_manager_t::action_triggered( gui_action_creator_t *comp, value_t v )           // 28-Dec-01    Markus Weber    Added
{
	if (comp == &tabs) {
		int const tab = tabs.get_active_tab_index();
		uint8 old_selected_tab = selected_tab;
		selected_tab = tabs_to_index[tab];
		build_desc_list();
		build_vehicle_list();
	}
	if (comp == &bt_show_only_owned)
	{
		bt_show_only_owned.pressed = !bt_show_only_owned.pressed;
		show_only_owned = bt_show_only_owned.pressed;
		build_desc_list();
	}

	// sort by what
	if (comp == &combo_sorter_desc) {
		// sort by what
		sint32 sort_mode = combo_sorter_desc.get_selection();
		if (sort_mode < 0)
		{
			combo_sorter_desc.set_selection(0);
			sort_mode = 0;
		}
		sortby_desc = (sort_mode_desc_t)sort_mode;
		build_desc_list();
	}
	return true;
}



void vehicle_manager_t::draw(scr_coord pos, scr_size size)
{
	gui_frame_t::draw(pos, size);

	int old_selected_index =-1;

	if (old_selected_index != selected_index)
	{
		//build_vehicle_list();
		old_selected_index = selected_index;
	}
}


void vehicle_manager_t::display(scr_coord pos)
{
	static cbuffer_t buf_vehicle_descs;
	buf_vehicle_descs.clear();
	buf_vehicle_descs.printf("amount_of_vehicle_descs: %u", amount_of_vehicle_descs);
	lb_amount_of_vehicle_descs.set_text_pointer(buf_vehicle_descs);

	static cbuffer_t buf_vehicle;
	buf_vehicle.clear();
	buf_vehicle.printf("amount_of_vehicles: %u", amount_of_vehicles);
	lb_amount_of_vehicles.set_text_pointer(buf_vehicle);


}


void vehicle_manager_t::set_windowsize(scr_size size)
{
	gui_frame_t::set_windowsize(size);

}

void vehicle_manager_t::vehicle_on_display()
{
	for (int i = 0; i < vehicle_desc_infos.get_count(); i++)
	{
		if (vehicle_desc_infos.get_element(i)->is_selected())
		{
			if (selected_index != i)
			{
				vehicle_desc_infos.get_element(selected_index)->set_selection(false);
				vehicle_for_display = vehicle_descs.get_element(i);
				selected_index = i;
			}
		}
	}

	// Build the list of the vehicles we have actually selected
	for (int i = 0; i < vehicle_infos.get_count(); i++)
	{
		if (vehicle_infos.get_element(i)->is_selected())
		{
			selected_vehicles.append(vehicles.get_element(i));
		}
	}
}


void vehicle_manager_t::sort_vehicles(bool it_is_descs)
{
	if (it_is_descs)
	{
		if (sortby_desc == by_desc_name || sortby_desc == by_desc_intro_year)
		{
			std::sort(vehicle_descs.begin(), vehicle_descs.end(), compare_vehicle_desc);
		}

		// How many of each vehicles do we own?
		amount_of_vehicles = vehicles.get_count();

		uint16 *owned_vehicles = new uint16[amount_of_vehicle_descs];
		for (int i = 0; i < amount_of_vehicle_descs; i++)
		{
			owned_vehicles[i] = 0;
		}
		for (int i = 0; i < amount_of_vehicle_descs; i++)
		{
			for (int j = 0; j < amount_of_vehicles; j++)
			{
				if (vehicle_descs.get_element(i) == vehicles.get_element(j)->get_desc())
				{
					owned_vehicles[i]++;
				}
			}
		}


		if (sortby_desc == by_desc_amount)
		{
			vehicle_descs_pr_name.resize(amount_of_vehicle_descs);
			vehicle_descs_pr_name.clear();
			for (int i = 0; i < amount_of_vehicle_descs; i++)
			{
				vehicle_descs_pr_name.append(vehicle_descs.get_element(i));
				char *tmp_name = new char[512]();
				if (tmp_name != nullptr)
				{
					sprintf(tmp_name, "%u.%u.", owned_vehicles[i], i);
					name_with_amount[i] = tmp_name;
				}
			}
			std::sort(name_with_amount, name_with_amount + amount_of_vehicle_descs, compare_vehicle_desc_amount);

			vehicle_descs.clear();
			char c[1] = { 'a' }; // Need to give c some value, since the forloop depends on it
			char accumulated_amount[10] = { 0 };
			char accumulated_index[10] = { 0 };
			int info_section;
			int char_offset;

			for (int i = 0; i < amount_of_vehicle_descs; i++)
			{
				char_offset = 0;
				info_section = 0;
				c[0] = 'a';
				for (int j = 0; *c != '\0'; j++)
				{
					*c = name_with_amount[i][j];
					if (*c == '.') // Index change
					{
						info_section++;
						if (info_section == 1) // First section done: The amount of this vehicle, not interrested....
						{
							char_offset = j + 1;
						}
						else if (info_section == 2) // Second section done: Bingo, the previous index number
						{
							break;
						}
					}
					else
					{
						if (info_section == 0)
						{
							accumulated_amount[j] = *c;
						}
						else if (info_section == 1)
						{
							accumulated_index[j - char_offset] = *c;
						}
					}
				}
				uint16 veh_amount = std::atoi(accumulated_amount);
				uint32 veh_index = std::atoi(accumulated_index);


				for (int j = 0; j < amount_of_vehicle_descs; j++)
				{
					if (j == veh_index)
					{
						vehicle_desc_t * info = vehicle_descs_pr_name.get_element(j);
						vehicle_descs.append(info);
						owned_vehicles[j] = veh_amount;
					}
				}
			}
		}
	}
}


void vehicle_manager_t::build_desc_list()
{
	int counter = 0;
	way_type = (waytype_t)selected_tab;
	vehicle_descs.clear();

	// We dont want to do the calculations twice, so take the maximum possible amount of desc's
	FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type))
	{
		counter++;
	}
	vehicle_descs.resize(counter);
	uint16 *owned_vehicles = new uint16[counter]();
	counter = 0;

	if (!show_only_owned)
	{
		FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type))
		{
			vehicle_descs.append(info);
		}
	}
	else
	{
		// We need to know how many vehicles we own, as that will be used to fill the descs later
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
				for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
				{
					vehicle_t* v = cnv->get_vehicle(veh);
					{
						counter++;
					}
				}
			}
		}
		vehicle_descs.resize(counter);
		// Fill the vector with the desc's we own
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
				for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
				{
					vehicle_t* v = cnv->get_vehicle(veh);
					vehicle_descs.append((vehicle_desc_t*)v->get_desc());
					for (int i = 0; i < vehicle_descs.get_count()-1; i++)
					{
						if (vehicle_descs.get_element(i) == v->get_desc())
						{
							owned_vehicles[i]++;
							vehicle_descs.remove_at(i);
						}
					}
				}
			}
		}
	}


	amount_of_vehicle_descs = vehicle_descs.get_count();

	//sort_vehicles(true);

	{
		int i, icnv = 0;
		icnv = vehicle_descs.get_count();
		cont_veh_desc.remove_all();
		if (icnv > 500)
		{
			icnv = 500;
		}
		int ypos = 10;
		for (i = 0; i < icnv; i++) {

			gui_vehicle_desc_info_t* const cinfo = new gui_vehicle_desc_info_t(vehicle_descs.get_element(i), owned_vehicles[i]);
			cinfo->set_pos(scr_coord(0, ypos));
			cinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, max(cinfo->image_height, 40)));
			vehicle_desc_infos.append(cinfo);
			cont_veh_desc.add_component(cinfo);
			ypos += max(cinfo->image_height, 40);

		}
		cont_veh_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, ypos));
	}
	build_vehicle_list();
}

void vehicle_manager_t::build_vehicle_list()
{
	int counter = 0;
	way_type = (waytype_t)selected_tab;
	vehicles.clear();
	vehicle_on_display();
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
			{
				vehicle_t* v = cnv->get_vehicle(veh);
				if (v->get_desc() == vehicle_for_display)
				{
					counter++;
				}
			}
		}
	}
	vehicles.resize(counter);
	
	// Fill the vectors with the vehicles we own
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
			{
				vehicle_t* v = cnv->get_vehicle(veh);
				if (v->get_desc() == vehicle_for_display)
				{
					vehicles.append(v);
				}
			}
		}
	}

	//sort_vehicles(false);

	{
		// display all individual vehicles
		int i, icnv = 0;
		icnv = vehicles.get_count();
		cont_veh.remove_all();

		vehicles.resize(icnv);
		int ypos = 0;
		for (i = 0; i < icnv; i++) {
			if (vehicles.get_element(i) != NULL)
			{
				gui_vehicle_info_t* const cinfo = new gui_vehicle_info_t(vehicles.get_element(i));
				cinfo->set_pos(scr_coord(0, ypos));
				cinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH-12, max(cinfo->image_height, 40)));
				vehicle_infos.append(cinfo);
				cont_veh.add_component(cinfo);
				ypos += max(cinfo->image_height, 40);
			}
		}
		cont_veh.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH-12, ypos));
	}
	display(scr_coord(0, 0));
}

// Sorting, for the different types of vehicles
bool vehicle_manager_t::compare_vehicle_desc(vehicle_desc_t* veh1, vehicle_desc_t* veh2)
{
	sint32 result = 0;

	switch (sortby_desc) {
	default:
	case by_desc_name:
		result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
		break;
	case by_desc_intro_year:
		result = sgn(veh1->get_intro_year_month() / 12 - veh2->get_intro_year_month() / 12);
		break;
	//case by_desc_issues:
	//	result = cnv1.get_id() - cnv2.get_id();
	//	break;
	}
	return result < 0;
}


bool vehicle_manager_t::compare_vehicle_desc_amount(char* veh1, char* veh2)
{
	sint32 result = 0;

	switch (sortby_desc) {
	default:
	case by_desc_amount:
		char c[1] = { 0 };
		char accumulated_char[10] = { 0 };
		for (int i = 0; *c != '.'; i++)
		{
			*c = veh1[i];
			if (*c == '.')
			{
				break;
			}
			else
			{
				accumulated_char[i] = *c;
			}
		}
		int veh1_amount = std::atoi(accumulated_char);

		c[0] = '\0';
		*accumulated_char = { 0 };
		for (int i = 0; *c != '.'; i++)
		{
			*c = veh2[i];
			if (*c == '.')
			{
				break;
			}
			else
			{
				accumulated_char[i] = *c;
			}
		}
		int veh2_amount = std::atoi(accumulated_char);

		result = veh1_amount - veh2_amount;
		return result > 0;
		break;
	}




	return false;
}

uint32 vehicle_manager_t::get_rdwr_id()
{
	return magic_vehicle_manager_t+player->get_player_nr();
}

void vehicle_manager_t::display_this_vehicle(vehicle_desc_t* veh)
{
	//vehicle_desc_t show_vehicle = *veh;
	//return show_vehicle;
}


// Here we model up each entries in the lists:
// We start with the vehicle_desc entries, ie vehicle models:

gui_vehicle_desc_info_t::gui_vehicle_desc_info_t(vehicle_desc_t* veh, uint16 vehicleamount)
{
	this->veh = veh;
	amount = vehicleamount;
	player_nr = welt->get_active_player_nr();

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
			selected = !selected;
			//vehicle_manager_t vema;
			//vema.display_this_vehicle(veh);
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
		// Name colors:
		// Black:		ok!
		// Dark blue:	obsolete
		// Pink:		Can upgrade

		int max_caracters = 30;
		int look_for_spaces_or_separators = 10;
		char name[256] = "\0"; // Translated name of the vehicle. Will not be modified
		char name_display[256] = "\0"; // The string that will be sent to the screen
		int ypos_name = 0;
		int suitable_break;
		int used_caracters = 0;
		COLOR_VAL text_color = COL_BLACK;
		sprintf(name, translator::translate(veh->get_name()));
		sprintf(name_display, name);

		scr_coord_val x, y, w, h;
		const image_id image = veh->get_base_image();
		display_get_base_image_offset(image, &x, &y, &w, &h);
		if (selected)
		{
			display_fillbox_wh_clip(offset.x + pos.x, offset.y + pos.y + 1, VEHICLE_NAME_COLUMN_WIDTH, max(h, 40) - 2, COL_DARK_BLUE, MN_GREY4, true);
			text_color = COL_WHITE;
		}

		// In order to show the name, but to prevent suuuuper long translations to ruin the layout, this will divide the name into several lines if necesary
		if (name[max_caracters] != '\0')
		{
			// Ok, our name is too long to be displayed in one line. Time to chup it up
			for (int i = 1; i <= 3; i++)
			{
				suitable_break = max_caracters;
				if (name_display[max_caracters] == '\0')
				{// Finally last line of name
					display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, name_display, ALIGN_LEFT, text_color, true);
					ypos_name += LINESPACE;
					break;
				}
				else
				{
					bool natural_separator = false;
					for (int j = 0; j < look_for_spaces_or_separators; j++)
					{	// Move down to second line
						if (name_display[max_caracters - j] == '(' || name_display[max_caracters - j] == '{' || name_display[max_caracters - j] == '[')
						{
							suitable_break = max_caracters - j;
							used_caracters += suitable_break;
							name_display[suitable_break] = '\0';
							natural_separator = true;
							break;
						}
						// Stay on upper line
						else if (name_display[max_caracters - j] == ' ' || name_display[max_caracters - j] == '-' || name_display[max_caracters - j] == '/' ||/*name_display[max_caracters - j] == '.' || */ name_display[max_caracters - j] == ',' || name_display[max_caracters - j] == ';' || name_display[max_caracters - j] == ')' || name_display[max_caracters - j] == '}' || name_display[max_caracters - j] == ']')
						{
							suitable_break = max_caracters - j + 1;
							used_caracters += suitable_break;
							name_display[suitable_break] = '\0';
							natural_separator = true;
							break;
						}
					}
					// No suitable breakpoint, divide line with "-"
					if (!natural_separator)
					{
						suitable_break = max_caracters;
						used_caracters += suitable_break;
						name_display[suitable_break] = '-';
						name_display[suitable_break + 1] = '\0';
					}
					display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, name_display, ALIGN_LEFT, text_color, true);

					// Reset the string
					name_display[0] = '\0';
					for (int j = 0; j < 256; j++)
					{
						name_display[j] = name[used_caracters + j];
					}
				}
				ypos_name += LINESPACE;
			}		
		}
		else
		{ // Ok, name is short enough to fit one line
			display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, name, ALIGN_LEFT, text_color, true);
		}

		ypos_name = 0;
		const int xpos_extra = VEHICLE_NAME_COLUMN_WIDTH - D_BUTTON_WIDTH - 10;

		// Byggår
		char year[10];
		sprintf(year, "%s: %u", translator::translate("since"), veh->get_intro_year_month() / 12);
		display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, year, ALIGN_RIGHT, text_color, true);
		ypos_name += LINESPACE;

		// Antal
		char amount_of_vehicles[10];
		sprintf(amount_of_vehicles, "%s: %u", translator::translate("amount"), amount);
		display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, amount_of_vehicles, ALIGN_RIGHT, text_color, true);
		ypos_name += LINESPACE;
		
		// Messages


		const int xoff = VEHICLE_NAME_COLUMN_WIDTH - D_BUTTON_WIDTH;
		int left = pos.x + offset.x + xoff + 4;
		display_base_img(image, left - x, pos.y + offset.y + 13 - y - h / 2, player_nr, false, true);

		//// Antal, displayed on top of the image
		//sprintf(amount_of_vehicles, "%u", amount);
		//display_proportional_clip(left, pos.y + offset.y + 13, amount_of_vehicles, ALIGN_RIGHT, SYSCOL_TEXT_HIGHLIGHT, true);
		//ypos_name += LINESPACE;

		image_height = h;

	}
}




// Actual vehicles:


gui_vehicle_info_t::gui_vehicle_info_t(vehicle_t* veh)
{
	this->veh = veh;
}

/**
* Events werden hiermit an die GUI-components
* gemeldet
* @author Hj. Malthaner
*/
bool gui_vehicle_info_t::infowin_event(const event_t *ev)
{
	
		if (IS_LEFTRELEASE(ev)) {
			//veh->show_info(); // Perhaps this option should be possible too?
			selected = !selected;
			return true;
		}
		else if (IS_RIGHTRELEASE(ev)) {
			//welt->get_viewport()->change_world_position(veh->get_pos());
			return true;
	
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
	if (!((pos.y + offset.y) > clip.yy || (pos.y + offset.y) < clip.y - 32) /*&& veh.is_bound()*/) {

		COLOR_VAL text_color = COL_BLACK;
		scr_coord_val x, y, w, h;
		const image_id image = veh->get_loaded_image();
		display_get_base_image_offset(image, &x, &y, &w, &h);
		if (selected)
		{
			display_fillbox_wh_clip(offset.x + pos.x, offset.y + pos.y + 1, VEHICLE_NAME_COLUMN_WIDTH, max(h, 40) - 2, COL_DARK_BLUE, MN_GREY4, true);
			text_color = COL_WHITE;
		}
		// name, use the convoi status color for redraw: Possible colors are YELLOW (not moving) BLUE: obsolete in convoi, RED: minus income, BLACK: ok
		int max_x = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6, veh->get_name(), ALIGN_LEFT, text_color, true) + 2;
		{
			int w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + LINESPACE, translator::translate("Gewinn"), ALIGN_LEFT, text_color, true) + 2;
			char buf[256];

			// only show assigned line, if there is one!
			if (veh->get_convoi()->in_depot()) {
				const char *txt = translator::translate("(in depot)");
				int w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + 2 * LINESPACE, txt, ALIGN_LEFT, text_color, true) + 2;
				max_x = max(max_x, w);
			}
			else if (veh->get_convoi()->get_line().is_bound()) {
				int w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + 2 * LINESPACE, translator::translate("Line"), ALIGN_LEFT, text_color, true) + 2;
				w += display_proportional_clip(pos.x + offset.x + 2 + w + 5, pos.y + offset.y + 6 + 2 * LINESPACE, veh->get_convoi()->get_line()->get_name(), ALIGN_LEFT, veh->get_convoi()->get_line()->get_state_color(), true);
				max_x = max(max_x, w + 5);
			}

			const int xoff = VEHICLE_NAME_COLUMN_WIDTH - D_BUTTON_WIDTH;
			int left = pos.x + offset.x + xoff + 4;
			display_base_img(image, left - x, pos.y + offset.y + 13 - y - h / 2, veh->get_owner()->get_player_nr(), false, true);

		}
		image_height = h;
	}
}