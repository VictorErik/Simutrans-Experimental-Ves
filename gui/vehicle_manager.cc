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
#include "../simcity.h"
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
#include "../display/viewport.h"
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
#define INFORMATION_COLUMN_HEIGHT (400)
#define RIGHT_HAND_COLUMN (D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH + 10)
#define SCL_HEIGHT (15*LINESPACE)

//static linehandle_t selected_line[simline_t::MAX_LINE_TYPE];
static uint16 tabs_to_index_waytype[8];
static uint16 tabs_to_index_information[8];
static uint8 max_idx = 0;
static uint8 selected_tab_waytype = 0;
static uint8 selected_tab_information = 0;

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

	bt_show_available_vehicles.init(button_t::square_state, translator::translate("show_available_vehicles"), scr_coord(D_MARGIN_LEFT, y_pos), scr_size(D_BUTTON_WIDTH * 2, D_BUTTON_HEIGHT));
	bt_show_available_vehicles.add_listener(this);
	bt_show_available_vehicles.set_tooltip(translator::translate("tick_to_show_all_available_vehicles"));
	bt_show_available_vehicles.pressed = false;
	show_available_vehicles = bt_show_available_vehicles.pressed;
	//bt_show_only_owned.set_pos(scr_coord(10, y_pos));
	add_component(&bt_show_available_vehicles);

	y_pos += D_BUTTON_HEIGHT;

	// waytype tab panel
	tabs_waytype.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	update_tabs();
	tabs_waytype.add_listener(this);
	add_component(&tabs_waytype);

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

	bt_select_all.init(button_t::square_state, translator::translate("select_all"), scr_coord(RIGHT_HAND_COLUMN, y_pos), scr_size(D_BUTTON_WIDTH * 2, D_BUTTON_HEIGHT));
	bt_select_all.add_listener(this);
	bt_select_all.set_tooltip(translator::translate("select_all_vehicles_in_the_list"));
	bt_select_all.pressed = false;
	select_all = bt_select_all.pressed;
	//bt_select_all.set_pos(scr_coord(10, y_pos));
	add_component(&bt_select_all);

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

	y_pos += VEHICLE_NAME_COLUMN_HEIGHT+ D_BUTTON_HEIGHT;

	// Label telling the amount of vehicles
	lb_amount_of_vehicle_descs.set_pos(scr_coord(10, y_pos));
	add_component(&lb_amount_of_vehicle_descs);

	lb_amount_of_vehicles.set_pos(scr_coord(RIGHT_HAND_COLUMN, y_pos));
	add_component(&lb_amount_of_vehicles);
	

	y_pos += D_BUTTON_HEIGHT;

	// information tab panel
	tabs_info.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	tabs_info.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH*2, SCL_HEIGHT));
	
	// general information
	tabs_info.add_tab(&dummy, translator::translate("general_information"));
	tabs_to_index_information[max_idx++] = 0;

	// upgrades
	tabs_info.add_tab(&dummy, translator::translate("upgrades"));
	tabs_to_index_information[max_idx++] = 0;
	
	// details
	tabs_info.add_tab(&dummy, translator::translate("details"));
	tabs_to_index_information[max_idx++] = 0;

	selected_tab_information = tabs_to_index_information[tabs_info.get_active_tab_index()];
	tabs_info.add_listener(this);
	add_component(&tabs_info);


	selected_index_desc = -1;

	//build_vehicle_list();
	build_desc_list();

	// Sizes:
	const int min_width = (VEHICLE_NAME_COLUMN_WIDTH * 2) + (D_MARGIN_LEFT*3);
	const int min_height = VEHICLE_NAME_COLUMN_HEIGHT + INFORMATION_COLUMN_HEIGHT;

	set_min_windowsize(scr_size(min_width, min_height));
	set_windowsize(scr_size(min_width, min_height));
	set_resizemode(diagonal_resize);
	resize(scr_coord(0, 0));

}


vehicle_manager_t::~vehicle_manager_t()
{	
	for (int i = 0; i <vehicle_desc_infos.get_count(); i++)
	{
		delete[] vehicle_desc_infos.get_element(i);
	}
	for (int i = 0; i <vehicle_infos.get_count(); i++)
	{
		delete[] vehicle_infos.get_element(i);
	}
}

bool vehicle_manager_t::action_triggered(gui_action_creator_t *comp, value_t v)           // 28-Dec-01    Markus Weber    Added
{
	// waytype tabs
	if (comp == &tabs_waytype) {
		int const tab = tabs_waytype.get_active_tab_index();
		uint8 old_selected_tab = selected_tab_waytype;
		selected_tab_waytype = tabs_to_index_waytype[tab];
		build_desc_list();
	}

	if (comp == &tabs_info) {
		int const tab = tabs_info.get_active_tab_index();
		uint8 old_selected_tab = selected_tab_information;
		selected_tab_information = tabs_to_index_information[tab];
	}
	

	if (comp == &bt_show_available_vehicles)
	{
		bt_show_available_vehicles.pressed = !bt_show_available_vehicles.pressed;
		show_available_vehicles = bt_show_available_vehicles.pressed;
		update_tabs();
		build_desc_list();
	}

	if (comp == &bt_select_all)
	{
		bt_select_all.pressed = !bt_select_all.pressed;
		select_all = bt_select_all.pressed;
		for (int i = 0; i < vehicle_infos.get_count(); i++)
		{
			vehicle_infos.get_element(i)->set_selection(select_all);
		}
		build_vehicle_info();
	}

	// sort by what
	if (comp == &combo_sorter_desc) {
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


	// details window

	// upgrade window

	// general information window



	bool update_vehicle_list = false;
	bool update_vehicle_desc_list = false;

	// This handles the selection of the vehicles in the "desc" section
	for (int i = 0; i < vehicle_desc_infos.get_count(); i++)
	{
		if (vehicle_desc_infos.get_element(i)->is_selected())
		{
			if (selected_index_desc != i)
			{
				if (i < vehicle_descs.get_count())
				{
					vehicle_for_display = vehicle_descs.get_element(i);
					selected_index_desc = i;

					for (int j = 0; j < vehicle_desc_infos.get_count(); j++)
					{
						if (j != selected_index_desc)
						{
							vehicle_desc_infos.get_element(j)->set_selection(false);
						}
					}
					update_vehicle_list = true;
					break;
				}
			}
		}
	}

	// This updates the vehicle info list
	updated_amount_selected_index_vehicle = 0;
	for (int i = 0; i < vehicle_infos.get_count(); i++)
	{
		if (vehicle_infos.get_element(i)->is_selected())
		{
			updated_amount_selected_index_vehicle++;
		}
	}
	if (old_amount_selected_index_vehicle != updated_amount_selected_index_vehicle)
	{
		old_amount_selected_index_vehicle = updated_amount_selected_index_vehicle;
		build_vehicle_info();
	}



	updated_amount_owned_vehicles = 0;
	way_type = (waytype_t)selected_tab_waytype;
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
			{
				updated_amount_owned_vehicles++;
			}
		}
	}
	if (old_amount_of_owned_vehicles != updated_amount_owned_vehicles)
	{
		old_amount_of_owned_vehicles = updated_amount_owned_vehicles;
		build_desc_list();
	}

	if (update_vehicle_list)
	{
		build_vehicle_list();
	}
}


void vehicle_manager_t::display(scr_coord pos)
{
	static cbuffer_t buf_vehicle_descs;
	buf_vehicle_descs.clear();
	buf_vehicle_descs.printf(translator::translate("amount_of_vehicle_descs: %u"), amount_of_vehicle_descs);
	lb_amount_of_vehicle_descs.set_text_pointer(buf_vehicle_descs);

	static cbuffer_t buf_vehicle;
	buf_vehicle.clear();
	buf_vehicle.printf(translator::translate("amount_of_vehicles: %u (%u selected)"), amount_of_vehicles, selected_vehicles.get_count());
	lb_amount_of_vehicles.set_text_pointer(buf_vehicle);


}


void vehicle_manager_t::set_windowsize(scr_size size)
{
	gui_frame_t::set_windowsize(size);

}

void vehicle_manager_t::update_tabs()
{
	// tab panel
	tabs_waytype.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 11 - 4, SCL_HEIGHT));
	max_idx = 0;

	tabs_waytype.clear();
	if (maglev_t::default_maglev) {
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::maglev_wt) || show_available_vehicles) {
				tabs_waytype.add_tab(&dummy, translator::translate("Maglev"), skinverwaltung_t::maglevhaltsymbol, translator::translate("Maglev"));
				tabs_to_index_waytype[max_idx++] = waytype_t::maglev_wt;
				break;
			}
		}
	}
	if (monorail_t::default_monorail) {
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::monorail_wt) || show_available_vehicles) {
				tabs_waytype.add_tab(&dummy, translator::translate("Monorail"), skinverwaltung_t::monorailhaltsymbol, translator::translate("Monorail"));
				tabs_to_index_waytype[max_idx++] = waytype_t::monorail_wt;
				break;
			}
		}
	}
	if (schiene_t::default_schiene) {
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::track_wt) || show_available_vehicles) {
				tabs_waytype.add_tab(&dummy, translator::translate("Train"), skinverwaltung_t::zughaltsymbol, translator::translate("Train"));
				tabs_to_index_waytype[max_idx++] = waytype_t::track_wt;
				break;
			}
		}
	}
	if (narrowgauge_t::default_narrowgauge) {
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::narrowgauge_wt) || show_available_vehicles) {
				tabs_waytype.add_tab(&dummy, translator::translate("Narrowgauge"), skinverwaltung_t::narrowgaugehaltsymbol, translator::translate("Narrowgauge"));
				tabs_to_index_waytype[max_idx++] = waytype_t::narrowgauge_wt;
				break;
			}
		}
	}
	if (!vehicle_builder_t::get_info(tram_wt).empty()) {
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::tram_wt) || show_available_vehicles) {
				tabs_waytype.add_tab(&dummy, translator::translate("Tram"), skinverwaltung_t::tramhaltsymbol, translator::translate("Tram"));
				tabs_to_index_waytype[max_idx++] = waytype_t::tram_wt;
				break;
			}
		}
	}
	if (strasse_t::default_strasse) {
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::road_wt) || show_available_vehicles) {
				tabs_waytype.add_tab(&dummy, translator::translate("Truck"), skinverwaltung_t::autohaltsymbol, translator::translate("Truck"));
				tabs_to_index_waytype[max_idx++] = waytype_t::road_wt;
				break;
			}
		}
	}
	if (!vehicle_builder_t::get_info(water_wt).empty()) {
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::water_wt) || show_available_vehicles) {
				tabs_waytype.add_tab(&dummy, translator::translate("Ship"), skinverwaltung_t::schiffshaltsymbol, translator::translate("Ship"));
				tabs_to_index_waytype[max_idx++] = waytype_t::water_wt;
				break;
			}
		}
	}
	if (runway_t::default_runway) {
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::air_wt) || show_available_vehicles) {
				tabs_waytype.add_tab(&dummy, translator::translate("Air"), skinverwaltung_t::airhaltsymbol, translator::translate("Air"));
				tabs_to_index_waytype[max_idx++] = waytype_t::air_wt;
				break;
			}
		}
	}

	if (max_idx <= 0)
	{
		tabs_waytype.add_tab(&dummy, translator::translate("no_vehicles_to_show"));
		tabs_to_index_waytype[max_idx++] = waytype_t::air_wt;
	}
	selected_tab_waytype = tabs_to_index_waytype[tabs_waytype.get_active_tab_index()];
}



void vehicle_manager_t::sort_vehicles(bool veh_desc)
{
	if (veh_desc)
	{
		if (sortby_desc == by_desc_name || sortby_desc == by_desc_intro_year)
		{
			std::sort(vehicle_descs.begin(), vehicle_descs.end(), compare_vehicle_desc);
		}

		//How many of each vehicles do we own?
		if (sortby_desc == by_desc_amount)
		{
			uint16* tmp_amounts;
			tmp_amounts = new uint16[amount_of_vehicle_descs];
			for (int i = 0; i < amount_of_vehicle_descs; i++)
			{
				tmp_amounts[i] = 0;
			}

			for (int i = 0; i < amount_of_vehicle_descs; i++)
			{
				for (unsigned j = 0; j < amount_of_vehicles_we_own; j++)
				{
					if (vehicle_descs.get_element(i) == vehicle_we_own.get_element(j)->get_desc())
					{
						tmp_amounts[i]++;
					}
				}
			}

			vehicle_descs_pr_name.clear();
			vehicle_descs_pr_name.resize(amount_of_vehicle_descs);

			// create a new list to keep track of which index number each desc had originally and match the index number to the value we want to sort by
			char **name_with_amount = new char *[amount_of_vehicle_descs];
			for (int i = 0; i < amount_of_vehicle_descs; i++)
			{
				vehicle_descs_pr_name.append(vehicle_descs.get_element(i));

				name_with_amount[i] = new char[50];
				if (name_with_amount[i] != nullptr)
				{
					sprintf(name_with_amount[i], "%u.%u.",tmp_amounts[i], i);
				}
			}

			std::sort(name_with_amount, name_with_amount + amount_of_vehicle_descs, compare_vehicle_desc_amount);

			// now clear the old list, so we can start build it from scratch again in the new order
			vehicle_descs.clear();
			char c[1] = { 'a' }; // Need to give c some value, since the forloop depends on it
			char accumulated_amount[10] = { 0 };
			char accumulated_index[10] = { 0 };

			int info_section;
			int char_offset;

			for (int i = 0; i < amount_of_vehicle_descs; i++)
			{
				for (int i = 0; i < 10; i++)
				{
					accumulated_amount[i] = '\0';
					accumulated_index[i] = '\0';
				}
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
						else if (info_section == 2) // Second section done: Bingo, the old index number
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
						break;
					}
				}
			}
			delete[] tmp_amounts;
			for (int i = 0; i < amount_of_vehicle_descs; i++)
			{
				if (name_with_amount[i] != nullptr)
				{
					delete[] name_with_amount[i];
				}
			}
		}
	}
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
		char accumulated_char_a[10] = { 0 };
		for (int i = 0; *c != '.'; i++)
		{
			*c = veh1[i];
			if (*c == '.')
			{
				break;
			}
			else
			{
				accumulated_char_a[i] = *c;
			}
		}
		uint16 veh1_amount = std::atoi(accumulated_char_a);

		char d[1] = { 0 };
		char accumulated_char_b[10] = { 0 };
		for (int i = 0; *d != '.'; i++)
		{
			*d = veh2[i];
			if (*d == '.')
			{
				break;
			}
			else
			{
				accumulated_char_b[i] = *d;
			}
		}
		uint16 veh2_amount = std::atoi(accumulated_char_b);

		result = veh1_amount - veh2_amount;
		return result > 0;
		break;
	}
	//return false;
}


void vehicle_manager_t::build_desc_list()
{
	int counter = 0;
	way_type = (waytype_t)selected_tab_waytype;


	vehicle_we_own.clear();
	vehicle_descs.clear();

	// How many vehicles do we own?
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
			{
				counter++;
			}
		}
	}
	vehicle_we_own.resize(counter);
	vehicle_descs.resize(counter);

	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
			{
				vehicle_t* v = cnv->get_vehicle(veh);
				vehicle_we_own.append(v);
			}
		}
	}

	if (!show_available_vehicles)
	{
		// Fill the vector with only the desc's we own
		FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
			if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
				for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
				{
					vehicle_t* v = cnv->get_vehicle(veh);
					vehicle_descs.append((vehicle_desc_t*)v->get_desc());
					for (int i = 0; i < vehicle_descs.get_count() - 1; i++)
					{
						if (vehicle_descs.get_element(i) == v->get_desc())
						{
							vehicle_descs.remove_at(i);
							break;
						}
					}
				}
			}
		}
	}
	else
	{
		counter = 0; // We need to count again, since we now count even vehicles we do not own
		FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type))
		{
			counter++;
		}
		vehicle_descs.resize(counter);

		FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type))
		{
			vehicle_descs.append(info);
		}

	}
	

	amount_of_vehicle_descs = vehicle_descs.get_count();
	amount_of_vehicles_we_own = vehicle_we_own.get_count();
	
	sort_vehicles(true);
	

	// count how many of each "desc" we own
	uint16* desc_amounts;
	desc_amounts = new uint16[vehicle_descs.get_count()];
	for (int i = 0; i <  vehicle_descs.get_count(); i++)
	{
		desc_amounts[i] = 0;
	}

	for (int i = 0; i <  vehicle_descs.get_count(); i++)
	{
		for (int j = 0; j < amount_of_vehicles_we_own; j++)
		{
			if (vehicle_descs.get_element(i) == vehicle_we_own.get_element(j)->get_desc())
			{
				desc_amounts[i]++;
			}
		}
	}

	{
		int i, icnv = 0;
		icnv = vehicle_descs.get_count();
		cont_veh_desc.remove_all();
		vehicle_desc_infos.clear();
		if (icnv > 500)
		{
			icnv = 500;
		}
		int ypos = 10;
		for (i = 0; i < icnv; i++) {

			gui_vehicle_desc_info_t* const cinfo = new gui_vehicle_desc_info_t(vehicle_descs.get_element(i), desc_amounts[i]);
			cinfo->set_pos(scr_coord(0, ypos));
			cinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, max(cinfo->image_height, 40)));
			vehicle_desc_infos.append(cinfo);
			cont_veh_desc.add_component(cinfo);
			ypos += max(cinfo->image_height, 40);

		}
		vehicle_desc_infos.set_count(icnv);
		cont_veh_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, ypos));
	}
	build_vehicle_list();

	// delete the amount of vehicles array
	delete[] desc_amounts;
}

void vehicle_manager_t::build_vehicle_list()
{
	int counter = 0;
	way_type = (waytype_t)selected_tab_waytype;
	vehicle_list.clear();
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
	vehicle_list.resize(counter);
	
	// Fill the vectors with the vehicles we own
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
			{
				vehicle_t* v = cnv->get_vehicle(veh);
				if (v->get_desc() == vehicle_for_display)
				{
					vehicle_list.append(v);
				}
			}
		}
	}

	//amount_of_vehicles = vehicles.get_count();
	amount_of_vehicles = vehicle_list.get_count();

	//sort_vehicles(false);

	{
		// display all individual vehicles
		int i, icnv = 0;
		icnv = vehicle_list.get_count();
		if (icnv > 500)
		{
			icnv = 500;
		}
		cont_veh.remove_all();
		vehicle_infos.clear();

		vehicle_list.resize(icnv);
		int ypos = 0;
		for (i = 0; i < icnv; i++) {
			if (vehicle_list.get_element(i) != NULL)
			{
				gui_vehicle_info_t* const cinfo = new gui_vehicle_info_t(vehicle_list.get_element(i));
				cinfo->set_pos(scr_coord(0, ypos));
				cinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH-12, max(cinfo->image_height, 50)));
				cinfo->set_selection(select_all);
				vehicle_infos.append(cinfo);
				cont_veh.add_component(cinfo);
				ypos += max(cinfo->image_height, 50);
			}
		}
		vehicle_infos.set_count(icnv);
		cont_veh.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH-12, ypos));
		build_vehicle_info();
	}
}


// This builds the actual array of selected vehicles that we will show info about
void vehicle_manager_t::build_vehicle_info()
{
	selected_vehicles.clear();
	selected_vehicles.resize(vehicle_infos.get_count());

	for (int i = 0; i < vehicle_infos.get_count(); i++)
	{
		if (vehicle_infos.get_element(i)->is_selected())
		{
			selected_vehicles.append(vehicle_list.get_element(i));
		}
	}
	display(scr_coord(0, 0));
}



uint32 vehicle_manager_t::get_rdwr_id()
{
	return magic_vehicle_manager_t+player->get_player_nr();
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
		image_height = 0;
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
			welt->get_viewport()->change_world_position(veh->get_pos());
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

		image_height = 0;
		int ypos_name = 0;

		COLOR_VAL text_color = COL_BLACK;
		scr_coord_val x, y, w, h;
		const image_id image = veh->get_loaded_image();
		display_get_base_image_offset(image, &x, &y, &w, &h);
		if (selected)
		{
			display_fillbox_wh_clip(offset.x + pos.x, offset.y + pos.y + 1, VEHICLE_NAME_COLUMN_WIDTH, max(h, 50) - 2, COL_DARK_BLUE, MN_GREY4, true);
			text_color = COL_WHITE;
		}
		// name wont be necessary, since we get the name from the left hand column and also elsewhere.
		// However, some method to ID the vehicle is desirable
		// TODO: ID-tag or similar.

		int max_x = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, translator::translate(veh->get_name()), ALIGN_LEFT, text_color, true) + 2;
		ypos_name += LINESPACE;
		// current speed/What is it doing (loading, waiting for time etcetc...)
		if (veh->get_convoi()->in_depot())
		{
			const char *txt = translator::translate("(in depot)");
			w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, txt, ALIGN_LEFT, text_color, true) + 2;
			max_x = max(max_x, w);
			ypos_name += LINESPACE;
		}
		else
		{
			convoi_t* cnv = veh->get_convoi();
			COLOR_VAL speed_color = text_color;
			char speed_text[256];
			air_vehicle_t* air_vehicle = NULL;
			if (veh->get_waytype() == air_wt)
			{
				air_vehicle = (air_vehicle_t*)veh;
			}
			const air_vehicle_t* air = (const air_vehicle_t*)this;

			switch (cnv->get_state())
			{
			case convoi_t::WAITING_FOR_CLEARANCE_ONE_MONTH:
			case convoi_t::WAITING_FOR_CLEARANCE:
				if (air_vehicle && air_vehicle->is_runway_too_short() == true)
				{
					sprintf(speed_text, "%s (%s) %i%s", translator::translate("Runway too short"), translator::translate("requires"), cnv->front()->get_desc()->get_minimum_runway_length(), translator::translate("m"));
					speed_color = COL_RED;
				}
				else
				{
					sprintf(speed_text, translator::translate("Waiting for clearance!"));
					speed_color = COL_YELLOW;
				}
				break;

			case convoi_t::CAN_START:
			case convoi_t::CAN_START_ONE_MONTH:
				sprintf(speed_text, translator::translate("Waiting for clearance!"));
				speed_color = text_color;
				break;

			case convoi_t::EMERGENCY_STOP:
				char emergency_stop_time[64];
				cnv->snprintf_remaining_emergency_stop_time(emergency_stop_time, sizeof(emergency_stop_time));
				sprintf(speed_text, translator::translate("emergency_stop %s left"), emergency_stop_time);
				speed_color = COL_RED;
				break;

			case convoi_t::LOADING:
				char waiting_time[64];
				cnv->snprintf_remaining_loading_time(waiting_time, sizeof(waiting_time));
				if (cnv->get_schedule()->get_current_entry().is_flag_set(schedule_entry_t::wait_for_time))
				{
					sprintf(speed_text, translator::translate("Waiting for schedule. %s left"), waiting_time);
					speed_color = COL_YELLOW;
				}
				else if (cnv->get_loading_limit())
				{
					if (!cnv->is_wait_infinite() && strcmp(waiting_time, "0:00"))
					{
						sprintf(speed_text, translator::translate("Loading (%i->%i%%), %s left!"), cnv->get_loading_level(), cnv->get_loading_limit(), waiting_time);
						speed_color = COL_YELLOW;
					}
					else
					{
						sprintf(speed_text, translator::translate("Loading (%i->%i%%)!"), cnv->get_loading_level(), cnv->get_loading_limit());
						speed_color = COL_YELLOW;
					}
				}
				else
				{
					sprintf(speed_text, translator::translate("Loading. %s left!"), waiting_time);
					speed_color = text_color;
				}

				break;

			case convoi_t::REVERSING:
				char reversing_time[64];
				cnv->snprintf_remaining_reversing_time(reversing_time, sizeof(reversing_time));
				sprintf(speed_text, translator::translate("Reversing. %s left"), reversing_time);
				speed_color = text_color;
				break;

			case convoi_t::CAN_START_TWO_MONTHS:
			case convoi_t::WAITING_FOR_CLEARANCE_TWO_MONTHS:
				if (air_vehicle && air_vehicle->is_runway_too_short() == true)
				{
					sprintf(speed_text, "%s (%s %i%s)", translator::translate("Runway too short"), translator::translate("requires"), cnv->front()->get_desc()->get_minimum_runway_length(), translator::translate("m"));
					speed_color = COL_RED;
				}
				else
				{
					sprintf(speed_text, translator::translate("clf_chk_stucked"));
					speed_color = COL_ORANGE;
				}
				break;

			case convoi_t::NO_ROUTE:
				if (air_vehicle && air_vehicle->is_runway_too_short() == true)
				{
					sprintf(speed_text, "%s (%s %i%s)", translator::translate("Runway too short"), translator::translate("requires"), cnv->front()->get_desc()->get_minimum_runway_length(), translator::translate("m"));
				}
				else
				{
					sprintf(speed_text, translator::translate("clf_chk_noroute"));
				}
				speed_color = COL_RED;
				break;

			case convoi_t::NO_ROUTE_TOO_COMPLEX:
				sprintf(speed_text, translator::translate("no_route_too_complex_message"));
				speed_color = COL_RED;
				break;

			case convoi_t::OUT_OF_RANGE:
				sprintf(speed_text, "%s (%s %i%s)", translator::translate("out of range"), translator::translate("max"), cnv->front()->get_desc()->get_range(), translator::translate("km"));
				speed_color = COL_RED;
				break;

			default:
				if (air_vehicle && air_vehicle->is_runway_too_short() == true)
				{
					sprintf(speed_text, "%s (%s %i%s)", translator::translate("Runway too short"), translator::translate("requires"), cnv->front()->get_desc()->get_minimum_runway_length(), translator::translate("m"));
					speed_color = COL_RED;
				}
				else
				{
					//use median speed to avoid flickering
					mean_convoi_speed += speed_to_kmh(cnv->get_akt_speed() * 4);
					mean_convoi_speed /= 2;
					const sint32 max_speed = veh->get_desc()->get_topspeed();
					sprintf(speed_text, translator::translate("%i km/h (max. %ikm/h)"), (mean_convoi_speed + 3) / 4, max_speed);
					speed_color = text_color;
				}
			}
			display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, speed_text, ALIGN_LEFT, speed_color, true) + 2;
			ypos_name += LINESPACE;
		}

		// WHERE is it (coordinate?, closest city?, show which line it is traversing?)
		// only show assigned line, if there is one!
		if (veh->get_convoi()->in_depot()) {

		}
		else if (veh->get_convoi()->get_line().is_bound()) {
			w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, translator::translate("Line"), ALIGN_LEFT, text_color, true) + 2;
			w += display_proportional_clip(pos.x + offset.x + 2 + w + 5, pos.y + offset.y + 6 + ypos_name, veh->get_convoi()->get_line()->get_name(), ALIGN_LEFT, veh->get_convoi()->get_line()->get_state_color(), true);
			max_x = max(max_x, w + 5);
		}

		ypos_name += LINESPACE;
		// Near a city?
		grund_t *gr = welt->lookup(veh->get_pos());
		stadt_t *city = welt->get_city(gr->get_pos().get_2d());
		if (city)
		{
			display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, city->get_name(), ALIGN_LEFT, text_color, true) + 2;
		}



		ypos_name = 0;
		const int xpos_extra = VEHICLE_NAME_COLUMN_WIDTH - D_BUTTON_WIDTH - 10;

		// age		
		char year[20];
		sprintf(year, "%s: %i", translator::translate("bought"), veh->get_purchase_time() / 12);
		display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, year, ALIGN_RIGHT, text_color, true) + 2;
		ypos_name += LINESPACE;

		// TODO: odometer	
		//char odometer[20];
		//sprintf(odometer, "%s: %i", translator::translate("odometer"), /*vehicle odometer should be displayed here*/);
		//display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, odometer, ALIGN_RIGHT, text_color, true) + 2;



		const int xoff = VEHICLE_NAME_COLUMN_WIDTH - D_BUTTON_WIDTH;
		int left = pos.x + offset.x + xoff + 4;
		display_base_img(image, left - x, pos.y + offset.y + 13 - y - h / 2, veh->get_owner()->get_player_nr(), false, true);

		image_height = h;

	}
}