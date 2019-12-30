/*
 * Vehicle management
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 *
 * Author: Ves
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

#include "../descriptor/intro_dates.h"


#include "karte.h"

#define VEHICLE_NAME_COLUMN_WIDTH ((D_BUTTON_WIDTH*5)+15)
#define VEHICLE_NAME_COLUMN_HEIGHT (250)
#define INFORMATION_COLUMN_HEIGHT (20*LINESPACE)
#define UPGRADE_LIST_COLUMN_WIDTH ((D_BUTTON_WIDTH*4) + 15)
#define UPGRADE_LIST_COLUMN_HEIGHT (INFORMATION_COLUMN_HEIGHT - LINESPACE*3)
#define RIGHT_HAND_COLUMN (D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH + 10)
#define SCL_HEIGHT (15*LINESPACE)

#define MIN_WIDTH (VEHICLE_NAME_COLUMN_WIDTH * 2) + (D_MARGIN_LEFT * 3)
#define MIN_HEIGHT (D_BUTTON_HEIGHT + 11 + VEHICLE_NAME_COLUMN_HEIGHT + SCL_HEIGHT + INFORMATION_COLUMN_HEIGHT)

static uint16 tabs_to_index_waytype[8];
static uint16 tabs_to_index_vehicletype[5];
static uint16 tabs_to_index_information[8];
static uint8 max_idx_waytype = 0;
static uint8 max_idx_vehicletype = 0;
static uint8 max_idx_information = 0;
static uint8 selected_tab_waytype = 0;
static uint8 selected_tab_waytype_index = 0;
static uint8 selected_tab_vehicletype = 0;
static uint8 selected_tab_information = 0;
static uint8 selected_sortby_desc = 0;
static uint8 selected_sortby_veh = 0;
static uint8 selected_displ_desc = 0;
static uint8 selected_displ_veh = 0;
static vehicle_desc_t* desc_for_display = NULL;

bool vehicle_manager_t::desc_sortreverse = false;
bool vehicle_manager_t::veh_sortreverse = false;
bool vehicle_manager_t::show_available_vehicles = false;
bool vehicle_manager_t::select_all = false;
bool vehicle_manager_t::hide_veh_in_depot = false;

vehicle_manager_t::sort_mode_desc_t vehicle_manager_t::sortby_desc = by_desc_name;
vehicle_manager_t::sort_mode_veh_t vehicle_manager_t::sortby_veh = by_issue;
vehicle_manager_t::display_mode_desc_t vehicle_manager_t::display_desc = displ_desc_name;
vehicle_manager_t::display_mode_veh_t vehicle_manager_t::display_veh = displ_veh_age;

int vehicle_manager_t::display_desc_by_good = 0;
int vehicle_manager_t::display_desc_by_class = 0;
int vehicle_manager_t::display_veh_by_cargo = 0;

const char *vehicle_manager_t::sort_text_desc[SORT_MODES_DESC] =
{
	"name",
	"notifications",
	"intro_year",
	"amount",
	"cargo_type_and_capacity",
	"speed",
	"catering_level",
	"comfort",
	"classes",
	"power",
	"tractive_effort",
	"weight",
	"axle_load",
	"runway_length"

};
const char *vehicle_manager_t::sort_text_veh[SORT_MODES_VEH] =
{
	"age",
	"odometer",
	"issues",
	"location"
};
const char *vehicle_manager_t::display_text_desc[DISPLAY_MODES_DESC] =
{
	"name",
	"amount",
	"intro_year",
	"speed",
	"cargo_cat",
	"comfort",
	"classes",
	"catering_level",
	"power",
	"tractive_effort",
	"weight",
	"axle_load",
	"runway_length"
};
const char *vehicle_manager_t::display_text_veh[DISPLAY_MODES_VEH] =
{
	"age",
	"odometer",
	"location",
	"cargo"
};

static const char * engine_type_names[11] =
{
	"unknown",
	"steam",
	"diesel",
	"electric",
	"bio",
	"sail",
	"fuel_cell",
	"hydrogene",
	"battery",
	"petrol",
	"turbine"
};

vehicle_manager_t::vehicle_manager_t(player_t *player_) :
	gui_frame_t(translator::translate("vehicle_manager"), player_),
	player(player_),
	lb_amount_desc(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_amount_veh(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_desc_page(NULL, SYSCOL_TEXT, gui_label_t::centered),
	lb_veh_page(NULL, SYSCOL_TEXT, gui_label_t::centered),
	lb_desc_sortby(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_veh_sortby(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_display_desc(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_display_veh(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_upgrade_to_from(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_available_liveries(translator::translate("available_liveries:"), SYSCOL_TEXT, gui_label_t::left),
	scrolly_desc(&cont_desc),
	scrolly_veh(&cont_veh),
	scrolly_upgrade(&cont_upgrade),
	scrolly_livery(&cont_livery)
{
	scr_coord coord_dummy = scr_coord(0, 0);
	scr_size size_dummy = scr_size(0, 0);

	// ----------- Left hand side upper labels, buttons and comboboxes -----------------//
	// Show available vehicles button
	bt_show_available_vehicles.init(button_t::square_state, translator::translate("show_available_vehicles"), coord_dummy, scr_size(D_BUTTON_WIDTH * 2, D_BUTTON_HEIGHT));
	bt_show_available_vehicles.add_listener(this);
	bt_show_available_vehicles.set_tooltip(translator::translate("tick_to_show_all_available_vehicles"));
	bt_show_available_vehicles.pressed = show_available_vehicles;
	add_component(&bt_show_available_vehicles);

	// Waytype tab panel
	tabs_waytype.add_listener(this);
	add_component(&tabs_waytype);

	// Vehicle type tab panel
	tabs_vehicletype.add_listener(this);
	add_component(&tabs_vehicletype);

	// "Desc" sorting label, combobox and reverse sort button
	lb_desc_sortby.set_visible(true);
	add_component(&lb_desc_sortby);	

	combo_sorter_desc.clear_elements();
	for (int i = 0; i < SORT_MODES_DESC; i++)	{
		combo_sorter_desc.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(sort_text_desc[i]), SYSCOL_TEXT));
	}
	combo_sorter_desc.set_focusable(true);
	combo_sorter_desc.set_selection(sortby_desc);
	combo_sorter_desc.set_highlight_color(1);
	combo_sorter_desc.add_listener(this);
	add_component(&combo_sorter_desc);
	
	bt_desc_sortreverse.init(button_t::square_state, translator::translate("reverse_sort_order"), coord_dummy, size_dummy);
	bt_desc_sortreverse.add_listener(this);
	bt_desc_sortreverse.set_tooltip(translator::translate("tick_to_reverse_the_sort_order_of_the_list"));
	bt_desc_sortreverse.pressed = false;
	bt_desc_sortreverse.pressed = desc_sortreverse;
	add_component(&bt_desc_sortreverse);

	// "Desc" display label, combobox and textfield/comboboxcoord_dummy
	lb_display_desc.set_visible(true);
	add_component(&lb_display_desc);

	combo_display_desc.clear_elements();
	for (int i = 0; i < DISPLAY_MODES_DESC; i++)	{
		combo_display_desc.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(display_text_desc[i]), SYSCOL_TEXT));
	}
	combo_display_desc.set_focusable(true);
	combo_display_desc.set_selection(display_desc);
	combo_display_desc.set_highlight_color(1);
	combo_display_desc.add_listener(this);
	add_component(&combo_display_desc);

	ti_desc_display.set_visible(false);
	ti_desc_display.add_listener(this);
	add_component(&ti_desc_display);

	combo_desc_display.clear_elements();
	combo_desc_display.set_focusable(true);
	combo_desc_display.set_visible(true);
	combo_desc_display.set_selection(0);
	combo_desc_display.set_highlight_color(1);
	combo_desc_display.add_listener(this);
	add_component(&combo_desc_display);

	// ----------- Right hand side upper labels, buttons and comboboxes -----------------//
	// "Veh" select all button
	bt_select_all.init(button_t::square_state, translator::translate("preselect_all"), coord_dummy, size_dummy);
	bt_select_all.add_listener(this);
	bt_select_all.set_tooltip(translator::translate("preselects_all_vehicles_in_the_list_automatically"));
	bt_select_all.pressed = select_all;
	add_component(&bt_select_all);

	// Hide vehicles in depot button
	bt_hide_in_depot.init(button_t::square_state, translator::translate("hide_in_depot"), coord_dummy, size_dummy);
	bt_hide_in_depot.add_listener(this);
	bt_hide_in_depot.set_tooltip(translator::translate("hides_vehicles_that_are_currently_located_in_a_depot"));
	bt_hide_in_depot.pressed = hide_veh_in_depot;
	add_component(&bt_hide_in_depot);
	
	// "Veh" sorting label, combobox and reverse sort button
	lb_veh_sortby.set_visible(true);
	add_component(&lb_veh_sortby);

	combo_sorter_veh.clear_elements();
	for (int i = 0; i < SORT_MODES_VEH; i++)	{
		combo_sorter_veh.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(sort_text_veh[i]), SYSCOL_TEXT));
	}
	combo_sorter_veh.set_focusable(true);
	combo_sorter_veh.set_selection(sortby_veh);
	combo_sorter_veh.set_highlight_color(1);
	combo_sorter_veh.add_listener(this);
	add_component(&combo_sorter_veh);
	
	bt_veh_sortreverse.init(button_t::square_state, translator::translate("reverse_sort_order"), coord_dummy, size_dummy);
	bt_veh_sortreverse.add_listener(this);
	bt_veh_sortreverse.set_tooltip(translator::translate("tick_to_reverse_the_sort_order_of_the_list"));
	bt_veh_sortreverse.pressed = veh_sortreverse;
	add_component(&bt_veh_sortreverse);

	// "Veh" display label, combobox and textfield/combobox
	lb_display_veh.set_visible(true);
	add_component(&lb_display_veh);

	combo_display_veh.clear_elements();
	for (int i = 0; i < DISPLAY_MODES_VEH; i++)	{
		combo_display_veh.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(display_text_veh[i]), SYSCOL_TEXT));
	}
	combo_display_veh.set_focusable(true);
	combo_display_veh.set_selection(display_veh);
	combo_display_veh.set_highlight_color(1);
	combo_display_veh.add_listener(this);
	add_component(&combo_display_veh);

	ti_veh_display.set_visible(false);
	ti_veh_display.add_listener(this);
	add_component(&ti_veh_display);

	combo_veh_display.clear_elements();
	combo_veh_display.set_focusable(true);
	combo_veh_display.set_visible(false);
	combo_veh_display.set_selection(0);
	combo_veh_display.set_highlight_color(1);
	combo_veh_display.add_listener(this);
	add_component(&combo_veh_display);
	
	// ----------- The two lists of vehicles and their counting labels -----------------//
	// "Desc" list
	cont_desc.set_size(size_dummy);
	scrolly_desc.set_show_scroll_y(true);
	scrolly_desc.set_scroll_amount_y(40);
	scrolly_desc.set_visible(true);
	add_component(&scrolly_desc);

	// "Veh" list
	cont_veh.set_size(size_dummy);
	scrolly_veh.set_show_scroll_y(true);
	scrolly_veh.set_scroll_amount_y(40);
	scrolly_veh.set_visible(true);
	add_component(&scrolly_veh);
	
	// "Desc" and "Veh" -amount of entries labels
	add_component(&lb_amount_desc);
	add_component(&lb_amount_veh);
	
	// "Desc" and "Veh" -list arrow buttons and labels
	bt_desc_prev_page.init(button_t::arrowleft, NULL, coord_dummy);
	bt_desc_prev_page.add_listener(this);
	bt_desc_prev_page.set_tooltip(translator::translate("previous_page"));
	bt_desc_prev_page.set_visible(false);
	add_component(&bt_desc_prev_page);
	lb_desc_page.set_visible(false);
	add_component(&lb_desc_page);
	bt_desc_next_page.init(button_t::arrowright, NULL, coord_dummy);
	bt_desc_next_page.add_listener(this);
	bt_desc_next_page.set_tooltip(translator::translate("next_page"));
	bt_desc_next_page.set_visible(false);
	add_component(&bt_desc_next_page);

	bt_veh_prev_page.init(button_t::arrowleft, NULL, coord_dummy);
	bt_veh_prev_page.add_listener(this);
	bt_veh_prev_page.set_tooltip(translator::translate("previous_page"));
	bt_veh_prev_page.set_visible(false);
	add_component(&bt_veh_prev_page);
	lb_veh_page.set_visible(false);
	add_component(&lb_veh_page);
	bt_veh_next_page.init(button_t::arrowright, NULL, coord_dummy);
	bt_veh_next_page.add_listener(this);
	bt_veh_next_page.set_tooltip(translator::translate("next_page"));
	bt_veh_next_page.set_visible(false);
	add_component(&bt_veh_next_page);
		
	// ----------- Lower section info box with tab panels, buttons, labels and whatnot -----------------//
	// Lower section tab panels
	max_idx_information = 0;
	tabs_info.add_tab(&dummy, translator::translate("infotab_general_information"));
	tabs_to_index_information[max_idx_information++] = infotab_general;
	tabs_info.add_tab(&cont_economics_info, translator::translate("infotab_economics_information"));
	tabs_to_index_information[max_idx_information++] = infotab_economics;
	tabs_info.add_tab(&cont_maintenance_info, translator::translate("infotab_maintenance_information"));
	tabs_to_index_information[max_idx_information++] = infotab_maintenance;
	tabs_info.add_tab(&dummy, translator::translate("infotab_advanced"));
	tabs_to_index_information[max_idx_information++] = infotab_advanced;

	//selected_tab_information = tabs_to_index_information[tabs_info.get_active_tab_index()];
	if (selected_tab_information <= max_idx_information)
	{
		tabs_info.set_active_tab_index(selected_tab_information);
	}
	else
	{
		tabs_info.set_active_tab_index(0);
	}
	tabs_info.add_listener(this);
	add_component(&tabs_info);
	
	// Economics tab:
	{
		// ---- Livery section ---- //

		
		cont_economics_info.add_component(&lb_available_liveries);

		// Livery list
		cont_livery.set_size(size_dummy);
		scrolly_livery.set_show_scroll_y(true);
		scrolly_livery.set_scroll_amount_y(40);
		scrolly_livery.set_visible(true);
		scrolly_livery.set_focusable(true);
		cont_economics_info.add_component(&scrolly_livery);
	}

	// Maintenance tab:
	{
		// ---- Upgrade section ---- //
		// Upgrade buttons
		bt_upgrade_im.init(button_t::roundbox, translator::translate("upgrade_immediately"), coord_dummy, D_BUTTON_SIZE);
		bt_upgrade_im.add_listener(this);
		bt_upgrade_im.set_tooltip(translator::translate("send_the_convoy_containing_the_vehicle_immediately_to_depot_for_upgrading"));
		bt_upgrade_im.set_visible(true);
		cont_maintenance_info.add_component(&bt_upgrade_im);

		// (I assume this will be possible, hence making space for the button now //ves)
		bt_upgrade_ov.init(button_t::roundbox, translator::translate("upgrade_on_overhaul"), coord_dummy, D_BUTTON_SIZE);
		bt_upgrade_ov.add_listener(this);
		bt_upgrade_ov.set_tooltip(translator::translate("upgrade_the_vehicle_when_vehicle_is_next_due_to_overhaul"));
		bt_upgrade_ov.set_visible(true);
		cont_maintenance_info.add_component(&bt_upgrade_ov);

		// Display upgrades / downgrades button and label
		bt_upgrade_to_from.init(button_t::roundbox, NULL, coord_dummy, scr_size(D_BUTTON_HEIGHT,D_BUTTON_HEIGHT));
		bt_upgrade_to_from.add_listener(this);
		bt_upgrade_to_from.set_tooltip(translator::translate("press_to_switch_between_upgrade_to_or_upgrade_from"));
		bt_upgrade_to_from.set_visible(true);
		cont_maintenance_info.add_component(&bt_upgrade_to_from);

		cont_maintenance_info.add_component(&lb_upgrade_to_from);

		
		// Upgrade list
		cont_upgrade.set_size(size_dummy);
		scrolly_upgrade.set_show_scroll_y(true);
		scrolly_upgrade.set_scroll_amount_y(40);
		scrolly_upgrade.set_visible(true);
		scrolly_upgrade.set_focusable(true);
		cont_maintenance_info.add_component(&scrolly_upgrade);

	}

	// Initiate default values and make stuff that is necesary for the window to work
	//display_desc_by_good = 0;
	//display_desc_by_class = 0;
	//display_veh_by_cargo = 0;
	selected_desc_index = -1;
	selected_upgrade_index = -1;
	display_upgrade_into = true;
	veh_is_selected = false;
	goto_this_desc = desc_for_display;
	sortby_desc = (sort_mode_desc_t)selected_sortby_desc;
	display_desc = (display_mode_desc_t)selected_displ_desc;
	sortby_veh = (sort_mode_veh_t)selected_sortby_veh;
	display_veh = (display_mode_veh_t)selected_displ_veh;
	
	if (desc_for_display)
	{
		selected_tab_waytype_index = 0;
	}
	sprintf(sortby_text, translator::translate("sort_vehicles_by:"));
	sprintf(displayby_text, translator::translate("display_vehicles_by:"));

	update_tabs();
	reset_desc_text_input_display();
	reset_veh_text_input_display();
	set_desc_display_rules();
	build_desc_list();
	display_tab_objects();

	set_min_windowsize(scr_size(MIN_WIDTH, MIN_HEIGHT));
	set_resizemode(diagonal_resize);
	resize(scr_coord(0, 0));
}


vehicle_manager_t::~vehicle_manager_t()
{	
	for (int i = 0; i <desc_info.get_count(); i++)	{
		delete[] desc_info.get_element(i);
	}
	for (int i = 0; i <veh_info.get_count(); i++)	{
		delete[] veh_info.get_element(i);
	}
	for (int i = 0; i <upgrade_info.get_count(); i++)	{
		delete[] upgrade_info.get_element(i);
	}
	for (int i = 0; i < livery_info.get_count(); i++)	{
		delete[] livery_info.get_element(i);
	}
	if (veh_is_selected)	{
		delete[] veh_selection;
	}
}

bool vehicle_manager_t::action_triggered(gui_action_creator_t* comp, value_t v)           // 28-Dec-01    Markus Weber    Added
{
	// ---- Upper section ---- //
	if (comp == &bt_show_available_vehicles) {
		bt_show_available_vehicles.pressed = !bt_show_available_vehicles.pressed;
		show_available_vehicles = bt_show_available_vehicles.pressed;
		update_tabs();
		save_previously_selected_desc();
		page_turn_desc = false;
		build_desc_list();
	}
	if (comp == &tabs_waytype) {
		int const tab = tabs_waytype.get_active_tab_index();
		uint8 old_selected_tab = selected_tab_waytype;
		selected_tab_waytype = tabs_to_index_waytype[tab];
		selected_tab_waytype_index = tab;
		selected_tab_vehicletype = 0;
		update_vehicle_type_tabs();
		build_desc_list();
		page_turn_desc = false;
	}
	if (comp == &tabs_vehicletype) {
		int const tab = tabs_vehicletype.get_active_tab_index();
		uint8 old_selected_tab = selected_tab_vehicletype;
		selected_tab_vehicletype = tabs_to_index_vehicletype[tab];
		build_desc_list();
		page_turn_desc = false;
	}
	if (comp == &bt_select_all) {
		bt_select_all.pressed = !bt_select_all.pressed;
		select_all = bt_select_all.pressed;
		for (int i = 0; i < veh_info.get_count(); i++)
		{
			veh_info.get_element(i)->set_selection(select_all);
		}
		for (int i = 0; i < veh_list.get_count(); i++)
		{
			veh_selection[i] = select_all;
		}

		if (select_all)
		{
			old_count_veh_selection = veh_list.get_count();
			if (veh_list.get_count() > 0)
			{
				veh_is_selected = true;
			}
		}
		else
		{
			old_count_veh_selection = 0;
			veh_is_selected = false;
		}
		display(scr_coord(0, 0));
	}
	if (comp == &bt_hide_in_depot) {
		bt_hide_in_depot.pressed = !bt_hide_in_depot.pressed;
		hide_veh_in_depot = bt_hide_in_depot.pressed;
		build_veh_list();
	}
	if (comp == &combo_sorter_desc) {
		sint32 sort_mode = combo_sorter_desc.get_selection();
		if (sort_mode < 0)		{
			combo_sorter_desc.set_selection(0);
			sort_mode = 0;
		}
		sortby_desc = (sort_mode_desc_t)sort_mode;
		selected_sortby_desc = sortby_desc;
		save_previously_selected_desc();
		page_turn_desc = false;
		display_desc_list();
	}
	if (comp == &combo_sorter_veh) {
		sint32 sort_mode = combo_sorter_veh.get_selection();
		if (sort_mode < 0)		{
			combo_sorter_veh.set_selection(0);
			sort_mode = 0;
		}
		sortby_veh = (sort_mode_veh_t)sort_mode;
		selected_sortby_veh = sortby_veh;
		// Because we cant remember what vehicles we had selected when sorting, reset the selection to whatever the select all button says
		for (int i = 0; i < veh_list.get_count(); i++)		{
			veh_selection[i] = select_all;
		}
		if (select_all)		{
			old_count_veh_selection = veh_list.get_count();
		}
		else		{
			old_count_veh_selection = 0;
		}
		display_veh_list();
	}
	if (comp == &combo_display_desc) {
		sint32 display_mode = combo_display_desc.get_selection();
		if (display_mode < 0)		{
			combo_display_desc.set_selection(0);
			display_mode = 0;
		}
		display_desc = (display_mode_desc_t)display_mode;
		selected_displ_desc = display_desc;
		save_previously_selected_desc();
		page_turn_desc = false;
		reset_desc_text_input_display();
		set_desc_display_rules();
		build_desc_list();
	}
	if (comp == &combo_display_veh) {
		sint32 display_mode = combo_display_veh.get_selection();
		if (display_mode < 0)		{
			combo_display_veh.set_selection(0);
			display_mode = 0;
		}
		display_veh = (display_mode_veh_t)display_mode;
		selected_displ_veh = display_veh;
		reset_veh_text_input_display();
		set_veh_display_rules();
		build_veh_list();
	}
	if (comp == &ti_desc_display) {
		update_desc_text_input_display();
		build_desc_list();
	}
	if (comp == &ti_veh_display) {
		update_veh_text_input_display();
		build_veh_list();
	}
	if (comp == &combo_desc_display) {
		sint32 display_mode = combo_desc_display.get_selection();
		if (display_mode < 0)		{
			combo_desc_display.set_selection(0);
			display_mode = 0;
		}
		if (display_desc == displ_desc_cargo_cat)	{
			display_desc_by_good = display_mode;
		}
		else if (display_desc == displ_desc_classes)	{
			display_desc_by_class = display_mode;
		}
		build_desc_list();
	}
	if (comp == &combo_veh_display) {
		sint32 display_mode = combo_veh_display.get_selection();
		if (display_mode < 0)	{
			combo_veh_display.set_selection(0);
			display_mode = 0;
		}
		if (display_veh == displ_veh_cargo)	{
			display_veh_by_cargo = display_mode;
		}
		build_veh_list();
	}
	if (comp == &bt_desc_sortreverse) {
		bt_desc_sortreverse.pressed = !bt_desc_sortreverse.pressed;
		desc_sortreverse = bt_desc_sortreverse.pressed;
		save_previously_selected_desc();
		page_turn_desc = false;
		display_desc_list();
	}
	if (comp == &bt_veh_sortreverse) {
		bt_veh_sortreverse.pressed = !bt_veh_sortreverse.pressed;
		veh_sortreverse = bt_veh_sortreverse.pressed;
		// Because we cant remember what vehicles we had selected when sorting, reset the selection to whatever the select all button says
		for (int i = 0; i < veh_list.get_count(); i++)	{
			veh_selection[i] = select_all;
		}
		if (select_all)	{
			old_count_veh_selection = veh_list.get_count();
		}
		else	{
			old_count_veh_selection = 0;
		}
		display_veh_list();
	}
	// ---- Middle section ---- //
	if (comp == &bt_desc_prev_page) {
		if (page_display_desc <= 1)	{
			page_display_desc = page_amount_desc;
		}
		else 	{
			page_display_desc--;
		}
		page_turn_desc = true;
		save_previously_selected_desc();
		display_desc_list();
	}
	if (comp == &bt_desc_next_page) {
		if (page_display_desc >= page_amount_desc)	{
			page_display_desc = 1;
		}
		else	{
			page_display_desc++;
		}
		page_turn_desc = true;
		save_previously_selected_desc();
		display_desc_list();
	}
	if (comp == &bt_veh_prev_page) {
		if (page_display_veh <= 1)	{
			page_display_veh = page_amount_veh;
		}
		else	{
			page_display_veh--;
		}
		display_veh_list();
	}
	if (comp == &bt_veh_next_page) {
		if (page_display_veh >= page_amount_veh)	{
			page_display_veh = 1;
		}
		else	{
			page_display_veh++;
		}
		display_veh_list();
	}
	// ---- Lower section ---- //
	if (comp == &tabs_info) {
		int const tab = tabs_info.get_active_tab_index();
		uint8 old_selected_tab = selected_tab_information;
		selected_tab_information = tabs_to_index_information[tab];
		display_tab_objects();
	}
	if (comp == &bt_upgrade_to_from) {
		display_upgrade_into = !display_upgrade_into;
		if (display_upgrade_into)
		{
		}
		else
		{
		}
		build_upgrade_list();
	}

	if (comp == &bt_upgrade_im) {
		// Code that directly sends the convoy to the depot and upgrades the particular vehicle(s)!
	}
	if (comp == &bt_upgrade_ov) {
		// Code that sends the convoy to the depot and upgrades the particular vehicle(s) when it is due to overhaul!
	}
	return true;
}

void vehicle_manager_t::display_tab_objects()
{
	int info_display = (uint16)selected_tab_information;
	if (info_display == infotab_general)	{
	}
	else if (info_display == infotab_economics)	{
		build_livery_list();
	}
	else if (info_display == infotab_maintenance)	{
		build_upgrade_list();
	}
	else if (info_display == infotab_advanced)	{
	}
}

void vehicle_manager_t::reset_desc_text_input_display()
{
	char default_display[64];	
	static cbuffer_t tooltip_syntax_display;
	tooltip_syntax_display.clear();

	ti_desc_display.set_visible(false);
	//combo_desc_display.set_visible(false); // If this line is uncommented, the combobox wont show up at all
	ti_desc_display.set_color(SYSCOL_TEXT_HIGHLIGHT);
	
	if (ti_desc_invalid_entry_form)
	{
		sprintf(default_display, translator::translate("invalid_entry"));
		ti_desc_display.set_color(COL_RED);
		ti_desc_invalid_entry_form = false;
	}
	else
	{
		switch (display_desc)
		{
		case displ_desc_intro_year:
			ti_desc_display.set_visible(true);
			tooltip_syntax_display.printf(translator::translate("text_field_syntax: %s"), "\">1234\", \"<1234\", \"1234\" or \"1234-5678\"");
			if (welt->use_timeline())
			{
				sprintf(default_display, "< %u", welt->get_current_month() / 12);
			}
			else
			{
				sprintf(default_display, "");
			}
			break;
		case displ_desc_amount:
		case displ_desc_speed:
		case displ_desc_catering_level:
		case displ_desc_comfort:
		case displ_desc_power:
		case displ_desc_tractive_effort:
		case displ_desc_weight:
		case displ_desc_axle_load:
		case displ_desc_runway_length:
			ti_desc_display.set_visible(true);
			sprintf(default_display, "> 0");
			tooltip_syntax_display.printf(translator::translate("text_field_syntax: %s"), "\">1234\", \"<1234\", \"1234\" or \"1234-5678\"");
			break;

		case displ_desc_name:
			ti_desc_display.set_visible(true);
			sprintf(default_display, "");
			tooltip_syntax_display.printf(translator::translate("text_field_syntax: %s"), translator::translate("syntax_name_of_vehicle"));
			break;

		case displ_desc_cargo_cat:
			combo_desc_display.clear_elements();
			combo_desc_display.set_visible(true);
			combo_desc_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("all_good"), SYSCOL_TEXT));
			//combo_desc_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("no_good"), SYSCOL_TEXT));

			for (int i = 0; i < goods_manager_t::get_max_catg_index(); i++)
			{
				if (goods_manager_t::get_info_catg_index(i)->get_catg() == 0) // Special freight -> spell out with its own name
				{
					combo_desc_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(goods_manager_t::get_info_catg_index(i)->get_name()), SYSCOL_TEXT));
				}
				else
				{
					combo_desc_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(goods_manager_t::get_info_catg_index(i)->get_catg_name()), SYSCOL_TEXT));
				}
			}
			combo_desc_display.set_selection(display_desc_by_good);
			tooltip_syntax_display.printf(translator::translate("select_a_good_category_from_the_right_drop_down_list"));
			break;

		case displ_desc_classes:
			combo_desc_display.clear_elements();
			combo_desc_display.set_visible(true);

			combo_desc_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("all_pass_classes"), SYSCOL_TEXT));
			for (int j = 0; j < goods_manager_t::passengers->get_number_of_classes(); j++)
			{
				int i = goods_manager_t::passengers->get_number_of_classes() - 1 - j;
				char *class_name = new char[32]();
				if (class_name != nullptr)
				{
					sprintf(class_name, "p_class[%u]", i);
				}
				combo_desc_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(class_name), SYSCOL_TEXT));
				delete[] class_name;
			}	

			combo_desc_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("all_mail_classes"), SYSCOL_TEXT));
			for (int j = 0; j < goods_manager_t::mail->get_number_of_classes(); j++)
			{
				int i = goods_manager_t::mail->get_number_of_classes() - 1 - j;
				char *class_name = new char[32]();
				if (class_name != nullptr)
				{
					sprintf(class_name, "m_class[%u]", i);
				}
				combo_desc_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(class_name), SYSCOL_TEXT));
				delete[] class_name;
			}
			combo_desc_display.set_selection(display_desc_by_good);
			tooltip_syntax_display.printf(translator::translate("select_a_class_to_be_displayed_from_the_right_drop_down_list"));
			break;

		default:
			ti_desc_display.set_visible(true);
			sprintf(default_display, "");
			tooltip_syntax_display.printf(translator::translate("text_field_syntax: %s"), "");
			break;
		}
	}
	lb_display_desc.set_tooltip(tooltip_syntax_display);
	tstrncpy(old_desc_display_param, default_display, sizeof(old_desc_display_param));
	tstrncpy(desc_display_param, default_display, sizeof(desc_display_param));
	ti_desc_display.set_text(desc_display_param, sizeof(desc_display_param));
}

void vehicle_manager_t::update_desc_text_input_display()
{
	const char *t = ti_desc_display.get_text();
	ti_desc_display.set_color(SYSCOL_TEXT_HIGHLIGHT);
	// only change if old name and current name are the same
	// otherwise some unintended undo if renaming would occur
	if (t  &&  t[0] && strcmp(t, desc_display_param) && strcmp(old_desc_display_param, desc_display_param) == 0) {
		// do not trigger this command again
		tstrncpy(old_desc_display_param, t, sizeof(old_desc_display_param));
	}
	set_desc_display_rules();
}

void vehicle_manager_t::set_desc_display_rules()
{
	char entry[64] = { 0 };
	sprintf(entry, ti_desc_display.get_text());

	// For any type of number input, this is the syntax:
	// First logic: ">" or "<" (if specified)
	// First value: "number"
	// Second logic: "-" (if specified)
	// Second value: "number" (if specified)
	//
	// For any type of name input, the algorithm will search for the string in any part of the name

	if (display_desc == displ_desc_name)
	{		
		sprintf(desc_display_name, entry);
		letters_to_compare = 0;
		for (int i = 0; i < sizeof(desc_display_name); i++)
		{
			if (desc_display_name[i] == '\0')
			{
				break;
			}
			else
			{
				letters_to_compare++;
			}
		}
	}
	else
	{
		ch_desc_first_logic[0] = 0;
		ch_desc_second_logic[0] = 0;
		for (int i = 0; i < 10; i++)
		{
			ch_desc_first_value[i] = 0;
			ch_desc_second_value[i] = 0;
		}

		ti_desc_first_logic_exists = false;
		ti_desc_second_logic_exists = false;
		ti_desc_first_value_exists = false;
		ti_desc_second_value_exists = false;
		ti_desc_no_logics = false;
		ti_desc_invalid_entry_form = false;

		desc_display_first_value = 0;
		desc_display_second_value = 0;

		int first_value_offset = 0;
		int secon_value_offset = 0;
		char c[1] = { 'a' }; // need to give c some value, since the forloop depends on it. The strings returned from the sorter will always have a number as its first value, so we should be safe
	
		for (int j = 0; *c != '\0'; j++)
		{
			*c = entry[j];
			if ((*c == '>' || *c == '<') && !ti_desc_first_logic_exists && !ti_desc_no_logics)
			{
				ch_desc_first_logic[0] = *c;
				ti_desc_first_logic_exists = true;
			}
			else if (!ti_desc_second_logic_exists && (*c == '0' || *c == '1' || *c == '2' || *c == '3' || *c == '4' || *c == '5' || *c == '6' || *c == '7' || *c == '8' || *c == '9'))
			{
				ch_desc_first_value[first_value_offset] = *c;
				ti_desc_first_value_exists = true;
				ti_desc_no_logics = true;
				first_value_offset++;
			}
			else if ((*c == '-') && ti_desc_first_value_exists && !ti_desc_second_logic_exists)
			{
				ch_desc_second_logic[0] = *c;
				ti_desc_second_logic_exists = true;
			}
			else if (ti_desc_second_logic_exists && (*c == '0' || *c == '1' || *c == '2' || *c == '3' || *c == '4' || *c == '5' || *c == '6' || *c == '7' || *c == '8' || *c == '9'))
			{
				ch_desc_second_value[secon_value_offset] = *c;
				ti_desc_second_value_exists = true;
				secon_value_offset++;
			}
			else if (*c == ' ' || *c == '\0')
			{
				//ignore spaces
			}
			else
			{
				ti_desc_invalid_entry_form = true;
				break;
			}

		}
		if (ti_desc_invalid_entry_form)
		{
			reset_desc_text_input_display();
		}
		else
		{
			if (ti_desc_first_value_exists)
			{
				desc_display_first_value = std::atoi(ch_desc_first_value);
			}
			if (ti_desc_second_value_exists)
			{
				desc_display_second_value = std::atoi(ch_desc_second_value);
			}
		}
	}
}



void vehicle_manager_t::reset_veh_text_input_display()
{
	char default_display[64];
	static cbuffer_t tooltip_syntax_display;
	tooltip_syntax_display.clear();

	ti_veh_display.set_visible(false);
	//combo_veh_display.set_visible(false); // If this line is uncommented, the combobox wont show up at all
	ti_veh_display.set_color(SYSCOL_TEXT_HIGHLIGHT);


		if (ti_veh_invalid_entry_form)
		{
			ti_veh_display.set_visible(true);
			sprintf(default_display, translator::translate("invalid_entry"));
			ti_veh_display.set_color(COL_RED);
			ti_veh_invalid_entry_form = false;
		}
		else
		{
			switch (display_veh)
			{
			case displ_veh_age:
			case displ_veh_odometer:
				ti_veh_display.set_visible(true);
				tooltip_syntax_display.printf(translator::translate("text_field_syntax: %s"), "\">1234\", \"<1234\", \"1234\" or \"1234-5678\"");
				sprintf(default_display, "> 0");
				break;

			case displ_veh_location:
				ti_veh_display.set_visible(true);
				sprintf(default_display, "");
				tooltip_syntax_display.printf(translator::translate("text_field_syntax: %s"), translator::translate("syntax_name_of_location"));
				break;
			case displ_veh_cargo:
			{
				combo_veh_display.clear_elements();
				combo_veh_display.set_visible(true);
				veh_display_combobox_indexes.clear();
				display_show_any = false;
				int counter = 0;
				if (desc_for_display)
				{
					FOR(vector_tpl<goods_desc_t const*>, const i, welt->get_goods_list()) {
						if (i->get_catg_index() == desc_for_display->get_freight_type()->get_catg_index())
						{
							counter++;
						}
					}
					veh_display_combobox_indexes.resize(counter);
					if (desc_for_display->get_total_capacity() > 0)
					{
						combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("show_all"), SYSCOL_TEXT));
						veh_display_combobox_indexes.append(0);

						// Passenger and mail vehicles display by class here perhaps?
						if (desc_for_display->get_freight_type() == goods_manager_t::passengers)
						{
							combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("empty_vehicle"), SYSCOL_TEXT));
							veh_display_combobox_indexes.append(0);
							combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(desc_for_display->get_freight_type()->get_name()), SYSCOL_TEXT));
							veh_display_combobox_indexes.append(goods_manager_t::INDEX_PAS);
						}
						else if (desc_for_display->get_freight_type() == goods_manager_t::mail)
						{
							combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("empty_vehicle"), SYSCOL_TEXT));
							veh_display_combobox_indexes.append(0);
							combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(desc_for_display->get_freight_type()->get_name()), SYSCOL_TEXT));
							veh_display_combobox_indexes.append(goods_manager_t::INDEX_MAIL);
						}
						else if (desc_for_display->get_freight_type()->get_catg() == 0) // Special freight
						{
							combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("empty_vehicle"), SYSCOL_TEXT));
							veh_display_combobox_indexes.append(0);
							combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(desc_for_display->get_freight_type()->get_name()), SYSCOL_TEXT));
							veh_display_combobox_indexes.append(desc_for_display->get_freight_type()->get_catg());
						}
						else // Normal good car
						{
							display_show_any = true;
							combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("empty_vehicle"), SYSCOL_TEXT));
							veh_display_combobox_indexes.append(0);
							combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("any_load"), SYSCOL_TEXT));
							veh_display_combobox_indexes.append(0);
							FOR(vector_tpl<goods_desc_t const*>, const i, welt->get_goods_list()) {
								if (i->get_catg_index() == desc_for_display->get_freight_type()->get_catg_index())
								{
									combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(i->get_name()), SYSCOL_TEXT));
									veh_display_combobox_indexes.append(i->get_index());
								}
							}
						}
					}
					else // Carries no good
					{
						combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("carries_no_good"), SYSCOL_TEXT));
						display_veh_by_cargo = 0;
					}
				}
				else // No vehicles selected
				{
					combo_veh_display.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate("carries_no_good"), SYSCOL_TEXT));
					display_veh_by_cargo = 0;
				}

				combo_veh_display.set_selection(display_veh_by_cargo);
				tooltip_syntax_display.printf(translator::translate("select_a_cargo_from_the_right_drop_down_list"));
				break;



			}
			default:
				ti_veh_display.set_visible(true);
				sprintf(default_display, "");
				tooltip_syntax_display.printf(translator::translate("text_field_syntax: %s"), "");
				break;
			}
		}
	
	lb_display_veh.set_tooltip(tooltip_syntax_display);
	tstrncpy(old_veh_display_param, default_display, sizeof(old_veh_display_param));
	tstrncpy(veh_display_param, default_display, sizeof(veh_display_param));
	ti_veh_display.set_text(veh_display_param, sizeof(veh_display_param));
}

void vehicle_manager_t::update_veh_text_input_display()
{
	const char *t = ti_veh_display.get_text();
	ti_veh_display.set_color(SYSCOL_TEXT_HIGHLIGHT);
	// only change if old name and current name are the same
	// otherwise some unintended undo if renaming would occur
	if (t  &&  t[0] && strcmp(t, veh_display_param) && strcmp(old_veh_display_param, veh_display_param) == 0) {
		// do not trigger this command again
		tstrncpy(old_veh_display_param, t, sizeof(old_veh_display_param));
	}
	set_veh_display_rules();
}

void vehicle_manager_t::set_veh_display_rules()
{
	char entry[64] = { 0 };
	sprintf(entry, ti_veh_display.get_text());

	// For any type of number input, this is the syntax:
	// First logic: ">" or "<" (if specified)
	// First value: "number"
	// Second logic: "-" (if specified)
	// Second value: "number" (if specified)
	//
	// For any type of name input, the algorithm will search for the string in any part of the name

	if (display_veh == displ_veh_location)
	{
		sprintf(veh_display_location, entry);
		letters_to_compare = 0;
		for (int i = 0; i < sizeof(veh_display_location); i++)
		{
			if (veh_display_location[i] == '\0')
			{
				break;
			}
			else
			{
				letters_to_compare++;
			}
		}
	}
	else
	{
		ch_veh_first_logic[0] = 0;
		ch_veh_second_logic[0] = 0;
		for (int i = 0; i < 10; i++)
		{
			ch_veh_first_value[i] = 0;
			ch_veh_second_value[i] = 0;
		}

		ti_veh_first_logic_exists = false;
		ti_veh_second_logic_exists = false;
		ti_veh_first_value_exists = false;
		ti_veh_second_value_exists = false;
		ti_veh_no_logics = false;
		ti_veh_invalid_entry_form = false;

		veh_display_first_value = 0;
		veh_display_second_value = 0;

		int first_value_offset = 0;
		int secon_value_offset = 0;
		char c[1] = { 'a' };
		for (int j = 0; *c != '\0'; j++)
		{
			*c = entry[j];
			if ((*c == '>' || *c == '<') && !ti_veh_first_logic_exists && !ti_veh_no_logics)
			{
				ch_veh_first_logic[0] = *c;
				ti_veh_first_logic_exists = true;
			}
			else if (!ti_veh_second_logic_exists && (*c == '0' || *c == '1' || *c == '2' || *c == '3' || *c == '4' || *c == '5' || *c == '6' || *c == '7' || *c == '8' || *c == '9'))
			{
				ch_veh_first_value[first_value_offset] = *c;
				ti_veh_first_value_exists = true;
				ti_veh_no_logics = true;
				first_value_offset++;
			}
			else if ((*c == '-') && ti_veh_first_value_exists && !ti_veh_second_logic_exists)
			{
				ch_veh_second_logic[0] = *c;
				ti_veh_second_logic_exists = true;
			}
			else if (ti_veh_second_logic_exists && (*c == '0' || *c == '1' || *c == '2' || *c == '3' || *c == '4' || *c == '5' || *c == '6' || *c == '7' || *c == '8' || *c == '9'))
			{
				ch_veh_second_value[secon_value_offset] = *c;
				ti_veh_second_value_exists = true;
				secon_value_offset++;
			}
			else if (*c == ' ' || *c == '\0')
			{
				//ignore spaces
			}
			else
			{
				ti_veh_invalid_entry_form = true;
				break;
			}

		}
		if (ti_veh_invalid_entry_form)
		{
			reset_veh_text_input_display();
		}
		else
		{
			if (ti_veh_first_value_exists)
			{
				veh_display_first_value = std::atoi(ch_veh_first_value);
			}
			if (ti_veh_second_value_exists)
			{
				veh_display_second_value = std::atoi(ch_veh_second_value);
			}
		}
	}
}

uint8 vehicle_manager_t::return_desc_category(vehicle_desc_t*desc)
{
	uint8 desc_category = 0;
	if (desc->get_engine_type() == vehicle_desc_t::electric && (desc->get_freight_type() == goods_manager_t::passengers || desc->get_freight_type() == goods_manager_t::mail))
	{
		desc_category = 2;
	}
	else if ((desc->get_freight_type() == goods_manager_t::passengers || desc->get_freight_type() == goods_manager_t::mail) || desc->get_catering_level()>0)
	{
		desc_category = 1;
	}
	else if (desc->get_power() > 0 || desc->get_total_capacity() == 0)
	{
		desc_category = 3;
	}
	else
	{
		desc_category = 4;
	}
	return desc_category;
}

bool vehicle_manager_t::is_desc_displayable(vehicle_desc_t *desc)
{
	bool display = false;
	
	if ((return_desc_category(desc) == selected_tab_vehicletype) || selected_tab_vehicletype == 0)
	{
		if (display_desc == displ_desc_name)
		{
			if (desc_display_name[0] == '\0')
			{
				display = true;
			}
			else
			{
				char desc_name[256] = { 0 };
				sprintf(desc_name, translator::translate(desc->get_name()));
				int counter = 0;
				char c[1] = { 0 };

				for (int i = 0; i < sizeof(desc_name); i++)
				{
					*c = toupper(desc_name[i]);
					if (*c == toupper(desc_display_name[counter]))
					{
						counter++;
						if (counter == letters_to_compare)
						{
							display = true;
							break;
						}
					}
					else
					{
						counter = 0;
					}
				}
			}
		}
		else if (display_desc == displ_desc_cargo_cat)
		{
				if (display_desc_by_good == 0) // Display all
				{
					display = true;
				}
				else if (display_desc_by_good == desc->get_freight_type()->get_catg_index() + 1 && desc->get_total_capacity() > 0) // Display the good category, but only if the vehicle carries any good
				{
					display = true;
				}
		}
		else if (display_desc == displ_desc_classes)
		{
			bool pass_veh = desc->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_PAS;
			bool mail_veh = desc->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_MAIL;
			bool correct_type = false;
			int offset = 0;

			if (pass_veh && display_desc_by_class <= goods_manager_t::passengers->get_number_of_classes() + 1 || mail_veh && display_desc_by_class >= goods_manager_t::mail->get_number_of_classes() + 2)
			{
				correct_type = true;
			}

			if (correct_type)
			{
				if (mail_veh)
				{
					offset = goods_manager_t::passengers->get_number_of_classes() + 1;
				}
				if (display_desc_by_class == offset) // Display all passengers or mail
				{
					display = true;
				}
				else  // Display a passenger or mail class
				{
					for (uint8 i = 0; i < desc->get_number_of_classes(); i++)
					{
						int actual_class = desc->get_number_of_classes() - (display_desc_by_class - offset);
						if (actual_class == i && desc->get_capacity(i) > 0)
						{
							display = true;
							break;
						}
					}
				}
			}
		}
		else
		{
			sint64 value_to_compare = 0;
			switch (display_desc)
			{
			case displ_desc_intro_year:
				value_to_compare = desc->get_intro_year_month() / 12;
				break;
			case displ_desc_amount:
			{
				uint16 desc_amounts = 0;
				FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
					if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
						for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++) {
							vehicle_t* v = cnv->get_vehicle(veh);
							if (desc == (vehicle_desc_t*)v->get_desc())
							{
								desc_amounts++;
							}
						}
					}
				}
				value_to_compare = desc_amounts;
			}
			break;

			case displ_desc_speed:
				value_to_compare = desc->get_topspeed();
				break;
			case displ_desc_catering_level:
				value_to_compare = desc->get_catering_level();
				break;
			case displ_desc_comfort:
			{
				uint8 base_comfort = 0;
				for (int i = 0; i < desc->get_number_of_classes(); i++)
				{
					if (desc->get_comfort(i) > base_comfort)
					{
						base_comfort = desc->get_comfort(i);
					}
				}
				value_to_compare = base_comfort;
			}
				break;
			case displ_desc_power:
				value_to_compare = desc->get_power();
				break;
			case displ_desc_tractive_effort:
				value_to_compare = desc->get_tractive_effort();
				break;
			case displ_desc_weight:
				value_to_compare = desc->get_weight();
				break;
			case displ_desc_axle_load:
				value_to_compare = desc->get_axle_load();
				break;
			case displ_desc_runway_length:
				value_to_compare = desc->get_minimum_runway_length();
				break;
			default:
				break;
			}

			if ((ti_desc_first_value_exists) && !ti_desc_first_logic_exists && !ti_desc_second_logic_exists && !ti_desc_second_value_exists) // only a value is specified
			{
				display = value_to_compare == desc_display_first_value;
			}
			else if ((ti_desc_first_logic_exists && ti_desc_first_value_exists) && !ti_desc_second_logic_exists && !ti_desc_second_value_exists) // greater than, or smaller than specified value
			{
				if (ch_desc_first_logic[0] == '>')
				{
					display = value_to_compare >= desc_display_first_value;
				}
				else
				{
					display = value_to_compare <= desc_display_first_value;
				}
			}
			else if ((ti_desc_first_value_exists && ti_desc_second_logic_exists && ti_desc_second_value_exists) && !ti_desc_first_logic_exists) // greater than lowest the lowest and smaller than highest value specified
			{
				display = (value_to_compare >= desc_display_first_value) && (value_to_compare <= desc_display_second_value);
			}
			else
			{
				display = true;
			}
		}
	}
	return display;
}

bool vehicle_manager_t::is_veh_displayable(vehicle_t *veh)
{
	bool display = false;

	// First, go through the different display categories to see if this vehicle fit
	if (display_veh == displ_veh_cargo)
	{
		if (display_veh_by_cargo == 0) // Show all vehicles
		{
			display = true;
		}
		else if (display_veh_by_cargo == 1) // Show only empty vehicles
		{
			if (veh->get_cargo_carried() <= 0)
			{
				display = true;
			}
		}
		else if (display_veh_by_cargo == 2 && display_show_any)
		{
			if (veh->get_cargo_carried() > 0)
			{
				display = true;
			}
		}
		else
		{
			uint8 number_of_classes = veh->get_desc()->get_number_of_classes();
			vector_tpl<ware_t> fracht_array(number_of_classes);
			fracht_array.clear();
			int counter = 0;
			for (uint8 i = 0; i < number_of_classes; i++)
			{
				FOR(slist_tpl<ware_t>, w, veh->get_cargo(i))
				{
					counter++;
				}
			}
			fracht_array.resize(counter);
			for (uint8 i = 0; i < number_of_classes; i++)
			{
				FOR(slist_tpl<ware_t>, w, veh->get_cargo(i))
				{
					fracht_array.append(w);
				}
			}
			for (int i = 0; i < fracht_array.get_count(); i++)
			{
				if (veh_display_combobox_indexes.get_count() > 0)
				{
					int freight_index = fracht_array.get_element(i).get_index();
					int freight_index_vehicle = veh_display_combobox_indexes[display_veh_by_cargo];
					if (freight_index == freight_index_vehicle)
					{
						display = true;
					}
				}
			}

		}
	}

	else if (display_veh == displ_veh_location) // free text input
	{
		if (veh_display_location[0] == '\0')
		{
			display = true;
		}
		else
		{
			if (veh->get_convoi()->in_depot() && (veh_display_location[0] == 'd' &&veh_display_location[1] == 'e' &&veh_display_location[2] == 'p' &&veh_display_location[3] == 'o' &&veh_display_location[4] == 't'))
			{
				display = true;
			}
			if (veh->get_pos().x >= 0 && veh->get_pos().y >= 0)
			{
				char location_name[256] = { 0 };
				grund_t *gr = welt->lookup(veh->get_pos());
				stadt_t *city = welt->get_city(gr->get_pos().get_2d());
				sprintf(location_name, city ? city->get_name() : welt->find_nearest_city(gr->get_pos().get_2d())->get_name());

				int counter = 0;
				char c[1] = { 0 };

				for (int i = 0; i < sizeof(location_name); i++)
				{
					*c = toupper(location_name[i]);
					if (*c == toupper(veh_display_location[counter]))
					{
						counter++;
						if (counter == letters_to_compare)
						{
							display = true;
							break;
						}
					}
					else
					{
						counter = 0;
					}
				}
			}
		}
	}

	else // numberinput
	{
		sint64 value_to_compare = 0;
		switch (display_veh)
		{
		case displ_veh_age:
			value_to_compare = (welt->get_current_month() / 12) - (veh->get_purchase_time() / 12);
			break;
		case displ_veh_odometer:
			//value_to_compare = ODOMETER VALUE
			break;
		default:
			break;
		}

		if ((ti_veh_first_value_exists) && !ti_veh_first_logic_exists && !ti_veh_second_logic_exists && !ti_veh_second_value_exists) // only a value is specified
		{
			display = value_to_compare == veh_display_first_value;
		}
		else if ((ti_veh_first_logic_exists && ti_veh_first_value_exists) && !ti_veh_second_logic_exists && !ti_veh_second_value_exists) // greater than, or smaller than specified value
		{
			if (ch_veh_first_logic[0] == '>')
			{
				display = value_to_compare >= veh_display_first_value;
			}
			else
			{
				display = value_to_compare <= veh_display_first_value;
			}
		}
		else if ((ti_veh_first_value_exists && ti_veh_second_logic_exists && ti_veh_second_value_exists) && !ti_veh_first_logic_exists) // greater than lowest the lowest and smaller than highest value specified
		{
			display = (value_to_compare >= veh_display_first_value) && (value_to_compare <= veh_display_second_value);
		}
		else
		{
			display = true;
		}

	}

	// Now dont display anyway if any of these statements are true
	if (hide_veh_in_depot && veh->get_convoi()->in_depot())
	{
		display = false;
	}

	return display;
}

void vehicle_manager_t::draw_economics_information(const scr_coord& pos)
{
	char buf[1024];
	char tmp[50];
	const vehicle_desc_t* desc_info_text = NULL;
	desc_info_text = desc_for_display;
	int pos_y = 0;
	int pos_x = 0;
	int width = get_windowsize().w;
	const uint16 month_now_absolute = welt->get_current_month();
	const uint16 month_now = welt->get_timeline_year_month();
	player_nr = welt->get_active_player_nr();

	int column_1 = D_MARGIN_LEFT;
	int column_2 = (width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6;
	int column_3 = ((width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6) * 2;
	int column_4 = ((width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6) * 3;
	int column_5 = ((width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6) * 4;
	int column_6 = ((width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6) * 5;
	int column_7 = ((width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6) * 6; // Right hand edge of the window. Only things left from here!!
		
	buf[0] = '\0';
	if (desc_info_text) {
	}


	display_ddd_box_clip(pos.x + column_3 - 5, pos.y + pos_y, 0, UPGRADE_LIST_COLUMN_HEIGHT, MN_GREY0, MN_GREY4); // Vertical separator

	display_ddd_box_clip(pos.x + column_5 - 5, pos.y + pos_y, 0, UPGRADE_LIST_COLUMN_HEIGHT, MN_GREY0, MN_GREY4); // Vertical separator


}

void vehicle_manager_t::draw_maintenance_information(const scr_coord& pos)
{
	char buf[1024];
	char tmp[50];
	const vehicle_desc_t *desc_info_text = NULL;
	desc_info_text = desc_for_display;
	int pos_y = 0;
	int pos_x = 0;
	const uint16 month_now_absolute = welt->get_current_month();
	const uint16 month_now = welt->get_timeline_year_month();
	player_nr = welt->get_active_player_nr();
	int column_1 = 0;
	int column_2 = D_BUTTON_WIDTH * 4 + D_MARGIN_LEFT;
	int column_3 = D_BUTTON_WIDTH * 6 + D_MARGIN_LEFT;

	buf[0] = '\0';
	if (desc_info_text) {

		COLOR_VAL veh_selected_color = SYSCOL_TEXT;
		if (!veh_is_selected)
		{
			veh_selected_color = SYSCOL_EDIT_TEXT_DISABLED;
		}

		// column 1
		vehicle_as_potential_convoy_t convoy(*desc_info_text);
		int linespace_skips = 0;

		pos_y += LINESPACE * 3;
		// Age
		if (veh_is_selected)
		{
			int max_age = 0;
			int min_age = month_now_absolute;

			for (int j = 0; j < veh_list.get_count(); j++)
			{
				if (veh_selection[j] == true)
				{
					vehicle_t* veh = veh_list.get_element(j);
					if (veh->get_purchase_time() > max_age)
					{
						max_age = month_now_absolute - veh->get_purchase_time();
					}
					if (veh->get_purchase_time() <= min_age)
					{
						min_age = month_now_absolute - veh->get_purchase_time();
					}
				}
			}
			if (max_age < 0)
			{
				max_age = 0;
			}
			if (min_age < 0)
			{
				min_age = 0;
			}

			min_age /= 12;
			max_age /= 12;

			char age_entry[128] = "\0";

			if (max_age != min_age)
			{
				sprintf(age_entry, "%i - %i", max_age, min_age);
			}
			else
			{
				sprintf(age_entry, "%i", max_age);
			}
			if (max_age < 2)
			{
				sprintf(buf, translator::translate("age: %s %s"), age_entry, translator::translate("year"));
			}
			else
			{
				sprintf(buf, translator::translate("age: %s %s"), age_entry, translator::translate("years"));
			}
		}
		else
		{
			sprintf(buf, translator::translate("age:"));
		}
		display_proportional_clip(pos.x + pos_x, pos.y + pos_y, buf, ALIGN_LEFT, veh_selected_color, true);


		pos_y += LINESPACE * 5;


		pos_x = column_2;
		pos_y = 0;

		pos_x = column_3;
		pos_y = 0;



	}

	// Odometer
}


void vehicle_manager_t::draw_general_information(const scr_coord& pos)
{
	char buf[1024];
	const vehicle_desc_t *desc_info_text = NULL;
	desc_info_text = desc_for_display;
	int pos_y = 0;

	// This section is originally fetched from the gui_convoy_assembler_t, however is modified to display colors for different entries, such as reassigned classes, increased maintenance etc.
	buf[0] = '\0';
	if (desc_info_text) {
		// column 1
		vehicle_as_potential_convoy_t convoy(*desc_info_text);
		int linespace_skips = 0;

		// Name and traction type
		int n = sprintf(buf, "%s", translator::translate(desc_info_text->get_name(), welt->get_settings().get_name_language_id()));
		if (desc_info_text->get_power() > 0)
		{
			sprintf(buf + n, " (%s)", translator::translate(engine_type_names[desc_info_text->get_engine_type() + 1]));
		}
		display_proportional_clip(pos.x, pos.y + pos_y, buf, ALIGN_LEFT, SYSCOL_TEXT, true);
		pos_y += LINESPACE;

		// Cost information:
		// If any SELECTED vehicles, find out what their resale values are. If different resale values, then show the range
		double max_resale_value = -1;
		double min_resale_value = desc_info_text->get_value();
		if (veh_is_selected)
		{
		/*	for (uint8 j = 0; j < veh_selection.get_count(); j++)
			{
				vehicle_t* veh = veh_selection.get_element(j);
				if (veh->calc_sale_value() > max_resale_value)
				{
					max_resale_value = veh->calc_sale_value();
				}
				if (veh->calc_sale_value() <= min_resale_value)
				{
					min_resale_value = veh->calc_sale_value();
				}
			}*/
		}

		char tmp[128];
		money_to_string(tmp, desc_info_text->get_value() / 100.0, false);
		sprintf(buf, translator::translate("Cost: %8s"), tmp);
		int extra_x = display_proportional_clip(pos.x, pos.y + pos_y, buf, ALIGN_LEFT, SYSCOL_TEXT, true);
		extra_x += 5;
		char resale_entry[128] = "\0";
		if (max_resale_value != -1)
		{
			char tmp_1[128];
			char tmp_2[128];
			money_to_string(tmp_1, max_resale_value / 100.0, false);
			money_to_string(tmp_2, min_resale_value / 100.0, false);

			if (max_resale_value != min_resale_value)
			{
				sprintf(resale_entry, "(%s %s - %s)", translator::translate("Restwert:"), tmp_1, tmp_2);
			}
			else
			{
				sprintf(resale_entry, "(%s %s)", translator::translate("Restwert:"), tmp_1);
			}
			sprintf(buf, "%s", resale_entry);
			display_proportional_clip(pos.x + extra_x, pos.y + pos_y, buf, ALIGN_LEFT, COL_DARK_GREEN, true);
		}
		pos_y += LINESPACE;

		sprintf(buf, translator::translate("Maintenance: %1.2f$/km, %1.2f$/month\n"), desc_info_text->get_running_cost() / 100.0, desc_info_text->get_adjusted_monthly_fixed_cost(welt) / 100.0);
		display_proportional_clip(pos.x, pos.y + pos_y, buf, ALIGN_LEFT, SYSCOL_TEXT, true);
		pos_y += LINESPACE;

		// Calculations taken from convoy_detail_t
		uint32 run_actual = 0, run_nominal = 0, run_percent = 0;
		uint32 mon_actual = 0, mon_nominal = 0, mon_percent = 0;
		run_nominal += desc_info_text->get_running_cost();
		run_actual += desc_info_text->get_running_cost(welt);
		mon_nominal += welt->calc_adjusted_monthly_figure(desc_info_text->get_fixed_cost());
		mon_actual += welt->calc_adjusted_monthly_figure(desc_info_text->get_fixed_cost(welt));
		bool increased_cost = false;
	
		if (run_nominal) run_percent = ((run_actual - run_nominal) * 100) / run_nominal;
		if (mon_nominal) mon_percent = ((mon_actual - mon_nominal) * 100) / mon_nominal;
		if (run_percent)
		{
			if (mon_percent)
			{
				sprintf(buf, "%s: %d%%/km %d%%/mon", translator::translate("Obsolescence increase"), run_percent, mon_percent);
				increased_cost = true;
			}
			else
			{
				sprintf(buf, "%s: %d%%/km", translator::translate("Obsolescence increase"), run_percent);
				increased_cost = true;
			}
		}
		else
		{
			if (mon_percent)
			{
				sprintf(buf, "%s: %d%%/mon", translator::translate("Obsolescence increase"), mon_percent);
				increased_cost = true;
			}
		}
		if (increased_cost == true)
		{
			display_proportional_clip(pos.x, pos.y + pos_y, buf, ALIGN_LEFT, COL_DARK_BLUE, true);
		}

		pos_y += LINESPACE*2;
		n = 0;

		// Physics information:
		n += sprintf(buf + n, "%s %3d km/h\n", translator::translate("Max. speed:"), desc_info_text->get_topspeed());
		n += sprintf(buf + n, "%s %4.1ft\n", translator::translate("Weight:"), desc_info_text->get_weight() / 1000.0); // Convert kg to tonnes
		if (desc_info_text->get_waytype() != water_wt)
		{
			n += sprintf(buf + n, "%s %it\n", translator::translate("Axle load:"), desc_info_text->get_axle_load());
			char tmpbuf[16];
			const double reduced_way_wear_factor = desc_info_text->get_way_wear_factor() / 10000.0;
			number_to_string(tmpbuf, reduced_way_wear_factor, 4);
			n += sprintf(buf + n, "%s: %s\n", translator::translate("Way wear factor"), tmpbuf);
		}
		n += sprintf(buf + n, "%s %4.1fkN\n", translator::translate("Max. brake force:"), convoy.get_braking_force().to_double() / 1000.0); // Extended only
		n += sprintf(buf + n, "%s %4.3fkN\n", translator::translate("Rolling resistance:"), desc_info_text->get_rolling_resistance().to_double() * (double)desc_info_text->get_weight() / 1000.0); // Extended only

		n += sprintf(buf + n, "%s: ", translator::translate("Range"));
		if (desc_info_text->get_range() == 0)
		{
			n += sprintf(buf + n, translator::translate("unlimited"));
		}
		else
		{
			n += sprintf(buf + n, "%i km", desc_info_text->get_range());
		}
		n += sprintf(buf + n, "\n");

		if (desc_info_text->get_waytype() == air_wt)
		{
			n += sprintf(buf + n, "%s: %i m \n", translator::translate("Minimum runway length"), desc_info_text->get_minimum_runway_length());
		}
		n += sprintf(buf + n, "\n");

		linespace_skips = 0;

		// Engine information:
		linespace_skips = 0;
		if (desc_info_text->get_power() > 0)
		{ // LOCO
			sint32 friction = convoy.get_current_friction();
			sint32 max_weight = convoy.calc_max_starting_weight(friction);
			sint32 min_speed = convoy.calc_max_speed(weight_summary_t(max_weight, friction));
			sint32 min_weight = convoy.calc_max_weight(friction);
			sint32 max_speed = convoy.get_vehicle_summary().max_speed;
			if (min_weight < convoy.get_vehicle_summary().weight)
			{
				min_weight = convoy.get_vehicle_summary().weight;
				max_speed = convoy.calc_max_speed(weight_summary_t(min_weight, friction));
			}
			n += sprintf(buf + n, "%s:", translator::translate("Pulls"));
			n += sprintf(buf + n,
				min_speed == max_speed ? translator::translate(" %gt @ %d km/h ") : translator::translate(" %gt @ %dkm/h%s%gt @ %dkm/h")  /*" %g t @ %d km/h " : " %g t @ %d km/h %s %g t @ %d km/h"*/,
				min_weight * 0.001f, max_speed, translator::translate("..."), max_weight * 0.001f, min_speed);
			n += sprintf(buf + n, "\n");
			n += sprintf(buf + n, translator::translate("Power/tractive force (%s): %4d kW / %d kN\n"), translator::translate(engine_type_names[desc_info_text->get_engine_type() + 1]), desc_info_text->get_power(), desc_info_text->get_tractive_effort());
			if (desc_info_text->get_gear() != 64) // Do this entry really have to be here...??? If not, it should be skipped. Space is precious..
			{
				n += sprintf(buf + n, "%s %0.2f : 1", translator::translate("Gear:"), desc_info_text->get_gear() / 64.0);
			}
			else
			{
				//linespace_skips++;
			}
			n += sprintf(buf + n, "\n");
		}
		else
		{
			n += sprintf(buf + n, "%s ", translator::translate("unpowered"));
			linespace_skips = +2;
		}
		if (linespace_skips > 0)
		{
			for (int i = 0; i < linespace_skips; i++)
			{
				n += sprintf(buf + n, "\n");
			}
		}
		linespace_skips = 0;

		// Copyright information:
		if (char const* const copyright = desc_info_text->get_copyright())
		{
			n += sprintf(buf + n, translator::translate("Constructed by %s"), copyright);
		}
		n += sprintf(buf + n, "\n");
		display_multiline_text(pos.x, pos.y + pos_y, buf, SYSCOL_TEXT);

		// column 2
		// Vehicle intro and retire information:
		pos_y = 0;
		n = 0;
		linespace_skips = 0;
		n += sprintf(buf + n, "%s %s %04d\n",
			translator::translate("Intro. date:"),
			translator::get_month_name(desc_info_text->get_intro_year_month() % 12),
			desc_info_text->get_intro_year_month() / 12
		);

		if (desc_info_text->get_retire_year_month() != DEFAULT_RETIRE_DATE * 12)
		{
			n += sprintf(buf + n, "%s %s %04d\n",
				translator::translate("Retire. date:"),
				translator::get_month_name(desc_info_text->get_retire_year_month() % 12),
				desc_info_text->get_retire_year_month() / 12
			);
		}
		else
		{
			n += sprintf(buf + n, "\n");
		}
		n += sprintf(buf + n, "\n");

		// Capacity information:
		linespace_skips = 0;
		if (desc_info_text->get_total_capacity() > 0)
		{
			bool pass_veh = desc_info_text->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_PAS;
			bool mail_veh = desc_info_text->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_MAIL;

			if (pass_veh || mail_veh)
			{
				uint8 classes_amount = desc_info_text->get_number_of_classes() < 1 ? 1 : desc_info_text->get_number_of_classes();
				char extra_pass[8];
				if (desc_info_text->get_overcrowded_capacity() > 0)
				{
					sprintf(extra_pass, "(%i)", desc_info_text->get_overcrowded_capacity());
				}
				else
				{
					extra_pass[0] = '\0';
				}

				n += sprintf(buf + n, translator::translate("Capacity: %3d %s%s %s\n"),
					desc_info_text->get_total_capacity(), extra_pass,
					translator::translate(desc_info_text->get_freight_type()->get_mass()),
					desc_info_text->get_freight_type()->get_catg() == 0 ? translator::translate(desc_info_text->get_freight_type()->get_name()) : translator::translate(desc_info_text->get_freight_type()->get_catg_name()));

				for (uint8 i = 0; i < classes_amount; i++)
				{
					if (desc_info_text->get_capacity(i) > 0)
					{
						char class_name_untranslated[32];
						if (mail_veh)
						{
							sprintf(class_name_untranslated, "m_class[%u]", i);
						}
						else
						{
							sprintf(class_name_untranslated, "p_class[%u]", i);
						}
						const char* class_name = translator::translate(class_name_untranslated);

						n += sprintf(buf + n, "%s: %3d %s %s ", class_name, desc_info_text->get_capacity(i), translator::translate(desc_info_text->get_freight_type()->get_mass()), translator::translate(desc_info_text->get_freight_type()->get_name()));
						
						// if the classes in any of the SELECTED vehicles are reassigned, display that here
						if (veh_is_selected)
						{
							bool multiple_classes = false;
							int old_reassigned_class = -1;
							uint8 display_class = i;
							for (uint8 j = 0; j < veh_list.get_count(); j++)
							{
								if (veh_selection[j] == true)
								{
									vehicle_t* veh = veh_list.get_element(j);
									if (veh)
									{
										if (old_reassigned_class != veh->get_reassigned_class(i))
										{
											if (old_reassigned_class == -1)
											{
												old_reassigned_class = veh->get_reassigned_class(i);
												display_class = old_reassigned_class;
											}
											else
											{
												multiple_classes = true;
												break;
											}
										}
									}
								}
							}
							if (display_class != i)
							{
								if (multiple_classes)
								{
									n += sprintf(buf + n, "- %s", translator::translate("reassigned_to_multiple"));
								}
								else
								{
									if (mail_veh)
									{
										sprintf(class_name_untranslated, "m_class[%u]", display_class);
									}
									else
									{
										sprintf(class_name_untranslated, "p_class[%u]", display_class);
									}
									const char* reassigned_class_name = translator::translate(class_name_untranslated);
									n += sprintf(buf + n, "- %s: %s", translator::translate("reassigned_to"), reassigned_class_name);
								}
							}
						}
						
						n += sprintf(buf + n, "\n");

						if (pass_veh)
						{
							char timebuf[32];
							uint8 base_comfort = desc_info_text->get_comfort(i);
							uint8 modified_comfort = 0;
							if (i >= desc_info_text->get_catering_level())
							{
								modified_comfort = desc_info_text->get_catering_level() > 0 ? desc_info_text->get_adjusted_comfort(desc_info_text->get_catering_level(), i) - base_comfort : 0;
							}
							char extra_comfort[8];
							if (modified_comfort > 0)
							{
								sprintf(extra_comfort, "+%i", modified_comfort);
							}
							else
							{
								extra_comfort[0] = '\0';
							}

							n += sprintf(buf + n, " - %s %i", translator::translate("Comfort:"), base_comfort);
							welt->sprintf_time_secs(timebuf, sizeof(timebuf), welt->get_settings().max_tolerable_journey(base_comfort + modified_comfort));
							n += sprintf(buf + n, "%s %s %s%s", extra_comfort, translator::translate("(Max. comfortable journey time: "), timebuf, ")\n");
						}
						else
						{
							linespace_skips++;
						}
					}

				}
			}
			else
			{
				n += sprintf(buf + n, translator::translate("Capacity: %3d %s%s %s\n"),
					desc_info_text->get_total_capacity(),
					"\0",
					translator::translate(desc_info_text->get_freight_type()->get_mass()),
					desc_info_text->get_freight_type()->get_catg() == 0 ? translator::translate(desc_info_text->get_freight_type()->get_name()) : translator::translate(desc_info_text->get_freight_type()->get_catg_name()));
				linespace_skips += 2;
			}


			char min_loading_time_as_clock[32];
			char max_loading_time_as_clock[32];
			//Loading time is only relevant if there is something to load.
			welt->sprintf_ticks(min_loading_time_as_clock, sizeof(min_loading_time_as_clock), desc_info_text->get_min_loading_time());
			welt->sprintf_ticks(max_loading_time_as_clock, sizeof(max_loading_time_as_clock), desc_info_text->get_max_loading_time());
			n += sprintf(buf + n, "%s %s - %s \n", translator::translate("Loading time:"), min_loading_time_as_clock, max_loading_time_as_clock);

			if (desc_info_text->get_catering_level() > 0)
			{
				if (mail_veh)
				{
					//Catering vehicles that carry mail are treated as TPOs.
					n += sprintf(buf + n, translator::translate("This is a travelling post office"));
					n += sprintf(buf + n, "\n");
				}
				else
				{
					n += sprintf(buf + n, translator::translate("Catering level: %i"), desc_info_text->get_catering_level());
					n += sprintf(buf + n, "\n");
				}
			}
			else
			{
				linespace_skips++;
			}

		}
		else
		{
			n += sprintf(buf + n, "%s ", translator::translate("this_vehicle_carries_no_good"));
			linespace_skips += 3;
		}
		if (linespace_skips > 0)
		{
			for (int i = 0; i < linespace_skips; i++)
			{
				n += sprintf(buf + n, "\n");
			}
			linespace_skips = 0;
		}

		// Permissive way constraints
		// (If vehicle has, way must have)
		// @author: jamespetts
		const way_constraints_t &way_constraints = desc_info_text->get_way_constraints();
		for (uint8 i = 0; i < way_constraints.get_count(); i++)
		{
			if (way_constraints.get_permissive(i))
			{
				n += sprintf(buf + n, "%s", translator::translate("\nMUST USE: "));
				char tmpbuf[30];
				sprintf(tmpbuf, "Permissive %i-%i", desc_info_text->get_waytype(), i);
				n += sprintf(buf + n, "%s", translator::translate(tmpbuf));
			}
		}
		if (desc_info_text->get_is_tall())
		{
			n += sprintf(buf + n, "%s", translator::translate("\nMUST USE: "));
			n += sprintf(buf + n, "%s", translator::translate("high_clearance_under_bridges_(no_low_bridges)"));
		}


		// Prohibitibve way constraints
		// (If way has, vehicle must have)
		// @author: jamespetts
		for (uint8 i = 0; i < way_constraints.get_count(); i++)
		{
			if (way_constraints.get_prohibitive(i))
			{
				n += sprintf(buf + n, "%s", translator::translate("\nMAY USE: "));
				char tmpbuf[30];
				sprintf(tmpbuf, "Prohibitive %i-%i", desc_info_text->get_waytype(), i);
				n += sprintf(buf + n, "%s", translator::translate(tmpbuf));
			}
		}
		if (desc_info_text->get_tilting())
		{
			n += sprintf(buf + n, "\n");
			n += sprintf(buf + n, "%s: ", translator::translate("equipped_with"));
			n += sprintf(buf + n, "%s", translator::translate("tilting_vehicle_equipment"));
		}

		display_multiline_text(pos.x + 335/*370*/, pos.y + pos_y + (LINESPACE * 2), buf, SYSCOL_TEXT);

	}
}

void vehicle_manager_t::draw(scr_coord pos, scr_size size)
{
	gui_frame_t::draw(pos, size);

	bool update_veh_list = false;
	bool update_desc_list = false;
	bool no_desc_selected = true;

	// This handles the selection of the vehicles in the "desc" section
	{
		for (int i = 0; i < desc_info.get_count(); i++)
		{
			if (desc_info.get_element(i)->is_selected())
			{
				no_desc_selected = false;
				int offset_index = (page_display_desc * desc_pr_page) - desc_pr_page;
				int actual_index = i + offset_index;
				if (selected_desc_index != actual_index)
				{
					if (actual_index < desc_list.get_count())
					{
						desc_for_display = desc_list.get_element(actual_index);
						selected_desc_index = actual_index;

						for (int j = 0; j < desc_info.get_count(); j++)
						{
							if (j + offset_index != selected_desc_index)
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
	}
	if (no_desc_selected && !page_turn_desc && selected_desc_index != -1)
	{
		desc_for_display = NULL;
		update_veh_list = true;
		selected_desc_index = -1;
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
		{
			update_veh_selection();
		}
	}

	// This handles the selection of the vehicles in the "upgrade" section

	for (int i = 0; i < upgrade_info.get_count(); i++)
	{
		if (upgrade_info.get_element(i)->is_selected())
		{
			if (selected_upgrade_index != i)
			{
				if (i < upgrade_info.get_count())
				{
					vehicle_as_upgrade = upgrade_info.get_element(i)->get_upgrade();
					selected_upgrade_index = i;

					for (int j = 0; j < upgrade_info.get_count(); j++)
					{
						if (j != selected_upgrade_index)
						{
							upgrade_info.get_element(j)->set_selection(false);
						}
					}
					break;
				}
			}
		}
	}

	// Upgrade section: When right click an entry, display info for that desc.
	for (int i = 0; i < upgrade_info.get_count(); i++)
	{
		if (upgrade_info.get_element(i)->is_open_info())
		{
			if (i < upgrade_info.get_count())
			{
				goto_this_desc = upgrade_info.get_element(i)->get_upgrade();
				update_desc_list = true;
				break;
			}
		}
	}
	
	

	if (reposition_desc_scroll)
	{
		scrolly_desc.set_scroll_position(0, set_desc_scroll_position);
		reposition_desc_scroll = false;
	}



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
		update_desc_list;
	}

	if (update_desc_list)
	{
		build_desc_list();
	}
	else if (update_veh_list)
	{
		build_veh_list();
	}

	// The "draw" information in the info tabs are fetched here
	// 0 = general information
	// 1 = maintenance information
	// 2 = echonomics information
	// 3 = advanced actions
	int info_display = (uint16)selected_tab_information;
	if (info_display == infotab_general)
	{
		draw_general_information(pos + desc_info_text_pos);
	}
	else if (info_display == infotab_economics)
	{
		if (update_veh_list)
		{
			build_livery_list();
		}
		draw_economics_information(pos + desc_info_text_pos);
	}
	else if (info_display == infotab_maintenance)
	{
		if (update_veh_list)
		{
			build_upgrade_list();
		}
		draw_maintenance_information(pos + desc_info_text_pos);
	}
	else if (info_display == infotab_advanced)
	{

	}

}


void vehicle_manager_t::display(scr_coord pos)
{
	static cbuffer_t buf_desc_sortby;
	buf_desc_sortby.clear();
	buf_desc_sortby.printf(sortby_text);
	lb_desc_sortby.set_text_pointer(buf_desc_sortby);
	
	static cbuffer_t buf_veh_sortby;
	buf_veh_sortby.clear();
	buf_veh_sortby.printf(sortby_text);
	lb_veh_sortby.set_text_pointer(buf_veh_sortby);

	static cbuffer_t buf_desc_display;
	buf_desc_display.clear();
	buf_desc_display.printf(displayby_text);
	lb_display_desc.set_text_pointer(buf_desc_display);

	static cbuffer_t buf_veh_display;
	buf_veh_display.clear();
	buf_veh_display.printf(displayby_text);
	lb_display_veh.set_text_pointer(buf_veh_display);

	static cbuffer_t buf_vehicle_descs;
	buf_vehicle_descs.clear();
	buf_vehicle_descs.printf(translator::translate("amount_of_vehicle_descs: %u"), desc_list.get_count());
	lb_amount_desc.set_text_pointer(buf_vehicle_descs);

	static cbuffer_t buf_vehicle;
	int selected_vehicles_amount = 0;
	for (int j = 0; j < veh_list.get_count(); j++)
	{
		if (veh_selection[j] == true)
		{
			selected_vehicles_amount++;
		}
	}
	buf_vehicle.clear();
	buf_vehicle.printf(translator::translate("amount_of_vehicles: %u (%u selected)"), veh_list.get_count(), selected_vehicles_amount);
	lb_amount_veh.set_text_pointer(buf_vehicle);

	static cbuffer_t buf_desc_page_select;
	static cbuffer_t buf_desc_page_tooltip;
	buf_desc_page_select.clear();
	buf_desc_page_select.printf(translator::translate("page %i of %i"), page_display_desc, page_amount_desc);
	buf_desc_page_tooltip.clear();
	buf_desc_page_tooltip.printf(translator::translate("%i vehicles_per_page"), desc_pr_page);
	lb_desc_page.set_text_pointer(buf_desc_page_select);
	lb_desc_page.set_tooltip(buf_desc_page_tooltip);
	lb_desc_page.set_align(gui_label_t::centered);

	static cbuffer_t buf_veh_page_select;
	static cbuffer_t buf_veh_page_tooltip;
	buf_veh_page_select.clear();
	buf_veh_page_select.printf(translator::translate("page %i of %i"), page_display_veh, page_amount_veh);
	buf_veh_page_tooltip.clear();
	buf_veh_page_tooltip.printf(translator::translate("%i vehicles_per_page"), veh_pr_page);
	lb_veh_page.set_text_pointer(buf_veh_page_select);
	lb_veh_page.set_tooltip(buf_veh_page_tooltip);
	lb_veh_page.set_align(gui_label_t::centered);


	// Lower section stuff:

	bt_upgrade_im.disable();
	bt_upgrade_ov.disable();

	if (desc_for_display)
	{

	}

	if (veh_is_selected)
	{
		// Maintenance tab buttons:
		if (amount_of_upgrades > 0)
		{
			if (display_upgrade_into)
			{
				bt_upgrade_im.enable();
				bt_upgrade_ov.enable();
			}
		}
	}

}


void vehicle_manager_t::set_windowsize(scr_size size)
{
	gui_frame_t::set_windowsize(size);

	// For some strange reason, having "extra_width = get_windowsize().w - MIN_WIDTH" directly generates strange results...
	int minwidth = MIN_WIDTH;
	int minheight = MIN_HEIGHT;
	int width = get_windowsize().w;
	int height = get_windowsize().h;

	// The added width and height to the window from default
	int extra_width = width - minwidth;
	int extra_height = height - minheight;

	// Define the columns for upper section
	// Start by determining which of these translations is the longest, since the GUI depends upon it:
	int label_length = max(display_calc_proportional_string_len_width(sortby_text, -1), display_calc_proportional_string_len_width(displayby_text, -1));
	int combobox_width = (VEHICLE_NAME_COLUMN_WIDTH - label_length - 15) / 2;

	// Now the actual columns:
	int column_1 = D_MARGIN_LEFT;
	int column_2 = column_1 + label_length + 5 + (extra_width / 6);
	int column_3 = column_2 + combobox_width + 5 + (extra_width / 6);

	int y_pos = 5;

	
	// ----------- Left hand side upper labels, buttons and comboboxes -----------------//
	// Show available vehicles button
	bt_show_available_vehicles.set_pos(scr_coord(column_1, y_pos));
	bt_show_available_vehicles.set_size(scr_size(D_BUTTON_WIDTH * 2, D_BUTTON_HEIGHT));
	y_pos += D_BUTTON_HEIGHT;

	// Waytype tab panel
	tabs_waytype.set_pos(scr_coord(column_1, y_pos));
	tabs_waytype.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 11 - 4 + (extra_width / 2), SCL_HEIGHT));
	y_pos += D_BUTTON_HEIGHT * 2;

	// Vehicle type tab panel
	tabs_vehicletype.set_pos(scr_coord(column_1, y_pos));
	tabs_vehicletype.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 11 - 4 + (extra_width/2), SCL_HEIGHT));
	y_pos += (D_BUTTON_HEIGHT*2) + 6;

	// "Desc" sorting label, combobox and reverse sort button
	lb_desc_sortby.set_pos(scr_coord(column_1, y_pos));
	combo_sorter_desc.set_pos(scr_coord(column_2, y_pos));
	combo_sorter_desc.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_sorter_desc.set_max_size(scr_size(combobox_width, LINESPACE * 5 + 2 + 16));
	bt_desc_sortreverse.set_pos(scr_coord(column_3, y_pos));
	bt_desc_sortreverse.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	y_pos += D_BUTTON_HEIGHT;

	// "Desc" display label, combobox and textfield/combobox
	lb_display_desc.set_pos(scr_coord(column_1, y_pos));
	combo_display_desc.set_pos(scr_coord(column_2, y_pos));
	combo_display_desc.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_display_desc.set_max_size(scr_size(combobox_width, LINESPACE * 5 + 2 + 16));
	ti_desc_display.set_pos(scr_coord(column_3, y_pos));
	ti_desc_display.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_desc_display.set_pos(scr_coord(column_3, y_pos));
	combo_desc_display.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_desc_display.set_max_size(scr_size(combobox_width, LINESPACE * 5 + 2 + 16));

	// ----------- Right hand side upper labels, buttons and comboboxes -----------------//
	// Define the columns for use in the "Veh" section
	y_pos = 5;
	column_1 += RIGHT_HAND_COLUMN - D_MARGIN_LEFT + (extra_width / 2); // D_MARGIN_LEFT is already added to column_1
	column_2 += RIGHT_HAND_COLUMN - D_MARGIN_LEFT + (extra_width / 2);
	column_3 += RIGHT_HAND_COLUMN - D_MARGIN_LEFT + (extra_width / 2);

	y_pos += D_BUTTON_HEIGHT;
	y_pos += D_BUTTON_HEIGHT;
	y_pos += D_BUTTON_HEIGHT;
	y_pos += D_BUTTON_HEIGHT;

	// Diverse buttons
	bt_select_all.set_pos(scr_coord(column_1, y_pos));
	bt_select_all.set_size(scr_size(column_2 - column_1, D_BUTTON_HEIGHT));
	bt_hide_in_depot.set_pos(scr_coord(column_2, y_pos));
	bt_hide_in_depot.set_size(scr_size(column_3 - column_2, D_BUTTON_HEIGHT));
	y_pos += D_BUTTON_HEIGHT + 6;

	// "Veh" sorting label, combobox and reverse sort button
	lb_veh_sortby.set_pos(scr_coord(column_1, y_pos));
	lb_veh_sortby.set_size(D_BUTTON_SIZE);
	combo_sorter_veh.set_pos(scr_coord(column_2, y_pos));
	combo_sorter_veh.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_sorter_veh.set_max_size(scr_size(combobox_width, LINESPACE * 5 + 2 + 16));
	bt_veh_sortreverse.set_pos(scr_coord(column_3, y_pos));
	bt_veh_sortreverse.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	y_pos += D_BUTTON_HEIGHT;

	// "Veh" display label, combobox and textfield/combobox
	lb_display_veh.set_pos(scr_coord(column_1, y_pos));
	combo_display_veh.set_pos(scr_coord(column_2, y_pos));
	combo_display_veh.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_display_veh.set_max_size(scr_size(combobox_width, LINESPACE * 5 + 2 + 16));
	ti_veh_display.set_pos(scr_coord(column_3, y_pos));
	ti_veh_display.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_veh_display.set_pos(scr_coord(column_3, y_pos));
	combo_veh_display.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_veh_display.set_max_size(scr_size(combobox_width, LINESPACE * 5 + 2 + 16));
	y_pos += D_BUTTON_HEIGHT * 2;


	// ----------- The two lists of vehicles and their counting labels -----------------//
	// "Desc" list
	//cont_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH + (extra_width / 2), VEHICLE_NAME_COLUMN_HEIGHT + extra_height));
	scrolly_desc.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	scrolly_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH + (extra_width / 2), VEHICLE_NAME_COLUMN_HEIGHT + extra_height));
	for (int i = 0; i < desc_info.get_count(); i++)
	{
		desc_info[i]->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH + (extra_width / 2), desc_info[i]->get_size().h));
	}

	// "Veh" list
	//cont_veh.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH + (extra_width / 2), VEHICLE_NAME_COLUMN_HEIGHT + extra_height));
	scrolly_veh.set_pos(scr_coord(RIGHT_HAND_COLUMN + (extra_width / 2), y_pos));
	scrolly_veh.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH + (extra_width / 2), VEHICLE_NAME_COLUMN_HEIGHT + extra_height));
	for (int i = 0; i < veh_info.get_count(); i++)
	{
		veh_info[i]->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH + (extra_width / 2), veh_info[i]->get_size().h));
	}
	
	y_pos += VEHICLE_NAME_COLUMN_HEIGHT + extra_height;

	// "Desc" and "Veh" -amount of entries labels
	lb_amount_desc.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	lb_amount_veh.set_pos(scr_coord(RIGHT_HAND_COLUMN + (extra_width / 2), y_pos));

	// "Desc" and "Veh" -list arrow buttons and labels
	bt_desc_prev_page.set_pos(scr_coord(D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH - 110 + (extra_width / 2), y_pos));
	lb_desc_page.set_pos(scr_coord(D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH - 90 + (extra_width / 2), y_pos));
	bt_desc_next_page.set_pos(scr_coord(D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH - gui_theme_t::gui_arrow_right_size.w + (extra_width / 2), y_pos));

	bt_veh_prev_page.set_pos(scr_coord(RIGHT_HAND_COLUMN + VEHICLE_NAME_COLUMN_WIDTH - 110 + extra_width, y_pos));
	lb_veh_page.set_pos(scr_coord(RIGHT_HAND_COLUMN + VEHICLE_NAME_COLUMN_WIDTH - 90 + extra_width, y_pos));
	bt_veh_next_page.set_pos(scr_coord(RIGHT_HAND_COLUMN + VEHICLE_NAME_COLUMN_WIDTH - gui_theme_t::gui_arrow_right_size.w + extra_width, y_pos));

	y_pos += D_BUTTON_HEIGHT;

	// ----------- Lower section info box with tab panels, buttons, labels and whatnot -----------------//
	// Lower section tab panels
	tabs_info.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	tabs_info.set_size(scr_size((VEHICLE_NAME_COLUMN_WIDTH * 2) + extra_width, INFORMATION_COLUMN_HEIGHT + (D_BUTTON_HEIGHT * 2)));
	

	y_pos += D_BUTTON_HEIGHT * 2 + LINESPACE;

	desc_info_text_pos = scr_coord(D_MARGIN_LEFT, y_pos);

	// The information tabs have objects attached to some containers. Rearrange the columns into even spaces we can put buttons, lists and labels into

	//y_pos -= tabs_info.get_pos().y + D_BUTTON_HEIGHT * 2;
	column_1 = D_MARGIN_LEFT;
	column_2 = (width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6;
	column_3 = ((width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6) * 2;
	int column_4 = ((width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6) * 3;
	int column_5 = ((width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6) * 4;
	int column_6 = ((width - D_MARGIN_LEFT - D_MARGIN_RIGHT) / 6) * 5;

	int column_width = column_2 - column_1 - 5;
	// Economics tab:
	{
		y_pos = D_MARGIN_TOP;

		lb_available_liveries.set_pos(scr_coord(column_5, y_pos));

		y_pos += LINESPACE * 2;

		// Livery list
		scrolly_livery.set_pos(scr_coord(column_5, y_pos));
		scrolly_livery.set_size(scr_size(column_6 - column_5 + column_width, UPGRADE_LIST_COLUMN_HEIGHT));
		for (int i = 0; i < livery_info.get_count(); i++)
		{
			livery_info[i]->set_size(scr_size(column_6 - column_5 + column_width, livery_info[i]->get_size().h));
		}
		initial_livery_entry_width = column_6 - column_5 + column_width;
	}
	// Maintenance tab:
	{
		y_pos = D_MARGIN_TOP;

		bt_upgrade_im.set_pos(scr_coord(column_3, y_pos));
		bt_upgrade_im.set_size(scr_size(column_width, D_BUTTON_HEIGHT));

		y_pos += D_BUTTON_HEIGHT + 5;

		bt_upgrade_ov.set_pos(scr_coord(column_3, y_pos));
		bt_upgrade_ov.set_size(scr_size(column_width, D_BUTTON_HEIGHT));

		y_pos = D_MARGIN_TOP;

		bt_upgrade_to_from.set_pos(scr_coord(column_4, y_pos));
		lb_upgrade_to_from.set_pos(scr_coord(column_4 + bt_upgrade_to_from.get_size().w + 5, y_pos));

		y_pos += LINESPACE * 2;

		// Upgrade list
		//cont_upgrade.set_size(scr_size(UPGRADE_LIST_COLUMN_WIDTH, UPGRADE_LIST_COLUMN_HEIGHT));
		scrolly_upgrade.set_pos(scr_coord(column_4, y_pos));
		scrolly_upgrade.set_size(scr_size(column_6 - column_4 + column_width, UPGRADE_LIST_COLUMN_HEIGHT));
		for (int i = 0; i < upgrade_info.get_count(); i++)
		{
			upgrade_info[i]->set_size(scr_size(column_6 - column_4 + column_width, upgrade_info[i]->get_size().h));
		}
		initial_upgrade_entry_width = column_6 - column_4 + column_width;
	}




}

void vehicle_manager_t::update_tabs()
{
	// tab panel
	max_idx_waytype = 0;
	bool maglev = false;
	bool monorail = false;
	bool train = false;
	bool narrowgauge = false;
	bool tram = false;
	bool truck = false;
	bool ship = false;
	bool air = false;
	const uint16 month_now = welt->get_timeline_year_month();

	// Store the waytype of the previously selected tab, so we dont loose track of what vehicle we had selected
	waytype_t* old_selected_tab = NULL;
	if (tabs_waytype.get_count() > 0)
	{
		for (int i = 0; i < tabs_waytype.get_count(); i++)
		{
			if (tabs_waytype.get_active_tab_index() == i)
			{
				old_selected_tab = (waytype_t*)tabs_to_index_waytype[i];
				break;
			}
		}
	}
	else if (desc_for_display)
	{ // Should only come in here when window is just opened and no tabs have been created yet
		old_selected_tab = (waytype_t*)desc_for_display->get_waytype();
	}

	tabs_waytype.clear();

	// If the "show available vehicles" is selected, this will show all vehicles in the pakset, if they are currently in production, hence the required tabs are enabled:
	if (show_available_vehicles)
	{
		if (maglev_t::default_maglev) {
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(waytype_t::maglev_wt)) {
				if (!info->is_future(month_now) && !info->is_retired(month_now)) {
					maglev = true;
				}
			}
		}
		if (monorail_t::default_monorail) {
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(waytype_t::monorail_wt)) {
				if (!info->is_future(month_now) && !info->is_retired(month_now)) {
					monorail = true;
				}
			}
		}
		if (schiene_t::default_schiene) {
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(waytype_t::track_wt)) {
				if (!info->is_future(month_now) && !info->is_retired(month_now)) {
					train = true;
				}
			}
		}
		if (narrowgauge_t::default_narrowgauge) {
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(waytype_t::narrowgauge_wt)) {
				if (!info->is_future(month_now) && !info->is_retired(month_now)) {
					narrowgauge = true;
				}
			}
		}
		if (!vehicle_builder_t::get_info(tram_wt).empty()) {
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(waytype_t::tram_wt)) {
				if (!info->is_future(month_now) && !info->is_retired(month_now)) {
					tram = true;
				}
			}
		}
		if (strasse_t::default_strasse) {
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(waytype_t::road_wt)) {
				if (!info->is_future(month_now) && !info->is_retired(month_now)) {
					truck = true;
				}
			}
		}
		if (!vehicle_builder_t::get_info(water_wt).empty()) {
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(waytype_t::water_wt)) {
				if (!info->is_future(month_now) && !info->is_retired(month_now)) {
					ship = true;
				}
			}
		}
		if (runway_t::default_runway) {
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(waytype_t::air_wt)) {
				if (!info->is_future(month_now) && !info->is_retired(month_now)) {
					air = true;
				}
			}
		}
	}

	// always enable tabs for the vehicles we own, even though there might be no available vehicles for that category
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
		tabs_to_index_waytype[max_idx_waytype++] = waytype_t::maglev_wt;
	}
	if (monorail) {
		tabs_waytype.add_tab(&dummy, translator::translate("Monorail"), skinverwaltung_t::monorailhaltsymbol, translator::translate("Monorail"));
		tabs_to_index_waytype[max_idx_waytype++] = waytype_t::monorail_wt;
	}
	if (train) {
		tabs_waytype.add_tab(&dummy, translator::translate("Train"), skinverwaltung_t::zughaltsymbol, translator::translate("Train"));
		tabs_to_index_waytype[max_idx_waytype++] = waytype_t::track_wt;
	}
	if (narrowgauge) {
		tabs_waytype.add_tab(&dummy, translator::translate("Narrowgauge"), skinverwaltung_t::narrowgaugehaltsymbol, translator::translate("Narrowgauge"));
		tabs_to_index_waytype[max_idx_waytype++] = waytype_t::narrowgauge_wt;
	}
	if (tram) {
		tabs_waytype.add_tab(&dummy, translator::translate("Tram"), skinverwaltung_t::tramhaltsymbol, translator::translate("Tram"));
		tabs_to_index_waytype[max_idx_waytype++] = waytype_t::tram_wt;
	}
	if (truck) {
		tabs_waytype.add_tab(&dummy, translator::translate("Truck"), skinverwaltung_t::autohaltsymbol, translator::translate("Truck"));
		tabs_to_index_waytype[max_idx_waytype++] = waytype_t::road_wt;
	}
	if (ship) {
		tabs_waytype.add_tab(&dummy, translator::translate("Ship"), skinverwaltung_t::schiffshaltsymbol, translator::translate("Ship"));
		tabs_to_index_waytype[max_idx_waytype++] = waytype_t::water_wt;
	}
	if (air) {
		tabs_waytype.add_tab(&dummy, translator::translate("Air"), skinverwaltung_t::airhaltsymbol, translator::translate("Air"));
		tabs_to_index_waytype[max_idx_waytype++] = waytype_t::air_wt;
	}

	// no tabs to show at all??? Show a tab to display the reason
	if (max_idx_waytype <= 0)
	{
		tabs_waytype.add_tab(&dummy, translator::translate("no_vehicles_to_show"));
		tabs_to_index_waytype[max_idx_waytype++] = waytype_t::air_wt;
	}
	// When the tabs are built, this presets the previous selected tab, if possible
	else
	{
		if (old_selected_tab)
		{
			for (int i = 0; i < tabs_waytype.get_count(); i++)
			{
				if (old_selected_tab == (waytype_t*)tabs_to_index_waytype[i])
				{
					tabs_waytype.set_active_tab_index(i);
					break;
				}
			}
		}
		else
		{
			tabs_waytype.set_active_tab_index(selected_tab_waytype_index);
		}
	}
	selected_tab_waytype = tabs_to_index_waytype[tabs_waytype.get_active_tab_index()];
	update_vehicle_type_tabs();
}

void vehicle_manager_t::update_vehicle_type_tabs()
{
	way_type = (waytype_t)selected_tab_waytype;
	const uint16 month_now = welt->get_timeline_year_month();
	max_idx_vehicletype = 0;
	
	// Tab organization and naming taken from "gui_convoy_assembler.cc, ie the depot window"
	char passenger_tab[65] = { 0 };	// passenger and mail vehicles
	char electrics_tab[65] = { 0 };	// Electric passenger and mail vehicles
	char powered_tab[65] = { 0 };	// Powered vehicles (with, or without good)
	char unpowered_tab[65] = { 0 };	// Unpowered vehicles (with good)
	if (way_type == waytype_t::road_wt)
	{
		sprintf(passenger_tab, "Bus_tab");
		sprintf(electrics_tab, "TrolleyBus_tab");
		sprintf(powered_tab, "LKW_tab");
		sprintf(unpowered_tab, "Anhaenger_tab");
	}
	else if (way_type == waytype_t::water_wt)
		{
			sprintf(passenger_tab, "Ferry_tab");
			sprintf(powered_tab, "Schiff_tab");
			sprintf(unpowered_tab, "Schleppkahn_tab");
		}
	else if (way_type == waytype_t::air_wt)
	{
		sprintf(passenger_tab, "Flug_tab");
		sprintf(powered_tab, "aircraft_tab");
	}
	else // Some rail based waytype..
	{
		sprintf(passenger_tab, "Pas_tab");
		sprintf(electrics_tab, "Electrics_tab");
		sprintf(powered_tab, "Lokomotive_tab");
		sprintf(unpowered_tab, "Waggon_tab");
	}

	bool display_passenger_tab = false;
	bool display_electrics_tab = false;
	bool display_powered_tab = false;
	bool display_unpowered_tab = false;

	if (show_available_vehicles)
	{
		FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type)) {
			if (!info->is_future(month_now) && !info->is_retired(month_now)) {
				if (return_desc_category(info) == 1)
				{
					display_passenger_tab = true;
				}
				else if(return_desc_category(info) == 2)
				{
					display_electrics_tab = true;
				}
				else if (return_desc_category(info) == 3)
				{
					display_powered_tab = true;
				}
				else if (return_desc_category(info) == 4)
				{
					display_unpowered_tab = true;
				}
			}
			if (display_electrics_tab && display_passenger_tab && display_powered_tab && display_unpowered_tab)
			{
				break;
			}
		}
	}

	// Always show a tab for the vehicle if we own it
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++) {
				vehicle_t* v = cnv->get_vehicle(veh);
				vehicle_desc_t* info = (vehicle_desc_t*)v->get_desc();
				if (return_desc_category(info) == 1)
				{
					display_passenger_tab = true;
				}
				else if (return_desc_category(info) == 2)
				{
					display_electrics_tab = true;
				}
				else if (return_desc_category(info) == 3)
				{
					display_powered_tab = true;
				}
				else if (return_desc_category(info) == 4)
				{
					display_unpowered_tab = true;
				}
			}
		}
		if (display_electrics_tab && display_passenger_tab && display_powered_tab && display_unpowered_tab)
		{
			break;
		}
	}

	// Now build the tabs:
	tabs_vehicletype.clear();

	tabs_vehicletype.add_tab(&dummy, translator::translate("show_all"));
	tabs_to_index_vehicletype[max_idx_vehicletype++] = 0;

	if (display_passenger_tab)
	{
		tabs_vehicletype.add_tab(&dummy, translator::translate(passenger_tab));
		tabs_to_index_vehicletype[max_idx_vehicletype++] = 1;
	}
	if (display_electrics_tab)
	{
		tabs_vehicletype.add_tab(&dummy, translator::translate(electrics_tab));
		tabs_to_index_vehicletype[max_idx_vehicletype++] = 2;
	}
	if (display_powered_tab)
	{
		tabs_vehicletype.add_tab(&dummy, translator::translate(powered_tab));
		tabs_to_index_vehicletype[max_idx_vehicletype++] = 3;
	}
	if (display_unpowered_tab)
	{
		tabs_vehicletype.add_tab(&dummy, translator::translate(unpowered_tab));
		tabs_to_index_vehicletype[max_idx_vehicletype++] = 4;
	}

	tabs_vehicletype.set_active_tab_index(selected_tab_vehicletype);	
	selected_tab_vehicletype = tabs_to_index_vehicletype[tabs_vehicletype.get_active_tab_index()];

}

void vehicle_manager_t::sort_desc()
{
	if (sortby_desc != by_desc_amount)
	{
		std::sort(desc_list.begin(), desc_list.end(), compare_desc);
	}

	// This sort mode is special, since the vehicles them self dont know how many they are. Instead, the vehicles are counted and stored in strings together with the vehicles current index number.
	// Those strings are then what is being sent to the std::sort, where it will decode the string to find out the "amount" number coded in the string. When the array of strings have been sorted,
	// the desc_list is then cleared and being built up again from the bottom, comparing the desc's old index number (which has also been saved in its own array) to the index numbers decoded from the strings.
	if (sortby_desc == by_desc_amount)
	{
		// count each vehcle we own and create a new uint16 for each one.
		uint16* tmp_amounts;
		tmp_amounts = new uint16[amount_desc];
		for (int i = 0; i < amount_desc; i++)
		{
			tmp_amounts[i] = 0;
		}

		// count how many of each vehicle we have.
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
		desc_list_old_index.clear();
		desc_list_old_index.resize(amount_desc);

		// create a new list of strings to keep track of which index number each desc had originally and match the index number to the amount value, which we want to sort by.
		char **name_with_amount = new char *[amount_desc];
		for (int i = 0; i < amount_desc; i++)
		{
			desc_list_old_index.append(desc_list.get_element(i));

			name_with_amount[i] = new char[50];
			if (name_with_amount[i] != nullptr)
			{
				sprintf(name_with_amount[i], "%u.%u.", tmp_amounts[i], i);
			}
		}

		// sort the strings!
		std::sort(name_with_amount, name_with_amount + amount_desc, compare_desc_amount);

		// now clear the old list, so we can start build it from scratch again.
		desc_list.clear();
		char c[1] = { 'a' }; // need to give c some value, since the forloop depends on it. The strings returned from the sorter will always have a number as its first value, so we should be safe
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
				if (*c == '.')
				{ // Section separator recorded, find out which section is done.
					info_section++;
					if (info_section == 1)
					{ // First section done: The amount of this vehicle, not interrested....
						char_offset = j + 1;
					}
					else if (info_section == 2)
					{ // Second section done: Bingo, the old index number. Cut the loop here, since we got what we need
						break;
					}
				}
				else // still a number of some kind, keep track of it
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

			// match the decoded index numbers with the old index number of the desc and append it to the actual desc_list.
			for (int j = 0; j < amount_desc; j++)
			{
				if (j == veh_index)
				{
					vehicle_desc_t * info = desc_list_old_index.get_element(j);
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
	if (sortby_veh == by_age || sortby_veh == by_odometer || sortby_veh == by_issue || sortby_veh == by_location)
	{
		std::sort(veh_list.begin(), veh_list.end(), compare_veh);
	}
}

int vehicle_manager_t::find_desc_issue_level(vehicle_desc_t* desc)
{
	// This section will rank the different 'issues' or problems that desc's may suffer in order to sort by them.
	// most critical issues, like very obsolete vehicles, will be given highest rank. Other ranks, like may upgrade will be given lower priority

	int desc_issue_level = 0;
	int obsolete_state = 100;
	
	// Increased running costs
	uint32 percentage = desc->calc_running_cost(welt, 100) - 100;
	if (percentage > 0)
	{
		desc_issue_level = obsolete_state + percentage;
	}

	// It has upgrades
	if (desc->get_upgrades_count() > 0)
	{
		const uint16 month_now = welt->get_timeline_year_month();
		int amount_of_upgrades = 0;
		int max_display_of_upgrades = 3;
		for (int i = 0; i < desc->get_upgrades_count(); i++)
		{
			vehicle_desc_t* info = (vehicle_desc_t*)desc->get_upgrades(i);
			if (info && (!info->is_future(month_now) && (!info->is_retired(month_now))))
			{
				desc_issue_level = 50;
				break;
			}
		}
	}

	return desc_issue_level;
}
int vehicle_manager_t::find_veh_issue_level(vehicle_t* veh)
{
	// This section will rank the different 'issues' or problems that vehicles may suffer in order to sort by them.
	// Most critical issues, like "stuck", "emergency stop", "out of range" and other similar issues are given the highest rank.
	// TODO: When more states are introduced, like "overhaul" and "lay over", those needs to be included as well.

	int veh_issue_level = 0;
	// Vehicle issue levels:
	int fatal_state = 100;

	switch (veh->get_convoi()->get_state())
	{
		// Fatal states:
	case convoi_t::NO_ROUTE:
	case convoi_t::NO_ROUTE_TOO_COMPLEX:
	case convoi_t::EMERGENCY_STOP:
	case convoi_t::OUT_OF_RANGE:
	case convoi_t::WAITING_FOR_CLEARANCE_TWO_MONTHS:
	case convoi_t::CAN_START_TWO_MONTHS:
		veh_issue_level = fatal_state;
		break;

		//Action required states:

		//Observation required states:
	case convoi_t::WAITING_FOR_CLEARANCE_ONE_MONTH:
		veh_issue_level = 9;
		break;

		//Normal states(driving):
	case convoi_t::WAITING_FOR_CLEARANCE:
	case convoi_t::CAN_START:
	case convoi_t::CAN_START_ONE_MONTH:
		veh_issue_level = 8;
		break;

		//Normal states(loading):
	case convoi_t::MAINTENANCE:
	case convoi_t::OVERHAUL:
	case convoi_t::REPLENISHING:
	case convoi_t::AWAITING_TRIGGER:
		veh_issue_level = 7;
		break;
	case convoi_t::LOADING:
	{
		char waiting_time[64];
		veh->get_convoi()->snprintf_remaining_loading_time(waiting_time, sizeof(waiting_time));
		if (veh->get_convoi()->get_schedule()->get_current_entry().is_flag_set(schedule_entry_t::wait_for_time))
		{// "Waiting for schedule. %s left"			
			veh_issue_level = 4;
		}
		else if (veh->get_convoi()->get_loading_limit())
		{
			if (veh->get_convoi()->is_wait_infinite() && strcmp(waiting_time, "0:00"))
			{// "Loading(%i->%i%%), %s left"				
				veh_issue_level = 5;
			}
			else
			{// "Loading(%i->%i%%)"				
				veh_issue_level = 6;
			}
		}
		else
		{// "Normal loading"			
			veh_issue_level = 3;
		}
	}
	break;

	case convoi_t::REVERSING:
		veh_issue_level = 2;
		break;

	case convoi_t::DRIVING:
		veh_issue_level = 1;
		break;

	default:
		veh_issue_level = 0;
		break;
	}
	air_vehicle_t* air_vehicle = NULL;
	if (veh->get_waytype() == air_wt)
	{
		air_vehicle = (air_vehicle_t*)veh;
	}
	if (air_vehicle && air_vehicle->is_runway_too_short() == true)
	{
		veh_issue_level = fatal_state;
	}
	return veh_issue_level;
}

bool vehicle_manager_t::compare_veh(vehicle_t* veh1, vehicle_t* veh2)
{
	sint64 result = 0;
	switch (sortby_veh) {
	default:
	case by_age:
	{
			result = sgn(veh1->get_purchase_time() - veh2->get_purchase_time());
		
	}
	break;

	case by_odometer:
	{
		//result = sgn(veh1->get_odometer() - veh2->get_odometer()); // This sort mode is not possible yet
	}
	break;

	case by_issue: // This sort mode sorts the vehicles in the most pressing states top
	{
			result = find_veh_issue_level(veh2) - find_veh_issue_level(veh1);		
	}
	break;

	case by_location:
	{
		char city_name1[256];
		if ((veh1->get_pos().x >= 0 && veh1->get_pos().y >= 0) && (veh2->get_pos().x >= 0 && veh2->get_pos().y >= 0))
		{
			grund_t *gr1 = welt->lookup(veh1->get_pos());
			stadt_t *city1 = welt->get_city(gr1->get_pos().get_2d());
			if (city1)
			{
				sprintf(city_name1, "%s", city1->get_name());
			}
			else
			{
				city1 = welt->find_nearest_city(gr1->get_pos().get_2d());
				if (city1)
				{
					const uint32 tiles_to_city = shortest_distance(gr1->get_pos().get_2d(), (koord)city1->get_pos());
					const double km_per_tile = welt->get_settings().get_meters_per_tile() / 1000.0;
					const double km_to_city = (double)tiles_to_city * km_per_tile;
					sprintf(city_name1, "%s%i", city1->get_name(), (int)km_to_city); // Add the distance value to the name to sort by distance to city as well
				}
			}
			char city_name2[256];
			grund_t *gr2 = welt->lookup(veh2->get_pos());
			stadt_t *city2 = welt->get_city(gr2->get_pos().get_2d());
			if (city2)
			{
				sprintf(city_name2, "%s", city2->get_name());
			}
			else
			{
				city2 = welt->find_nearest_city(gr2->get_pos().get_2d());
				if (city2)
				{
					const uint32 tiles_to_city = shortest_distance(gr2->get_pos().get_2d(), (koord)city2->get_pos());
					const double km_per_tile = welt->get_settings().get_meters_per_tile() / 1000.0;
					const double km_to_city = (double)tiles_to_city * km_per_tile;
					sprintf(city_name2, "%s%i", city2->get_name(), (int)km_to_city); // Add the distance value to the name to sort by distance to city as well
				}
			}
				result = strcmp(city_name1, city_name2);			
		}
		else
		{
			if (veh1->get_pos().x >= 0 && veh1->get_pos().y >= 0)
			{
				result = -1;
			}
			else
			{
				result = 1;
			}
		}
		break;
	}
	}
	if (veh_sortreverse)
	{
		result = -result;
	}
	return result < 0;
}

bool vehicle_manager_t::compare_desc(vehicle_desc_t* veh1, vehicle_desc_t* veh2)
{
	sint32 result = 0;

	switch (sortby_desc) {
	default:
	case by_desc_name:
		result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
		break;

	case by_desc_issues:
	{
		if (find_desc_issue_level(veh2) != find_desc_issue_level(veh1))
		{
			result = find_desc_issue_level(veh2) - find_desc_issue_level(veh1);
		}
		else
		{
			result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
			if (desc_sortreverse)
			{
				result = -result;
			}
		}
	}
	break;

	case by_desc_intro_year:
	{
		if (veh1->get_intro_year_month() != veh2->get_intro_year_month())
		{
			result = sgn(veh1->get_intro_year_month() - veh2->get_intro_year_month());
		}
		else
		{
			result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
			if (desc_sortreverse)
			{
				result = -result;
			}
		}
	}
	break;

	case by_desc_cargo_type_and_capacity:
	{
		if ((veh1->get_freight_type()->get_catg_index() != veh2->get_freight_type()->get_catg_index()) && (veh2->get_total_capacity() > 0 && veh1->get_total_capacity() > 0))
		{
			result = sgn(veh1->get_freight_type()->get_catg_index() - veh2->get_freight_type()->get_catg_index());
			if (desc_sortreverse)
			{
				result = -result;
			}
		}
		else
		{
			if (veh2->get_total_capacity() != veh1->get_total_capacity())
			{

				result = sgn(veh2->get_total_capacity() - veh1->get_total_capacity());
				if ((veh2->get_total_capacity() == 0 || veh1->get_total_capacity() == 0) && desc_sortreverse)
				{
					result = -result;
				}

			}
			else
			{
				result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
				if (desc_sortreverse)
				{
					result = -result;
				}
			}
		}
	}
	break;

	case by_desc_speed:
		result = sgn(veh2->get_topspeed() - veh1->get_topspeed());
		break;

	case by_desc_catering_level:
	{
		if ((veh1->get_freight_type()->get_catg_index() != veh2->get_freight_type()->get_catg_index()) && (veh2->get_catering_level() > 0 && veh1->get_catering_level() > 0))
		{
			result = sgn(veh1->get_freight_type()->get_catg_index() - veh2->get_freight_type()->get_catg_index());
			if (desc_sortreverse)
			{
				result = -result;
			}
		}
		else
		{
			if (veh2->get_catering_level() != veh1->get_catering_level())
			{
				result = sgn(veh2->get_catering_level() - veh1->get_catering_level());
				if ((veh2->get_catering_level() == 0 || veh1->get_catering_level() == 0) && desc_sortreverse)
				{
					result = -result;
				}
			}
			else
			{
				result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
				if (desc_sortreverse)
				{
					result = -result;
				}
			}
		}
	}
	break;

	case by_desc_comfort:
	{
		if (veh1->get_freight_type()->get_catg_index() != veh2->get_freight_type()->get_catg_index())
		{
			result = sgn(veh1->get_freight_type()->get_catg_index() - veh2->get_freight_type()->get_catg_index());
			if (desc_sortreverse)
			{
				result = -result;
			}
		}
		else
		{
			uint8 base_comfort_1 = 0;
			uint8 base_comfort_2 = 0;
			uint8 class_comfort_1 = 0;
			uint8 class_comfort_2 = 0;
			int added_comfort_1 = 0;
			int added_comfort_2 = 0;

			for (int i = 0; i < veh1->get_number_of_classes(); i++)
			{
				if (veh1->get_comfort(i) > base_comfort_1)
				{
					base_comfort_1 = veh1->get_comfort(i);
					class_comfort_1 = i;
				}
			}
			for (int i = 0; i < veh2->get_number_of_classes(); i++)
			{
				if (veh2->get_comfort(i) > base_comfort_2)
				{
					base_comfort_2 = veh2->get_comfort(i);
					class_comfort_2 = i;
				}
			}
			// If added comfort due to the vehicle being a catering car is desired, uncomment these two entries:
			//int added_comfort_1 = veh1->get_catering_level() > 0 ? veh1->get_adjusted_comfort(veh1->get_catering_level(), class_comfort_1) - base_comfort_1 : 0;
			//int added_comfort_2 = veh2->get_catering_level() > 0 ? veh2->get_adjusted_comfort(veh2->get_catering_level(), class_comfort_2) - base_comfort_2 : 0;

			if (base_comfort_2 + added_comfort_2 != base_comfort_1 + added_comfort_1)
			{
				result = sgn((base_comfort_2 + added_comfort_2) - (base_comfort_1 + added_comfort_1));
			}
			else
			{
				result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
				if (desc_sortreverse)
				{
					result = -result;
				}
			}
		}
	}
	break;

	case by_desc_classes:
	{
		if (veh1->get_freight_type()->get_catg_index() != veh2->get_freight_type()->get_catg_index() && (veh2->get_total_capacity() > 0 && veh1->get_total_capacity() > 0))
		{
			result = sgn(veh1->get_freight_type()->get_catg_index() - veh2->get_freight_type()->get_catg_index());
			if (desc_sortreverse)
			{
				result = -result;
			}
		}
		else
		{
			if (veh1->get_total_capacity() <= 0)
			{
				result = 1;
				if (desc_sortreverse)
				{
					result = -result;
				}
			}
			else if (veh2->get_total_capacity() <= 0)
			{
				result = -1;
				if (desc_sortreverse)
				{
					result = -result;
				}
			}
			else
			{
				uint8 veh1_classes_amount = veh1->get_number_of_classes() < 1 ? 1 : veh1->get_number_of_classes();
				uint8 veh2_classes_amount = veh2->get_number_of_classes() < 1 ? 1 : veh2->get_number_of_classes();
				sint16 veh1_current_class = -1;
				sint16 veh2_current_class = -1;
				uint16 veh1_current_class_capacity = 0;
				uint16 veh2_current_class_capacity = 0;
				uint8 veh1_highest_class = 0;
				uint8 veh2_highest_class = 0;
				uint8 veh1_accommodations = 0;
				uint8 veh2_accommodations = 0;
				uint16 veh1_highest_class_capacity = 0;
				uint16 veh2_highest_class_capacity = 0;
				int favor_veh = 0; // 1 = veh1, 2 = veh2, 0 = equal
				bool resolved = false;

				uint8 classes_checked = 0;
				uint8 number_of_classes = veh1->get_freight_type()->get_number_of_classes(); // Should be safe to assume that only vehicles carrying the same cargo is being compared here

				for (uint8 j = 1; j < number_of_classes; j++)
				{
					int i = number_of_classes - j;
					classes_checked++;
					if (i <= veh1_classes_amount)
					{
						if (veh1->get_capacity(i) > 0)
						{
							veh1_current_class = i;
							veh1_current_class_capacity = veh1->get_capacity(i);
							veh1_accommodations++;
							if (veh1_highest_class_capacity == 0)
							{
								veh1_highest_class = i;
								veh1_highest_class_capacity = veh1->get_capacity(i);
							}
						}
					}

					if (i <= veh2_classes_amount)
					{
						if (veh2->get_capacity(i) > 0)
						{
							veh2_current_class = i;
							veh2_current_class_capacity = veh2->get_capacity(i);
							veh2_accommodations++;
							if (veh2_highest_class_capacity == 0)
							{
								veh2_highest_class = i;
								veh2_highest_class_capacity = veh2->get_capacity(i);
							}
						}
					}

					// Is it ready to produce a result? If not, run the for loop again...
					if (veh2_highest_class != veh1_highest_class)
					{
						result = sgn(veh2_highest_class - veh1_highest_class);
						resolved = true;
						break;
					}
					else
					{
						if (veh1_accommodations == veh2_accommodations && veh1_current_class != veh2_current_class)
						{
							result = sgn(veh2_current_class - veh1_current_class);
							resolved = true;
							break;
						}
					}
				}

				if (!resolved)
				{
					if (veh1_accommodations != veh2_accommodations)
					{
						result = sgn(veh1_accommodations - veh2_accommodations);
					}
					else
					{
						result = sgn(veh2_highest_class_capacity - veh1_highest_class_capacity);
					}
				}
			}
		}
	}
		break;

	case by_desc_power:
	{
		if (veh2->get_power() != veh1->get_power())
		{
			result = veh2->get_power() - veh1->get_power();
			if ((veh2->get_power() == 0 || veh1->get_power() == 0) && desc_sortreverse)
			{
				result = -result;
			}
		}
		else
		{
			{
				result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
				if (desc_sortreverse)
				{
					result = -result;
				}
			}
		}
	}
	break;
	case by_desc_tractive_effort:
	{
		if (veh2->get_tractive_effort() != veh1->get_tractive_effort())
		{
			result = veh2->get_tractive_effort() - veh1->get_tractive_effort();
			if ((veh2->get_tractive_effort() == 0 || veh1->get_tractive_effort() == 0) && desc_sortreverse)
			{
				result = -result;
			}
		}
		else
		{
			{
				result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
				if (desc_sortreverse)
				{
					result = -result;
				}
			}
		}
	}
		break;
	case by_desc_weight:
	{
		if (veh2->get_weight() != veh1->get_weight())
		{
			result = veh2->get_weight() - veh1->get_weight();
			if ((veh2->get_weight() == 0 || veh1->get_weight() == 0) && desc_sortreverse)
			{
				result = -result;
			}
		}
		else
		{
			{
				result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
				if (desc_sortreverse)
				{
					result = -result;
				}
			}
		}
	}
		break;
	case by_desc_axle_load:
	{
		if (veh2->get_axle_load() != veh1->get_axle_load())
		{
			result = veh2->get_axle_load() - veh1->get_axle_load();
			if ((veh2->get_axle_load() == 0 || veh1->get_axle_load() == 0) && desc_sortreverse)
			{
				result = -result;
			}
		}
		else
		{
			{
				result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
				if (desc_sortreverse)
				{
					result = -result;
				}
			}
		}
	}
		break;
	case by_desc_runway_length:
	{
		if (veh2->get_minimum_runway_length() != veh1->get_minimum_runway_length())
		{
			result = veh2->get_minimum_runway_length() - veh1->get_minimum_runway_length();
		}
		else
		{
			{
				result = strcmp(translator::translate(veh1->get_name()), translator::translate(veh2->get_name()));
				if (desc_sortreverse)
				{
					result = -result;
				}
			}
		}
	}
		break;
	}

	if (desc_sortreverse)
	{
		result = -result;
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
		if (desc_sortreverse)
		{
			result = -result;
		}
		return result > 0;
		break;
	}
}

void vehicle_manager_t::build_livery_list()
{
	cont_livery.remove_all();
	livery_info.clear();
	int ypos = 10;
	amount_of_liveries = 0;
	int box_height = LINESPACE * 3;

	if (desc_for_display)
	{
		// Resize the livery_info vector
		uint32 n = 0;
		for (auto scheme : *welt->get_settings().get_livery_schemes())
		{
			if (scheme->is_available(welt->get_timeline_year_month()))
			{
				n++;
			}
		}
		livery_info.resize(n);

		// Fill the vector with actual entries

		// The following code is in pieces and does not work. "scheme" appears to not know its children schemes, and getting the available schemes out of the vehicle is cumbersome
		//int i = 0;
		//for (auto scheme : *welt->get_settings().get_livery_schemes())
		//{
		//	if (scheme->is_available(welt->get_timeline_year_month()))
		//	{
		//		if (desc_for_display->check_livery(scheme[i].name.c_str()))
		//		{

		//			gui_livery_info_t* const linfo = new gui_livery_info_t(desc_for_display);
		//			linfo->set_pos(scr_coord(0, ypos));
		//			linfo->set_size(scr_size(initial_livery_entry_width, max(linfo->get_entry_height(), box_height)));
		//			livery_info.append(linfo);
		//			cont_livery.add_component(linfo);
		//			ypos += max(linfo->get_entry_height(), box_height);
		//			amount_of_liveries++;

		//		}
		//	}
		//}

		//// This code is from livery_scheme.cc
		//const char* livery_name = NULL;
		//uint16 latest_valid_intro_date = 0;
		////for (uint32 i = 0; i < liveries.get_count(); i++)
		//for (auto scheme : *welt->get_settings().get_livery_schemes())
		//{
		//	if (date >= liveries[i].intro_date && desc->check_livery(liveries[i].name.c_str()) && liveries[i].intro_date > latest_valid_intro_date)
		//	{
		//		// This returns the most recent livery available for this vehicle that is not in the future.
		//		latest_valid_intro_date = liveries[i].intro_date;
		//		livery_name = liveries[i].name.c_str();
		//	}
		//}
		//return livery_name;
		

		// If no liveries available, display just the one it has
		if (amount_of_liveries <= 0)
		{
			gui_livery_info_t* const linfo = new gui_livery_info_t(desc_for_display);
			linfo->set_pos(scr_coord(0, ypos));
			linfo->set_size(scr_size(initial_livery_entry_width, max(linfo->get_entry_height(), box_height)));
			livery_info.append(linfo);
			cont_livery.add_component(linfo);
			ypos += max(linfo->get_entry_height(), box_height);
			amount_of_liveries++;
		}
		livery_info.resize(amount_of_liveries);
		cont_livery.set_size(scr_size(initial_livery_entry_width - 12, ypos));
	}
	display(scr_coord(0, 0));
}

void vehicle_manager_t::build_upgrade_list()
{
	cont_upgrade.remove_all();
	upgrade_info.clear();
	int ypos = 10;
	amount_of_upgrades = 0;
	int box_height = LINESPACE * 3;

	if (display_upgrade_into)
	{
		lb_upgrade_to_from.set_text(translator::translate("this_vehicle_can_upgrade_to"));
	}
	else
	{
		lb_upgrade_to_from.set_text(translator::translate("this_vehicle_can_upgrade_from"));
	}

	if (desc_for_display)
	{
		if (display_upgrade_into) // Build the list of upgrades to this vehicle		
		{
			if (desc_for_display->get_upgrades_count() > 0)
			{
				const uint16 month_now = welt->get_timeline_year_month();
				upgrade_info.resize(desc_for_display->get_upgrades_count());
				for (int i = 0; i < desc_for_display->get_upgrades_count(); i++)
				{
					vehicle_desc_t* upgrade = (vehicle_desc_t*)desc_for_display->get_upgrades(i);
					if (upgrade)
					{
						if (!upgrade->is_future(month_now) && (!upgrade->is_retired(month_now)))
						{
							gui_upgrade_info_t* const uinfo = new gui_upgrade_info_t(upgrade, desc_for_display);
							uinfo->set_pos(scr_coord(0, ypos));
							uinfo->set_size(scr_size(initial_upgrade_entry_width, max(uinfo->get_entry_height(), box_height)));
							upgrade_info.append(uinfo);
							cont_upgrade.add_component(uinfo);
							ypos += max(uinfo->get_entry_height(), box_height);
							amount_of_upgrades++;
						}
					}
				}
			}
		}
		else // Build the list of vehicles that can upgrade into this vehicle
		{
			bool upgrades_from_this;
			int counter = 0;
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type))
			{
				counter++;
			}
			upgrade_info.resize(counter);
			FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type))
			{
				upgrades_from_this = false;
				if (info->get_upgrades_count() > 0)
				{
					for (int i = 0; i < info->get_upgrades_count(); i++)
					{
						vehicle_desc_t* upgrade = (vehicle_desc_t*)info->get_upgrades(i);
						if (upgrade == desc_for_display)
						{
							upgrades_from_this = true;
							break;
						}
					}
				}
				if (upgrades_from_this)
				{
					gui_upgrade_info_t* const uinfo = new gui_upgrade_info_t(info, desc_for_display);
					uinfo->set_pos(scr_coord(0, ypos));
					uinfo->set_size(scr_size(initial_upgrade_entry_width, max(uinfo->get_entry_height(), box_height)));
					upgrade_info.append(uinfo);
					cont_upgrade.add_component(uinfo);
					ypos += max(uinfo->get_entry_height(), box_height);
					amount_of_upgrades++;
				}
			}
		}
		upgrade_info.resize(amount_of_upgrades);

		// Display "no_vehicles_available" when list is empty
		if (amount_of_upgrades <= 0)
		{
			cbuffer_t buf;
			buf.clear();
			if (display_upgrade_into)
			{
				buf.append(translator::translate("no_upgrades_available"));
			}
			else
			{
				buf.append(translator::translate("no_vehicles_upgrade_to_this_vehicle"));
			}
			int box_height = LINESPACE * 3;
			gui_special_info_t* const sinfo = new gui_special_info_t(buf, MN_GREY1);
			sinfo->set_pos(scr_coord(0, ypos));
			sinfo->set_size(scr_size(initial_upgrade_entry_width, max(sinfo->get_entry_height(), box_height)));
			cont_upgrade.add_component(sinfo);
			ypos += max(sinfo->get_entry_height(), box_height);
		}
		cont_upgrade.set_size(scr_size(initial_upgrade_entry_width - 12, ypos));
	}
	display(scr_coord(0,0));
}

void vehicle_manager_t::build_desc_list()
{
	// Build the list of desc's
	int counter = 0;
	way_type = (waytype_t)selected_tab_waytype;
	page_amount_desc = 1;
	page_display_desc = 1;
	const uint16 month_now = welt->get_timeline_year_month();
	vehicle_we_own.clear();
	desc_list.clear();

	// Reset the sizes to the maximum theoretical amount.
	FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type))
	{
		counter++;
	}
	desc_list.resize(counter);
	vehicle_we_own.resize(counter);

	// If true, populate the list with all available desc's
	if (show_available_vehicles) {
		FOR(slist_tpl<vehicle_desc_t *>, const info, vehicle_builder_t::get_info(way_type)) {
			if ((!info->is_future(month_now) && !info->is_retired(month_now)) && is_desc_displayable(info)) {
				desc_list.append(info);
			}
		}
	}

	// If true, we want this particular desc to be present, even though it might not be in production
	if (goto_this_desc)
	{
		bool this_desc_exists = false;
		for (int i = 0; i < desc_list.get_count(); i++)
		{
			if (desc_list.get_element(i) == goto_this_desc)
			{
				this_desc_exists = true;
				break;
			}
		}
		if (!this_desc_exists)
		{
			desc_list.append(goto_this_desc);
		}
	}

	// Then populate the list with any additional vehicles we might own, but has not yet been populated, for instance due to being out of production.
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++) {
				vehicle_t* v = cnv->get_vehicle(veh);
				vehicle_we_own.append(v);
				if  (is_desc_displayable((vehicle_desc_t*)v->get_desc()))
				{
					desc_list.append((vehicle_desc_t*)v->get_desc());
					for (int i = 0; i < desc_list.get_count() - 1; i++) {
						if (desc_list.get_element(i) == v->get_desc()) {
							desc_list.remove_at(i);
							break;
						}
					}
				}
			}
		}
	}
	amount_desc = desc_list.get_count();
	amount_veh_owned = vehicle_we_own.get_count();
	old_count_owned_veh = desc_list.get_count();

	// since we cant have too many entries displayed at once, find out how many pages we need and set the page turn buttons visible if necessary.
	page_amount_desc = ceil((double)desc_list.get_count() / desc_pr_page);
	if (page_amount_desc > 1)
	{
		bt_desc_next_page.set_visible(true);
			bt_desc_prev_page.set_visible(true);
			lb_desc_page.set_visible(true);
	}
	else
	{
		bt_desc_next_page.set_visible(false);
		bt_desc_prev_page.set_visible(false);
		lb_desc_page.set_visible(false);
	}
	if (goto_this_desc)
	{
		desc_for_display = goto_this_desc;
		save_previously_selected_desc();
		goto_this_desc = NULL;
	}
	display_desc_list();
}

void vehicle_manager_t::save_previously_selected_desc()
{
	// When we need to remember what vehicle was selected, for instance due to resorting, this will remember it.
	old_desc_for_display = desc_for_display;
}

void vehicle_manager_t::display_desc_list()
{
	// This creates the graphical list of the desc's.
	// Start by sorting...
	sort_desc();

	// if true, locate how far down the list it is, and anticipate the page it will appear on. If we did also not just do the page turn, select the correct page to be displayed.
	int desc_at_page = 1;
	if (old_desc_for_display)
	{
		for (int i = 0; i < desc_list.get_count(); i++)
		{
			if (desc_list.get_element(i) == old_desc_for_display)
			{
				desc_at_page = ceil(((double)i + 1) / desc_pr_page);
			}
		}
	}
	if (!page_turn_desc)
	{
		page_display_desc = desc_at_page;
	}

	// count how many of each "desc" we own using an array of uint16's
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

	// Calculate how many entries we need. If multiple pages exists, locate what page is currently displayed and store the index value for the first vehicle on that page into the "offset_index"
	int i, icnv = 0;
	int offset_index = (page_display_desc * desc_pr_page) - desc_pr_page;
	icnv = min(desc_list.get_count(), desc_pr_page);
	if (icnv > desc_list.get_count() - offset_index)
	{
		icnv = desc_list.get_count() - offset_index;
	}
	int ypos = 10;
	cont_desc.remove_all();
	desc_info.clear();
	desc_info.resize(desc_pr_page);

	// Assemble the list of vehicles and preselect the previously selected vehicle if found. If there is no old desc for display, remove the desc for display
	for (i = 0; i < icnv; i++) {
		gui_desc_info_t* const dinfo = new gui_desc_info_t(desc_list.get_element(i + offset_index), desc_amounts[i + offset_index], sortby_desc, display_desc);
		dinfo->set_pos(scr_coord(0, ypos));
		dinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, max(dinfo->get_entry_height(), 40)));
		desc_info.append(dinfo);
		cont_desc.add_component(dinfo);
		ypos += max(dinfo->get_entry_height(), 40);
		if (old_desc_for_display == desc_list.get_element(i + offset_index))
		{
			selected_desc_index = i + offset_index;
			dinfo->set_selection(true);
			set_desc_scroll_position = desc_info[i]->get_pos().y - 60;
			reposition_desc_scroll = true;
		}
	}

	// Display "no_vehicles_available" when list is empty
	if (amount_desc <= 0)
	{
		cbuffer_t buf;
		buf.clear();
		buf.append(translator::translate("no_vehicles_to_display"));
		int box_height = LINESPACE * 3;
		gui_special_info_t* const sinfo = new gui_special_info_t(buf, MN_GREY1);
		sinfo->set_pos(scr_coord(0, ypos));
		sinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, max(sinfo->get_entry_height(), box_height)));
		cont_desc.add_component(sinfo);
		ypos += max(sinfo->get_entry_height(), box_height);
	}
	desc_info.set_count(icnv);
	cont_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, ypos));
	if (!page_turn_desc)
	{
		if (!old_desc_for_display)
		{
			desc_for_display = NULL;
		}
		old_desc_for_display = NULL;
	}

	// delete the amount of vehicles array
	delete[] desc_amounts;
	build_veh_list();
	build_upgrade_list();
	build_livery_list();
}

void vehicle_manager_t::build_veh_list()
{
	if (bool_veh_selection_exists)
	{
		delete[] veh_selection;
	}
	bool_veh_selection_exists = false;

	// This builds the list of vehicles that we own
	int counter = 0;
	way_type = (waytype_t)selected_tab_waytype;
	veh_list.clear();
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
			{
				vehicle_t* v = cnv->get_vehicle(veh);
				if (v->get_desc() == desc_for_display && is_veh_displayable((vehicle_t*)v))
				{
					counter++;
				}
			}
		}
	}
	veh_list.resize(counter);

	// Populate the vector with the vehicles we own that is also "for display"
	FOR(vector_tpl<convoihandle_t>, const cnv, welt->convoys()) {
		if (cnv->get_owner() == player && cnv->get_vehicle(0)->get_waytype() == way_type) {
			for (unsigned veh = 0; veh < cnv->get_vehicle_count(); veh++)
			{
				vehicle_t* v = cnv->get_vehicle(veh);
				if (v->get_desc() == desc_for_display && is_veh_displayable((vehicle_t*)v))
				{
					veh_list.append(v);
				}
			}
		}
	}
	amount_veh = veh_list.get_count();

	// create a bunch of bool's to keep track of which "veh" is selected
	veh_is_selected = false;
	veh_selection = new (std::nothrow) bool[veh_list.get_count()];
	for (int i = 0; i < veh_list.get_count(); i++)
	{
		veh_selection[i] = select_all;
	}
	if (veh_list.get_count() > 0)
	{
		veh_is_selected = select_all;
	}
	bool_veh_selection_exists = true;

	// since we cant have too many entries displayed at once, find out how many pages we need and set the page turn buttons visible if necessary.
	page_amount_veh = ceil((double)veh_list.get_count() / veh_pr_page);
	if (page_amount_veh > 1)
	{
		bt_veh_next_page.set_visible(true);
		bt_veh_prev_page.set_visible(true);
		lb_veh_page.set_visible(true);
	}
	else
	{
		bt_veh_next_page.set_visible(false);
		bt_veh_prev_page.set_visible(false);
		lb_veh_page.set_visible(false);
	}
	// Some sort- or display modes requires a reset of the text input display
	if (display_veh == displ_veh_cargo)
	{
		reset_veh_text_input_display();
	}

	display_veh_list();
}

void vehicle_manager_t::display_veh_list()
{
	// This creates the graphical list of the desc's.
	// Start by sorting...
	sort_veh();

	// Calculate how many entries we need. If multiple pages exists, locate what page is currently displayed and store the index value for the first vehicle on that page into the "offset_index"
	int i, icnv = 0;
	int offset_index = (page_display_veh * veh_pr_page) - veh_pr_page;
	icnv = min(veh_list.get_count(), veh_pr_page);
	if (icnv > veh_list.get_count() - offset_index)
	{
		icnv = veh_list.get_count() - offset_index;
	}
	int ypos = 10;
	cont_veh.remove_all();
	veh_info.clear();
	veh_list.resize(icnv);

	// Assemble the list of vehicles and preselect if needed
	for (i = 0; i < icnv; i++) {
		if (veh_list.get_element(i) != NULL)
		{
			gui_veh_info_t* const vinfo = new gui_veh_info_t(veh_list.get_element(i+ offset_index), sortby_veh, display_veh);
			vinfo->set_pos(scr_coord(0, ypos));
			vinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, vinfo->get_entry_height()));
			vinfo->set_selection(veh_selection[i + offset_index]);
			veh_info.append(vinfo);
			cont_veh.add_component(vinfo);
			ypos += vinfo->get_entry_height();
		}
	}

	// Display "no_vehicles_available" when list is empty
	if (amount_veh <= 0)
	{
		cbuffer_t buf;
		buf.clear();
		buf.append(translator::translate("no_vehicles_to_display"));
		int box_height = LINESPACE * 3;
		gui_special_info_t* const sinfo = new gui_special_info_t(buf, MN_GREY1);
		sinfo->set_pos(scr_coord(0, ypos));
		sinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, sinfo->get_entry_height()));
		cont_veh.add_component(sinfo);
		ypos += sinfo->get_entry_height();
	}
	veh_info.set_count(icnv);
	cont_veh.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, ypos));
	display(scr_coord(0, 0));
}

void vehicle_manager_t::update_veh_selection()
{
	// This builds the actual array of selected vehicles that we will show info about

	veh_is_selected = false;

	int offset_index = (page_display_veh * veh_pr_page) - veh_pr_page;
	for (int i = 0; i < veh_info.get_count(); i++)
	{
		if (veh_info.get_element(i)->is_selected())
		{
			veh_selection[i + offset_index] = true;
			veh_is_selected = true;
		}
		else
		{
			veh_selection[i + offset_index] = false;
		}
	}

	if (!veh_is_selected) // any vehicles from other places selected?
	{
		for (int i = 0; i < veh_list.get_count(); i++)
		{
			if (veh_selection[i] == true)
			{
				veh_is_selected = true;
				break;
			}
		}
	}
	display(scr_coord(0, 0));
}


uint32 vehicle_manager_t::get_rdwr_id()
{
	return magic_vehicle_manager_t+player->get_player_nr();
}
void vehicle_manager_t::rdwr(loadsave_t *file)
{
	scr_size size;
	sint32 desc_xoff, desc_yoff, veh_xoff, veh_yoff, upgrade_xoff, upgrade_yoff, livery_xoff, livery_yoff;
	if (file->is_saving()) {
		size = get_windowsize();
		desc_xoff = scrolly_desc.get_scroll_x();
		desc_yoff = scrolly_desc.get_scroll_y();
		veh_xoff = scrolly_veh.get_scroll_x();
		veh_yoff = scrolly_veh.get_scroll_y();
		upgrade_xoff = scrolly_upgrade.get_scroll_x();
		upgrade_yoff = scrolly_upgrade.get_scroll_y();
		//livery_xoff = scrolly_livery.get_scroll_x();
		//livery_yoff = scrolly_livery.get_scroll_y();
	}
	size.rdwr(file);

	file->rdwr_long(desc_xoff);
	file->rdwr_long(desc_yoff);
	file->rdwr_long(veh_xoff);
	file->rdwr_long(veh_yoff);
	file->rdwr_long(upgrade_xoff);
	file->rdwr_long(upgrade_yoff);

	// open dialogue
	if (file->is_loading()) {
		set_windowsize(size);
		resize(scr_coord(0, 0));
		scrolly_desc.set_scroll_position(desc_xoff, desc_yoff);
		scrolly_veh.set_scroll_position(veh_xoff, veh_yoff);
		scrolly_upgrade.set_scroll_position(upgrade_xoff, upgrade_yoff);
		//scrolly_livery.set_scroll_position(livery_xoff, livery_yoff);
	}
}

