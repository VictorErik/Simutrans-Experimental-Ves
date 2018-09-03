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
const char *vehicle_manager_t::sort_text_veh[SORT_MODES_VEH] =
{
	"age",
	"odometer"
};

vehicle_manager_t::sort_mode_desc_t vehicle_manager_t::sortby_desc = by_desc_name;
vehicle_manager_t::sort_mode_veh_t vehicle_manager_t::sortby_veh = by_age;

vehicle_manager_t::vehicle_manager_t(player_t *player_) :
	gui_frame_t( translator::translate("vehicle_manager"), player_),
	player(player_),
	lb_amount_desc(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_amount_veh(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_desc_page(NULL, SYSCOL_TEXT, gui_label_t::centered),
	lb_veh_page(NULL, SYSCOL_TEXT, gui_label_t::centered),
	scrolly_desc(&cont_desc),
	scrolly_veh(&cont_veh)
{

	int y_pos = 5;

	bt_show_available_vehicles.init(button_t::square_state, translator::translate("show_available_vehicles"), scr_coord(D_MARGIN_LEFT, y_pos), scr_size(D_BUTTON_WIDTH * 2, D_BUTTON_HEIGHT));
	bt_show_available_vehicles.add_listener(this);
	bt_show_available_vehicles.set_tooltip(translator::translate("tick_to_show_all_available_vehicles"));
	bt_show_available_vehicles.pressed = false;
	show_available_vehicles = bt_show_available_vehicles.pressed;
	add_component(&bt_show_available_vehicles);

	y_pos += D_BUTTON_HEIGHT;

	// waytype tab panel
	tabs_waytype.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	update_tabs();
	tabs_waytype.add_listener(this);
	add_component(&tabs_waytype);

	y_pos += D_BUTTON_HEIGHT;

	bt_select_all.init(button_t::square_state, translator::translate("select_all"), scr_coord(RIGHT_HAND_COLUMN, y_pos), scr_size(D_BUTTON_WIDTH * 2, D_BUTTON_HEIGHT));
	bt_select_all.add_listener(this);
	bt_select_all.set_tooltip(translator::translate("select_all_vehicles_in_the_list"));
	bt_select_all.pressed = false;
	select_all = bt_select_all.pressed;
	//bt_select_all.set_pos(scr_coord(10, y_pos));
	add_component(&bt_select_all);

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

	combo_sorter_veh.clear_elements();
	for (int i = 0; i < SORT_MODES_VEH; i++)
	{
		combo_sorter_veh.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(sort_text_veh[i]), SYSCOL_TEXT));
	}
	combo_sorter_veh.set_pos(scr_coord(RIGHT_HAND_COLUMN, y_pos));
	combo_sorter_veh.set_size(scr_size(D_BUTTON_WIDTH * 2, D_BUTTON_HEIGHT));
	combo_sorter_veh.set_focusable(true);
	combo_sorter_veh.set_selection(sortby_veh);
	combo_sorter_veh.set_highlight_color(1);
	combo_sorter_veh.set_max_size(scr_size(D_BUTTON_WIDTH * 2, LINESPACE * 5 + 2 + 16));
	combo_sorter_veh.add_listener(this);
	add_component(&combo_sorter_veh);

	y_pos += D_BUTTON_HEIGHT * 2;

	// Vehicle desc list
	cont_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	scrolly_desc.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	scrolly_desc.set_show_scroll_x(true);
	scrolly_desc.set_scroll_amount_y(40);
	scrolly_desc.set_visible(true);
	scrolly_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	add_component(&scrolly_desc);


	// Vehicle list
	cont_veh.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	scrolly_veh.set_pos(scr_coord(RIGHT_HAND_COLUMN, y_pos));
	scrolly_veh.set_show_scroll_x(true);
	scrolly_veh.set_scroll_amount_y(40);
	scrolly_veh.set_visible(true);
	scrolly_veh.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	add_component(&scrolly_veh);

	y_pos += VEHICLE_NAME_COLUMN_HEIGHT;

	// Label telling the amount of vehicles
	lb_amount_desc.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	add_component(&lb_amount_desc);

	lb_amount_veh.set_pos(scr_coord(RIGHT_HAND_COLUMN, y_pos));
	add_component(&lb_amount_veh);
	
	// Arrow buttons to shift pages when too many vehicle/desc entries in the list
	bt_desc_prev_page.init(button_t::arrowleft, NULL, scr_coord(D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH - 110, y_pos));
	bt_desc_prev_page.add_listener(this);
	bt_desc_prev_page.set_tooltip(translator::translate("previous_page"));
	//bt_desc_prev_page.set_visible(false);
	add_component(&bt_desc_prev_page);

	lb_desc_page.set_pos(scr_coord(D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH - 90, y_pos));
	//lb_desc_page.set_visible(false);
	add_component(&lb_desc_page);

	bt_desc_next_page.init(button_t::arrowright, NULL, scr_coord(D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH - gui_theme_t::gui_arrow_right_size.w, y_pos));
	bt_desc_next_page.add_listener(this);
	bt_desc_next_page.set_tooltip(translator::translate("next_page"));
	//bt_desc_next_page.set_visible(false);
	add_component(&bt_desc_next_page);

	bt_veh_prev_page.init(button_t::arrowleft, NULL, scr_coord(RIGHT_HAND_COLUMN + VEHICLE_NAME_COLUMN_WIDTH - 110, y_pos));
	bt_veh_prev_page.add_listener(this);
	bt_veh_prev_page.set_tooltip(translator::translate("previous_page"));
	//bt_veh_prev_page.set_visible(false);
	add_component(&bt_veh_prev_page);

	lb_veh_page.set_pos(scr_coord(RIGHT_HAND_COLUMN + VEHICLE_NAME_COLUMN_WIDTH - 90, y_pos));
	//lb_veh_page.set_visible(false);
	add_component(&lb_veh_page);

	bt_veh_next_page.init(button_t::arrowright, NULL, scr_coord(RIGHT_HAND_COLUMN + VEHICLE_NAME_COLUMN_WIDTH - gui_theme_t::gui_arrow_right_size.w, y_pos));
	bt_veh_next_page.add_listener(this);
	bt_veh_next_page.set_tooltip(translator::translate("next_page"));
	//bt_veh_next_page.set_visible(false);
	add_component(&bt_veh_next_page);
		

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


	selected_desc_index = -1;

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
	for (int i = 0; i <desc_info.get_count(); i++)
	{
		delete[] desc_info.get_element(i);
	}
	for (int i = 0; i <veh_info.get_count(); i++)
	{
		delete[] veh_info.get_element(i);
	}
}

bool vehicle_manager_t::action_triggered(gui_action_creator_t *comp, value_t v)           // 28-Dec-01    Markus Weber    Added
{
	// waytype tabs
	if (comp == &tabs_waytype) {
		int const tab = tabs_waytype.get_active_tab_index();
		uint8 old_selected_tab = selected_tab_waytype;
		selected_tab_waytype = tabs_to_index_waytype[tab];
		if (select_all)
		{
			just_selected_all = true;
		}
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
		for (int i = 0; i < veh_info.get_count(); i++)
		{
			veh_info.get_element(i)->set_selection(select_all);
		}
		veh_selection.resize(veh_list.get_count());
		veh_selection.clear();
		if (select_all)
		{
			for (int i = 0; i < veh_list.get_count(); i++)
			{
				veh_selection.append(veh_list.get_element(i));
			}
		}
		else
		{
			veh_selection.clear();
		}
		just_selected_all = true;
		display(scr_coord(0,0));
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
		display_desc_list();
	}

	if (comp == &combo_sorter_veh) {
		sint32 sort_mode = combo_sorter_veh.get_selection();
		if (sort_mode < 0)
		{
			combo_sorter_veh.set_selection(0);
			sort_mode = 0;
		}
		sortby_veh = (sort_mode_veh_t)sort_mode;
		display_veh_list();
	}
	

	if (comp == &bt_desc_prev_page)
	{
		if (page_display_desc <= 1)
		{
			page_display_desc = page_amount_desc;
		}
		else
		{
			page_display_desc--;
		}
		//page_turn_veh = true;
		display_desc_list();
	}

	if (comp == &bt_desc_next_page)
	{
		if (page_display_desc >= page_amount_desc)
		{
			page_display_desc = 1;
		}
		else
		{
			page_display_desc++;
		}
		//page_turn_veh = true;
		display_desc_list();
	}

	if (comp == &bt_veh_prev_page)
	{
		if (page_display_veh <= 1)
		{
			page_display_veh = page_amount_veh;
		}
		else
		{
			page_display_veh--;
		}
		//page_turn_veh = true;
		display_veh_list();
	}

	if (comp == &bt_veh_next_page)
	{
		if (page_display_veh >= page_amount_veh)
		{
			page_display_veh = 1;
		}
		else
		{
			page_display_veh++;
		}
		//page_turn_veh = true;
		display_veh_list();
	}
	return true;
}



void vehicle_manager_t::draw(scr_coord pos, scr_size size)
{
	gui_frame_t::draw(pos, size);


	// details window

	// upgrade window

	// general information window



	bool update_veh_list = false;
	bool update_desc_list = false;
	vehicle_for_display = NULL;

	// This handles the selection of the vehicles in the "desc" section
	for (int i = 0; i < desc_info.get_count(); i++)
	{
		if (desc_info.get_element(i)->is_selected())
		{
			if (selected_desc_index != i)
			{
				if (i < desc_list.get_count())
				{
					vehicle_for_display = desc_list.get_element(i);
					selected_desc_index = i;

					for (int j = 0; j < desc_info.get_count(); j++)
					{
						if (j != selected_desc_index)
						{
							desc_info.get_element(j)->set_selection(false);
						}
					}
					update_veh_list = true;
					break;
				}
			}
		}
	}

	// This handles the selection of the vehicles in the "veh" section
	new_count_veh_selection = 0;
	for (int i = 0; i < veh_info.get_count(); i++)
	{
		if (veh_info.get_element(i)->is_selected())
		{
			new_count_veh_selection++;
		}
	}
	if (old_count_veh_selection != new_count_veh_selection)
	{
		old_count_veh_selection = new_count_veh_selection;
		if (!just_selected_all) // Make sure the change in selected amount was not due to the "select all" button
		{
			//select_all = false;
			//bt_select_all.pressed = false;
			build_veh_selection();
		}
	}
	just_selected_all = false;



	new_count_owned_veh = 0;
	way_type = (waytype_t)selected_tab_waytype;
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
			{
				new_count_owned_veh++;
			}
		}
	}
	if (old_count_owned_veh != new_count_owned_veh)
	{
		old_count_owned_veh = new_count_owned_veh;
		build_desc_list();
	}

	if (update_veh_list)
	{
		build_veh_list();
	}
}


void vehicle_manager_t::display(scr_coord pos)
{
	static cbuffer_t buf_vehicle_descs;
	buf_vehicle_descs.clear();
	buf_vehicle_descs.printf(translator::translate("amount_of_vehicle_descs: %u"), amount_desc);
	lb_amount_desc.set_text_pointer(buf_vehicle_descs);

	static cbuffer_t buf_vehicle;
	buf_vehicle.clear();
	buf_vehicle.printf(translator::translate("amount_of_vehicles: %u (%u selected)"), amount_veh, veh_selection.get_count());
	lb_amount_veh.set_text_pointer(buf_vehicle);

	static cbuffer_t buf_desc_page_select;
	buf_desc_page_select.clear();
	buf_desc_page_select.printf(translator::translate("page %i of %i"), page_display_desc, page_amount_desc);
	lb_desc_page.set_text_pointer(buf_desc_page_select);
	lb_desc_page.set_tooltip(translator::translate("500 vehicles_per_page"));
	lb_desc_page.set_align(gui_label_t::centered);

	static cbuffer_t buf_page_select;
	buf_page_select.clear();
	buf_page_select.printf(translator::translate("page %i of %i"), page_display_veh, page_amount_veh);
	lb_veh_page.set_text_pointer(buf_page_select);
	lb_veh_page.set_tooltip(translator::translate("500 vehicles_per_page"));
	lb_veh_page.set_align(gui_label_t::centered);


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
	bool maglev = false;
	bool monorail = false;
	bool train = false;
	bool narrowgauge = false;
	bool tram = false;
	bool truck = false;
	bool ship = false;
	bool air = false;



	if (show_available_vehicles) // TODO: make it timeline dependent, so it doesnt show unavailable vehicles
	{
		if (maglev_t::default_maglev) {
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type))
			{
				//if (info->get)
				desc_list.append(info);
			}
			maglev = true;
		}
		if (monorail_t::default_monorail) {
			monorail = true;
		}
		if (schiene_t::default_schiene) {
			train = true;
		}
		if (narrowgauge_t::default_narrowgauge) {
			narrowgauge = true;
		}
		if (!vehicle_builder_t::get_info(tram_wt).empty()) {
			tram = true;
		}
		if (strasse_t::default_strasse) {
			truck = true;
		}
		if (!vehicle_builder_t::get_info(water_wt).empty()) {
			ship = true;
		}
		if (runway_t::default_runway) {
			air = true;
		}
	}

	// show tabs for the vehicles we own, even though there might be no available vehicles for that category
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::maglev_wt)) {
			maglev = true;
			break;
		}
	}
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::monorail_wt)) {
			monorail = true;
			break;
		}
	}
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::track_wt)) {
			train = true;
			break;
		}
	}
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::narrowgauge_wt)) {
			narrowgauge = true;
			break;
		}
	}
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::tram_wt)) {
			tram = true;
			break;
		}
	}
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::road_wt)) {
			truck = true;
			break;
		}
	}
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::water_wt)) {
			ship = true;
			break;
		}
	}
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if ((cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == waytype_t::air_wt)) {
			air = true;
			break;
		}
	}


	// now we can create the actual tabs
	if (maglev) {
		tabs_waytype.add_tab(&dummy, translator::translate("Maglev"), skinverwaltung_t::maglevhaltsymbol, translator::translate("Maglev"));
		tabs_to_index_waytype[max_idx++] = waytype_t::maglev_wt;
	}
	if (monorail) {
		tabs_waytype.add_tab(&dummy, translator::translate("Monorail"), skinverwaltung_t::monorailhaltsymbol, translator::translate("Monorail"));
		tabs_to_index_waytype[max_idx++] = waytype_t::monorail_wt;
	}
	if (train) {
		tabs_waytype.add_tab(&dummy, translator::translate("Train"), skinverwaltung_t::zughaltsymbol, translator::translate("Train"));
		tabs_to_index_waytype[max_idx++] = waytype_t::track_wt;
	}
	if (narrowgauge) {
		tabs_waytype.add_tab(&dummy, translator::translate("Narrowgauge"), skinverwaltung_t::narrowgaugehaltsymbol, translator::translate("Narrowgauge"));
		tabs_to_index_waytype[max_idx++] = waytype_t::narrowgauge_wt;
	}
	if (tram) {
		tabs_waytype.add_tab(&dummy, translator::translate("Tram"), skinverwaltung_t::tramhaltsymbol, translator::translate("Tram"));
		tabs_to_index_waytype[max_idx++] = waytype_t::tram_wt;
	}
	if (truck) {
		tabs_waytype.add_tab(&dummy, translator::translate("Truck"), skinverwaltung_t::autohaltsymbol, translator::translate("Truck"));
		tabs_to_index_waytype[max_idx++] = waytype_t::road_wt;
	}
	if (ship) {
		tabs_waytype.add_tab(&dummy, translator::translate("Ship"), skinverwaltung_t::schiffshaltsymbol, translator::translate("Ship"));
		tabs_to_index_waytype[max_idx++] = waytype_t::water_wt;
	}
	if (air) {
		tabs_waytype.add_tab(&dummy, translator::translate("Air"), skinverwaltung_t::airhaltsymbol, translator::translate("Air"));
		tabs_to_index_waytype[max_idx++] = waytype_t::air_wt;
	}

	// no tabs to show at all??? Show a tab to display the reason
	if (max_idx <= 0)
	{
		tabs_waytype.add_tab(&dummy, translator::translate("no_vehicles_to_show"));
		tabs_to_index_waytype[max_idx++] = waytype_t::air_wt;
	}
	selected_tab_waytype = tabs_to_index_waytype[tabs_waytype.get_active_tab_index()];
}



void vehicle_manager_t::sort_desc()
{
	if (sortby_desc == by_desc_name || sortby_desc == by_desc_intro_year)
	{
		std::sort(desc_list.begin(), desc_list.end(), compare_desc);
	}

	//How many of each vehicles do we own?
	if (sortby_desc == by_desc_amount)
	{
		uint16* tmp_amounts;
		tmp_amounts = new uint16[amount_desc];
		for (int i = 0; i < amount_desc; i++)
		{
			tmp_amounts[i] = 0;
		}

		for (int i = 0; i < amount_desc; i++)
		{
			for (unsigned j = 0; j < amount_veh_owned; j++)
			{
				if (desc_list.get_element(i) == vehicle_we_own.get_element(j)->get_desc())
				{
					tmp_amounts[i]++;
				}
			}
		}

		desc_list_pr_name.clear();
		desc_list_pr_name.resize(amount_desc);

		// create a new list to keep track of which index number each desc had originally and match the index number to the value we want to sort by
		char **name_with_amount = new char *[amount_desc];
		for (int i = 0; i < amount_desc; i++)
		{
			desc_list_pr_name.append(desc_list.get_element(i));

			name_with_amount[i] = new char[50];
			if (name_with_amount[i] != nullptr)
			{
				sprintf(name_with_amount[i], "%u.%u.", tmp_amounts[i], i);
			}
		}

		std::sort(name_with_amount, name_with_amount + amount_desc, compare_desc_amount);

		// now clear the old list, so we can start build it from scratch again in the new order
		desc_list.clear();
		char c[1] = { 'a' }; // Need to give c some value, since the forloop depends on it
		char accumulated_amount[10] = { 0 };
		char accumulated_index[10] = { 0 };

		int info_section;
		int char_offset;

		for (int i = 0; i < amount_desc; i++)
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



			for (int j = 0; j < amount_desc; j++)
			{
				if (j == veh_index)
				{
					vehicle_desc_t * info = desc_list_pr_name.get_element(j);
					desc_list.append(info);
					break;
				}
			}
		}
		delete[] tmp_amounts;
		for (int i = 0; i < amount_desc; i++)
		{
			if (name_with_amount[i] != nullptr)
			{
				delete[] name_with_amount[i];
			}
		}
	}
}
	
	
void vehicle_manager_t::sort_veh()
{
	if (sortby_veh == by_age || sortby_veh == by_odometer)
	{
		std::sort(veh_list.begin(), veh_list.end(), compare_veh);
	}

}


bool vehicle_manager_t::compare_veh(vehicle_t* veh1, vehicle_t* veh2)
{
	sint32 result = 0;

	switch (sortby_veh) {
	default:
	case by_age:
		result = sgn(veh1->get_purchase_time() - veh2->get_purchase_time());
		break;
	case by_odometer:
		//result = sgn(veh1->get_odometer() - veh2->get_odometer()); // This sort mode only gets active when individual vehicle odometer gets tracked
		break;
		//case by_desc_issues:
		//	result = cnv1.get_id() - cnv2.get_id();
		//	break;
	}
	return result < 0;
}

// Sorting, for the different types of vehicles
bool vehicle_manager_t::compare_desc(vehicle_desc_t* veh1, vehicle_desc_t* veh2)
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


bool vehicle_manager_t::compare_desc_amount(char* veh1, char* veh2)
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
	page_amount_desc = 1;
	page_display_desc = 1;

	vehicle_we_own.clear();
	desc_list.clear();

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
	desc_list.resize(counter);

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
					desc_list.append((vehicle_desc_t*)v->get_desc());
					for (int i = 0; i < desc_list.get_count() - 1; i++)
					{
						if (desc_list.get_element(i) == v->get_desc())
						{
							desc_list.remove_at(i);
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
		desc_list.resize(counter);

		FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type))
		{
			desc_list.append(info);
		}

	}
	

	amount_desc = desc_list.get_count();
	amount_veh_owned = vehicle_we_own.get_count();
	page_amount_desc = ceil((double)desc_list.get_count() / 500);

	display_desc_list();
}


void vehicle_manager_t::display_desc_list()
{
	sort_desc();
	// count how many of each "desc" we own
	uint16* desc_amounts;
	desc_amounts = new uint16[desc_list.get_count()];
	for (int i = 0; i < desc_list.get_count(); i++)
	{
		desc_amounts[i] = 0;
	}

	for (int i = 0; i < desc_list.get_count(); i++)
	{
		for (int j = 0; j < amount_veh_owned; j++)
		{
			if (desc_list.get_element(i) == vehicle_we_own.get_element(j)->get_desc())
			{
				desc_amounts[i]++;
			}
		}
	}

	{
		int i, icnv = 0;
		int page = (page_display_desc * 500) - 500;
		icnv = min(desc_list.get_count(),500);
		if (icnv > desc_list.get_count() - page)
		{
			icnv = desc_list.get_count() - page;
		}
		int ypos = 10;
		cont_desc.remove_all();
		desc_info.clear();
		for (i = 0; i < icnv; i++) {
			gui_desc_info_t* const cinfo = new gui_desc_info_t(desc_list.get_element(i + page), desc_amounts[i + page]);
			cinfo->set_pos(scr_coord(0, ypos));
			cinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, max(cinfo->image_height, 40)));
			desc_info.append(cinfo);
			cont_desc.add_component(cinfo);
			ypos += max(cinfo->image_height, 40);

		}
		desc_info.set_count(icnv);
		cont_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, ypos));
	}

	// When the list is built, this presets the selected vehicle and assign it to the "vehicle_for_display"
	if (desc_info.get_count() > 0)
	{
		desc_info[0]->set_selection(true);
		vehicle_for_display = desc_list.get_element(0);
	}
	build_veh_list();

	// delete the amount of vehicles array
	delete[] desc_amounts;
}

void vehicle_manager_t::build_veh_list()
{
	int counter = 0;
	way_type = (waytype_t)selected_tab_waytype;
	veh_list.clear();
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
	veh_list.resize(counter);
	
	// Fill the vectors with the vehicles we own
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
			{
				vehicle_t* v = cnv->get_vehicle(veh);
				if (v->get_desc() == vehicle_for_display)
				{
					veh_list.append(v);
				}
			}
		}
	}

	amount_veh = veh_list.get_count();


	page_amount_veh = ceil((double)veh_list.get_count() / 500);
	display_veh_list();

}

void vehicle_manager_t::display_veh_list()
{

	sort_veh();
	{
		// display all individual vehicles
		int i, icnv = 0;
		int page = (page_display_veh * 500) - 500;
		icnv = min(veh_list.get_count(), 500);
		if (icnv > veh_list.get_count() - page)
		{
			icnv = veh_list.get_count() - page;
		}
		int ypos = 10;

		cont_veh.remove_all();
		veh_info.clear();
		veh_list.resize(icnv);
		for (i = 0; i < icnv; i++) {
			if (veh_list.get_element(i) != NULL)
			{
				gui_veh_info_t* const cinfo = new gui_veh_info_t(veh_list.get_element(i));
				cinfo->set_pos(scr_coord(0, ypos));
				cinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, max(cinfo->image_height, 50)));
				cinfo->set_selection(select_all);
				veh_info.append(cinfo);
				cont_veh.add_component(cinfo);
				ypos += max(cinfo->image_height, 50);
			}
		}
		veh_info.set_count(icnv);
		cont_veh.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, ypos));
		build_veh_selection();
	}
}


// This builds the actual array of selected vehicles that we will show info about
void vehicle_manager_t::build_veh_selection()
{
	veh_selection.clear();
	veh_selection.resize(veh_list.get_count());

	if (just_selected_all)
	{
		for (int i = 0; i < veh_list.get_count(); i++)
		{
			veh_selection.append(veh_list.get_element(i));
		}
	}
	else
	{
		int page = (page_display_veh * 500) - 500;
		for (int i = 0; i < veh_info.get_count(); i++)
		{
			if (veh_info.get_element(i)->is_selected())
			{
				veh_selection.append(veh_list.get_element(i + page));
			}
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

gui_desc_info_t::gui_desc_info_t(vehicle_desc_t* veh, uint16 vehicleamount)
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
bool gui_desc_info_t::infowin_event(const event_t *ev)
{
		if (IS_LEFTRELEASE(ev)) {
			//selected = !selected;
			selected = true;
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
void gui_desc_info_t::draw(scr_coord offset)
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


gui_veh_info_t::gui_veh_info_t(vehicle_t* veh)
{
	this->veh = veh;
}

/**
* Events werden hiermit an die GUI-components
* gemeldet
* @author Hj. Malthaner
*/
bool gui_veh_info_t::infowin_event(const event_t *ev)
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
void gui_veh_info_t::draw(scr_coord offset)
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