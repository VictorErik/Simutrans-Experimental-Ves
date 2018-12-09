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

#include "../descriptor/intro_dates.h"


#include "karte.h"

#define VEHICLE_NAME_COLUMN_WIDTH ((D_BUTTON_WIDTH*5)+15)
#define VEHICLE_NAME_COLUMN_HEIGHT (250)
#define INFORMATION_COLUMN_HEIGHT (20*LINESPACE)
#define UPGRADE_LIST_COLUMN_WIDTH (D_BUTTON_WIDTH*4 + 19)
#define UPGRADE_LIST_COLUMN_HEIGHT (INFORMATION_COLUMN_HEIGHT - LINESPACE*2)
#define RIGHT_HAND_COLUMN (D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH + 10)
#define SCL_HEIGHT (15*LINESPACE)

static uint16 tabs_to_index_waytype[8];
static uint16 tabs_to_index_information[8];
static uint8 max_idx_waytype = 0;
static uint8 max_idx_information = 0;
static uint8 selected_tab_waytype = 0;
static uint8 selected_tab_information = 0;

bool vehicle_manager_t::desc_sortreverse = false;
bool vehicle_manager_t::veh_sortreverse = false;

const char *vehicle_manager_t::sort_text_desc[SORT_MODES_DESC] =
{
	"name",
	"notifications",
	"intro_year",
	"amount",
	"cargo_type_and_capacity",
	"speed",
	"upgrades",
	"catering_level",
	"comfort",
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
	" ",
	"intro_year",
	"amount",
	"speed",
	"catering_level",
	"comfort",
	"power",
	"tractive_effort",
	"weight",
	"axle_load",
	"runway_length"
};
const char *vehicle_manager_t::display_text_veh[DISPLAY_MODES_VEH] =
{
	" ",
	"age",
	"odometer",
	"location"
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

vehicle_manager_t::sort_mode_desc_t vehicle_manager_t::sortby_desc = by_desc_name;
vehicle_manager_t::sort_mode_veh_t vehicle_manager_t::sortby_veh = by_issue;
vehicle_manager_t::display_mode_desc_t vehicle_manager_t::display_desc = displ_desc_none;
vehicle_manager_t::display_mode_veh_t vehicle_manager_t::display_veh = displ_veh_none;

vehicle_manager_t::vehicle_manager_t(player_t *player_) :
	gui_frame_t( translator::translate("vehicle_manager"), player_),
	player(player_),
	lb_amount_desc(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_amount_veh(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_desc_page(NULL, SYSCOL_TEXT, gui_label_t::centered),
	lb_veh_page(NULL, SYSCOL_TEXT, gui_label_t::centered),
	lb_desc_sortby(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_veh_sortby(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_display_desc(NULL, SYSCOL_TEXT, gui_label_t::left),
	lb_display_veh(NULL, SYSCOL_TEXT, gui_label_t::left),
	scrolly_desc(&cont_desc),
	scrolly_veh(&cont_veh),
	scrolly_upgrade(&cont_upgrade)
{

	veh_is_selected = false;
	goto_this_desc = NULL;

	int y_pos = 5;
	int x_pos = 0;

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

	bt_select_all.init(button_t::square_state, translator::translate("preselect_all"), scr_coord(RIGHT_HAND_COLUMN, y_pos), scr_size(D_BUTTON_WIDTH * 2, D_BUTTON_HEIGHT));
	bt_select_all.add_listener(this);
	bt_select_all.set_tooltip(translator::translate("preselects_all_vehicles_in_the_list_automatically"));
	bt_select_all.pressed = false;
	select_all = bt_select_all.pressed;
	//bt_select_all.set_pos(scr_coord(10, y_pos));
	add_component(&bt_select_all);

	y_pos += D_BUTTON_HEIGHT + 6;
	x_pos = D_MARGIN_LEFT;

	// Sorting and display section
	sprintf(sortby_text, translator::translate("sort_vehicles_by:"));
	sprintf(displayby_text, translator::translate("display_vehicles_by:"));

	int label_length = max(display_calc_proportional_string_len_width(sortby_text, -1), display_calc_proportional_string_len_width(displayby_text, -1));
	int combobox_width = (VEHICLE_NAME_COLUMN_WIDTH - label_length - 15) / 2;

	int column_1 = D_MARGIN_LEFT;
	int column_2 = column_1 + label_length + 5;
	int column_3 = column_2 + combobox_width + 5;

	lb_desc_sortby.set_pos(scr_coord(column_1, y_pos));
	lb_desc_sortby.set_visible(true);
	add_component(&lb_desc_sortby);
	
	combo_sorter_desc.clear_elements();
	for (int i = 0; i < SORT_MODES_DESC; i++)
	{
		combo_sorter_desc.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(sort_text_desc[i]), SYSCOL_TEXT));
	}
	combo_sorter_desc.set_pos(scr_coord(column_2, y_pos));
	combo_sorter_desc.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_sorter_desc.set_focusable(true);
	combo_sorter_desc.set_selection(sortby_desc);
	combo_sorter_desc.set_highlight_color(1);
	combo_sorter_desc.set_max_size(scr_size(combobox_width, LINESPACE * 5 + 2 + 16));
	combo_sorter_desc.add_listener(this);
	add_component(&combo_sorter_desc);
	
	bt_desc_sortreverse.init(button_t::square_state, translator::translate("reverse_sort_order"), scr_coord(column_3, y_pos), scr_size(combobox_width, D_BUTTON_HEIGHT));
	bt_desc_sortreverse.add_listener(this);
	bt_desc_sortreverse.set_tooltip(translator::translate("tick_to_reverse_the_sort_order_of_the_list"));
	bt_desc_sortreverse.pressed = false;
	desc_sortreverse = bt_desc_sortreverse.pressed;
	add_component(&bt_desc_sortreverse);

	y_pos += D_BUTTON_HEIGHT;

	lb_display_desc.set_pos(scr_coord(column_1, y_pos));
	lb_display_desc.set_visible(true);
	add_component(&lb_display_desc);

	// "Display only" selection field
	combo_display_desc.clear_elements();
	for (int i = 0; i < DISPLAY_MODES_DESC; i++)
	{
		combo_display_desc.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(display_text_desc[i]), SYSCOL_TEXT));
	}
	combo_display_desc.set_pos(scr_coord(column_2, y_pos));
	combo_display_desc.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_display_desc.set_focusable(true);
	combo_display_desc.set_selection(display_desc);
	combo_display_desc.set_highlight_color(1);
	combo_display_desc.set_max_size(scr_size(combobox_width, LINESPACE * 5 + 2 + 16));
	combo_display_desc.add_listener(this);
	add_component(&combo_display_desc);

	ti_desc_display.set_pos(scr_coord(column_3,y_pos));
	ti_desc_display.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	ti_desc_display.set_visible(false);
	ti_desc_display.add_listener(this);
	add_component(&ti_desc_display);

	y_pos -= D_BUTTON_HEIGHT;
	column_1 += RIGHT_HAND_COLUMN;
	column_2 += RIGHT_HAND_COLUMN;
	column_3 += RIGHT_HAND_COLUMN;

	lb_veh_sortby.set_pos(scr_coord(column_1, y_pos));
	lb_veh_sortby.set_size(D_BUTTON_SIZE);
	lb_veh_sortby.set_visible(true);
	add_component(&lb_veh_sortby);

	combo_sorter_veh.clear_elements();
	for (int i = 0; i < SORT_MODES_VEH; i++)
	{
		combo_sorter_veh.append_element(new gui_scrolled_list_t::const_text_scrollitem_t(translator::translate(sort_text_veh[i]), SYSCOL_TEXT));
	}
	combo_sorter_veh.set_pos(scr_coord(column_2, y_pos));
	combo_sorter_veh.set_size(scr_size(combobox_width, D_BUTTON_HEIGHT));
	combo_sorter_veh.set_focusable(true);
	combo_sorter_veh.set_selection(sortby_veh);
	combo_sorter_veh.set_highlight_color(1);
	combo_sorter_veh.set_max_size(scr_size(combobox_width, LINESPACE * 5 + 2 + 16));
	combo_sorter_veh.add_listener(this);
	add_component(&combo_sorter_veh);
	
	bt_veh_sortreverse.init(button_t::square_state, translator::translate("reverse_sort_order"), scr_coord(column_3, y_pos), scr_size(combobox_width, D_BUTTON_HEIGHT));
	bt_veh_sortreverse.add_listener(this);
	bt_veh_sortreverse.set_tooltip(translator::translate("tick_to_reverse_the_sort_order_of_the_list"));
	bt_veh_sortreverse.pressed = false;
	veh_sortreverse = bt_veh_sortreverse.pressed;
	add_component(&bt_veh_sortreverse);

	y_pos += D_BUTTON_HEIGHT;

	lb_display_veh.set_pos(scr_coord(column_1, y_pos));
	lb_display_veh.set_visible(true);
	add_component(&lb_display_veh);





	y_pos += D_BUTTON_HEIGHT * 2;
	scrolly_desc_pos = scr_coord(D_MARGIN_LEFT, y_pos);
	

	// Vehicle desc list
	cont_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	scrolly_desc.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	scrolly_desc.set_show_scroll_y(true);
	scrolly_desc.set_scroll_amount_y(40);
	scrolly_desc.set_visible(true);
	scrolly_desc.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	add_component(&scrolly_desc);


	// Vehicle list
	cont_veh.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH, VEHICLE_NAME_COLUMN_HEIGHT));
	scrolly_veh.set_pos(scr_coord(RIGHT_HAND_COLUMN, y_pos));
	scrolly_veh.set_show_scroll_y(true);
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
	bt_desc_prev_page.set_visible(false);
	add_component(&bt_desc_prev_page);

	lb_desc_page.set_pos(scr_coord(D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH - 90, y_pos));
	lb_desc_page.set_visible(false);
	add_component(&lb_desc_page);

	bt_desc_next_page.init(button_t::arrowright, NULL, scr_coord(D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH - gui_theme_t::gui_arrow_right_size.w, y_pos));
	bt_desc_next_page.add_listener(this);
	bt_desc_next_page.set_tooltip(translator::translate("next_page"));
	bt_desc_next_page.set_visible(false);
	add_component(&bt_desc_next_page);

	bt_veh_prev_page.init(button_t::arrowleft, NULL, scr_coord(RIGHT_HAND_COLUMN + VEHICLE_NAME_COLUMN_WIDTH - 110, y_pos));
	bt_veh_prev_page.add_listener(this);
	bt_veh_prev_page.set_tooltip(translator::translate("previous_page"));
	bt_veh_prev_page.set_visible(false);
	add_component(&bt_veh_prev_page);

	lb_veh_page.set_pos(scr_coord(RIGHT_HAND_COLUMN + VEHICLE_NAME_COLUMN_WIDTH - 90, y_pos));
	lb_veh_page.set_visible(false);
	add_component(&lb_veh_page);

	bt_veh_next_page.init(button_t::arrowright, NULL, scr_coord(RIGHT_HAND_COLUMN + VEHICLE_NAME_COLUMN_WIDTH - gui_theme_t::gui_arrow_right_size.w, y_pos));
	bt_veh_next_page.add_listener(this);
	bt_veh_next_page.set_tooltip(translator::translate("next_page"));
	bt_veh_next_page.set_visible(false);
	add_component(&bt_veh_next_page);
		

	y_pos += D_BUTTON_HEIGHT;

	// information tab panel
	tabs_info.set_pos(scr_coord(D_MARGIN_LEFT, y_pos));
	tabs_info.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH*2, SCL_HEIGHT));
	
	// general information
	tabs_info.add_tab(&dummy, translator::translate("general_information"));
	tabs_to_index_information[max_idx_information++] = 0;

	// maintenance information
	tabs_info.add_tab(&cont_maintenance_info, translator::translate("maintenance_information"));
	tabs_to_index_information[max_idx_information++] = 1;
	
	// details
	tabs_info.add_tab(&dummy, translator::translate("details"));
	tabs_to_index_information[max_idx_information++] = 2;

	selected_tab_information = tabs_to_index_information[tabs_info.get_active_tab_index()];
	tabs_info.add_listener(this);
	add_component(&tabs_info);

	y_pos += D_BUTTON_HEIGHT*2 + LINESPACE;

	desc_info_text_pos = scr_coord(D_MARGIN_LEFT, y_pos);
	selected_desc_index = -1;
	selected_upgrade_index = -1;
/*
	cont_desc_info.set_pos(scr_coord(desc_info_text_ypos));
	cont_desc_info.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH * 2, INFORMATION_COLUMN_HEIGHT));*/

	scr_coord dummy(D_MARGIN_LEFT, D_MARGIN_TOP);

	// The information tabs have objects attached to some containers.

	// Maintenance tab:
	{
		bt_upgrade.init(button_t::roundbox, translator::translate("upgrade"), dummy, D_BUTTON_SIZE);
		bt_upgrade.add_listener(this);
		bt_upgrade.set_tooltip(translator::translate("upgrade"));
		bt_upgrade.set_visible(false);
		cont_maintenance_info.add_component(&bt_upgrade);

		bt_upgrade_to_from.init(button_t::roundbox, NULL, dummy, scr_size(D_BUTTON_HEIGHT,D_BUTTON_HEIGHT));
		bt_upgrade_to_from.add_listener(this);
		bt_upgrade_to_from.set_tooltip(translator::translate("press_to_switch_between_upgrade_to_or_upgrade_from"));
		bt_upgrade_to_from.set_visible(false);
		cont_maintenance_info.add_component(&bt_upgrade_to_from);

		display_upgrade_into = true;

		// Upgrade list
		cont_upgrade.set_size(scr_size(UPGRADE_LIST_COLUMN_WIDTH, UPGRADE_LIST_COLUMN_HEIGHT));
		scrolly_upgrade.set_show_scroll_y(true);
		scrolly_upgrade.set_scroll_amount_y(40);
		scrolly_upgrade.set_visible(false);
		scrolly_upgrade.set_size(scr_size(UPGRADE_LIST_COLUMN_WIDTH, UPGRADE_LIST_COLUMN_HEIGHT));
		cont_maintenance_info.add_component(&scrolly_upgrade);

	}
	reset_desc_text_input_display();
	set_desc_display_rules();
	build_desc_list();

	// Sizes:
	const int min_width = (VEHICLE_NAME_COLUMN_WIDTH * 2) + (D_MARGIN_LEFT*3);
	const int min_height = tabs_info.get_pos().y + D_BUTTON_HEIGHT*2 + LINESPACE + INFORMATION_COLUMN_HEIGHT;

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
	for (int i = 0; i <upgrade_info.get_count(); i++)
	{
		delete[] upgrade_info.get_element(i);
	}
	if (veh_is_selected)
	{
		delete[] veh_selection;
	}
}

bool vehicle_manager_t::action_triggered(gui_action_creator_t *comp, value_t v)           // 28-Dec-01    Markus Weber    Added
{
	if (comp == &tabs_waytype) {
		int const tab = tabs_waytype.get_active_tab_index();
		uint8 old_selected_tab = selected_tab_waytype;
		selected_tab_waytype = tabs_to_index_waytype[tab];
		build_desc_list();
		page_turn_desc = false;
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
		save_previously_selected_desc();
		page_turn_desc = false;
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

	if (comp == &combo_sorter_desc) {
		sint32 sort_mode = combo_sorter_desc.get_selection();
		if (sort_mode < 0)
		{
			combo_sorter_desc.set_selection(0);
			sort_mode = 0;
		}
		sortby_desc = (sort_mode_desc_t)sort_mode;
		save_previously_selected_desc();
		page_turn_desc = false;
		display_desc_list();
	}

	if (comp == &combo_display_desc) {
		sint32 display_mode = combo_display_desc.get_selection();
		if (display_mode < 0)
		{
			combo_display_desc.set_selection(0);
			display_mode = 0;
		}
		display_desc = (display_mode_desc_t)display_mode;
		save_previously_selected_desc();
		page_turn_desc = false; 
		reset_desc_text_input_display();
		set_desc_display_rules();
		build_desc_list();
	}

	if (comp == &ti_desc_display) {
		update_desc_text_input_display();
		build_desc_list();
	}
	

	if (comp == &combo_sorter_veh) {
		sint32 sort_mode = combo_sorter_veh.get_selection();
		if (sort_mode < 0)
		{
			combo_sorter_veh.set_selection(0);
			sort_mode = 0;
		}
		sortby_veh = (sort_mode_veh_t)sort_mode;

		// Because we cant remember what vehicles we had selected when sorting, reset the selection to whatever the select all button says
		for (int i = 0; i < veh_list.get_count(); i++)
		{
			veh_selection[i] = select_all;
		}

		if (select_all)
		{
			old_count_veh_selection = veh_list.get_count();
		}
		else
		{
			old_count_veh_selection = 0;
		}

		display_veh_list();
	}

	if (comp == &bt_desc_sortreverse)
	{
		bt_desc_sortreverse.pressed = !bt_desc_sortreverse.pressed;
		desc_sortreverse = bt_desc_sortreverse.pressed;
		save_previously_selected_desc();
		page_turn_desc = false;
		display_desc_list();
	}

	if (comp == &bt_veh_sortreverse)
	{
		bt_veh_sortreverse.pressed = !bt_veh_sortreverse.pressed;
		veh_sortreverse = bt_veh_sortreverse.pressed;
		
		// Because we cant remember what vehicles we had selected when sorting, reset the selection to whatever the select all button says
		for (int i = 0; i < veh_list.get_count(); i++)
		{
			veh_selection[i] = select_all;
		}

		if (select_all)
		{
			old_count_veh_selection = veh_list.get_count();
		}
		else
		{
			old_count_veh_selection = 0;
		}
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
		page_turn_desc = true;
		save_previously_selected_desc();
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
		page_turn_desc = true;
		save_previously_selected_desc();
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
		display_veh_list();
	}

	if (comp == &bt_upgrade_to_from)
	{
		display_upgrade_into = !display_upgrade_into;

		if (display_upgrade_into)
		{

		}
		else
		{

		}
		build_upgrade_list();
	}
	return true;
}

void vehicle_manager_t::reset_desc_text_input_display()
{
	char default_display[64];
	ti_desc_display.set_visible(false);
	if (display_desc != displ_desc_none)
	{
		ti_desc_display.set_visible(true);
	}
	ti_desc_display.set_color(SYSCOL_TEXT_HIGHLIGHT);

	if (invalid_entry_form)
	{
		sprintf(default_display, translator::translate("invalid_entry"));
		ti_desc_display.set_color(COL_RED);
		invalid_entry_form = false;
	}
	else
	{
		switch (display_desc)
		{
		case displ_desc_intro_year:
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
			sprintf(default_display, "> 0");
			break;
		default:
			sprintf(default_display, "");
			break;
		}
	}
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
	char entry[50] = { 0 };
	sprintf(entry, ti_desc_display.get_text());
	char c[1] = { 'a' }; // need to give c some value, since the forloop depends on it. The strings returned from the sorter will always have a number as its first value, so we should be safe

	// For any type of number input, this is the syntax:
	// First logic: ">" or "<" (if specified)
	// First value: "number"
	// Second logic: "-" (if specified)
	// Second value: "number" (if specified)

	ch_first_logic[0] = 0;
	ch_second_logic[0] = 0;
	for (int i = 0; i < 10; i++)
	{
		ch_first_value[i] = 0;
		ch_second_value[i] = 0;
	}

	first_logic_exists = false;
	second_logic_exists = false;
	first_value_exists = false;
	second_value_exists = false;
	no_logics = false;
	invalid_entry_form = false;

	desc_display_first_value = 0;
	desc_display_second_value = 0;

	int first_value_offset = 0;
	int secon_value_offset = 0;

	for (int j = 0; *c != '\0'; j++)
	{
		*c = entry[j];
		if ((*c == '>' || *c == '<') && !first_logic_exists && !no_logics)
		{
			ch_first_logic[0] = *c;
			first_logic_exists = true;
		}
		else if (!second_logic_exists && (*c == '0' || *c == '1' || *c == '2' || *c == '3' || *c == '4' || *c == '5' || *c == '6' || *c == '7' || *c == '8' || *c == '9'))
		{
			ch_first_value[first_value_offset] = *c;
			first_value_exists = true; 
			no_logics = true;
			first_value_offset++;
		}
		else if ((*c == '-') && first_value_exists && !second_logic_exists)
		{
			ch_second_logic[0] = *c;
			second_logic_exists = true;
		}
		else if (second_logic_exists && (*c == '0' || *c == '1' || *c == '2' || *c == '3' || *c == '4' || *c == '5' || *c == '6' || *c == '7' || *c == '8' || *c == '9'))
		{
			ch_second_value[secon_value_offset] = *c;
			second_value_exists = true;
			secon_value_offset++;
		}
		else if (*c == ' ' || *c == '\0')
		{
			//ignore spaces
		}
		else
		{
			invalid_entry_form = true;
			break;
		}
		
	}
	if (invalid_entry_form)
	{
		reset_desc_text_input_display();
	}
	else
	{
		if (first_value_exists)
		{
			desc_display_first_value = std::atoi(ch_first_value);
		}
		if (second_value_exists)
		{
			desc_display_second_value = std::atoi(ch_second_value);
		}
	}
}

bool vehicle_manager_t::is_desc_displayable(vehicle_desc_t *desc)
{
	bool display = false;
	if (display_desc == displ_desc_none)
	{
		display = true;
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
			value_to_compare = desc->get_comfort();
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

		if ((first_value_exists) && !first_logic_exists && !second_logic_exists && !second_value_exists) // only a value is specified
		{
			display = value_to_compare == desc_display_first_value;
		}
		else if ((first_logic_exists && first_value_exists) && !second_logic_exists && !second_value_exists) // greater than, or smaller than specified value
		{
			if (ch_first_logic[0] == '>')
			{
				display = value_to_compare >= desc_display_first_value;
			}
			else
			{
				display = value_to_compare <= desc_display_first_value;
			}
		}
		else if ((first_value_exists && second_logic_exists && second_value_exists) && !first_logic_exists) // greater than lowest the lowest and smaller than highest value specified
		{
			display = (value_to_compare >= desc_display_first_value) && (value_to_compare <= desc_display_second_value);
		}
		else
		{
			display = true;
		}
	}
	return display;
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

	bt_upgrade_to_from.set_visible(false);
	scrolly_upgrade.set_visible(false);

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

		// Upgrade button
		bt_upgrade.set_pos(scr_coord(pos_x, pos_y));

		pos_x = column_3;
		pos_y = 0;


		// Switch upgrade mode button
		bt_upgrade_to_from.set_pos(scr_coord(pos_x, pos_y + (LINESPACE / 4)));
		bt_upgrade_to_from.set_visible(true);
		scrolly_upgrade.set_visible(true);

		// Upgrade information			
		if (display_upgrade_into)
		{
			sprintf(buf, "%s:", translator::translate("this_vehicle_can_upgrade_to"));
		}
		else
		{
			sprintf(buf, "%s:", translator::translate("this_vehicle_can_upgrade_from"));
		}
		display_proportional_clip(pos.x + pos_x + bt_upgrade_to_from.get_size().w + 5, pos.y + pos_y, buf, ALIGN_LEFT, SYSCOL_TEXT, true);
		pos_y += LINESPACE * 2;

		scrolly_upgrade.set_pos(scr_coord(D_MARGIN_LEFT + pos_x, pos_y));
		if (amount_of_upgrades <= 0)
		{
			pos_y += LINESPACE;
			sprintf(buf, "%s", translator::translate("keine"));
			display_proportional_clip(pos.x + pos_x, pos.y + pos_y, buf, ALIGN_LEFT, SYSCOL_TEXT, true);
			pos_y += LINESPACE;
		}

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
	if (info_display == 0)
	{
		draw_general_information(pos + desc_info_text_pos);
	}
	else if (info_display == 1)
	{
		if (update_veh_list)
		{
			build_upgrade_list();
		}
		draw_maintenance_information(pos + desc_info_text_pos);
	}
	else if (info_display == 2)
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
}


void vehicle_manager_t::set_windowsize(scr_size size)
{
	gui_frame_t::set_windowsize(size);
}

void vehicle_manager_t::update_tabs()
{
	// tab panel
	tabs_waytype.set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 11 - 4, SCL_HEIGHT));
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
			tabs_waytype.set_active_tab_index(0);
		}
	}
	selected_tab_waytype = tabs_to_index_waytype[tabs_waytype.get_active_tab_index()];
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
			if (!info->is_future(month_now) && (!info->is_retired(month_now)))
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
				if ((veh2->get_total_capacity() == 0 || veh1->get_total_capacity() == 0)&& desc_sortreverse)
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

	case by_desc_upgrades_available:
		// find a way to easily count the current number of available upgrades
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

void vehicle_manager_t::build_upgrade_list()
{
	cont_upgrade.remove_all();
	upgrade_info.clear();
	int ypos = 10;
	amount_of_upgrades = 0;
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
							uinfo->set_size(scr_size(UPGRADE_LIST_COLUMN_WIDTH - 12, max(uinfo->get_image_height(), 40)));
							upgrade_info.append(uinfo);
							cont_upgrade.add_component(uinfo);
							ypos += max(uinfo->get_image_height(), 40);
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
					uinfo->set_size(scr_size(UPGRADE_LIST_COLUMN_WIDTH - 12, max(uinfo->get_image_height(), 40)));
					upgrade_info.append(uinfo);
					cont_upgrade.add_component(uinfo);
					ypos += max(uinfo->get_image_height(), 40);
					amount_of_upgrades++;
				}
			}
			upgrade_info.resize(amount_of_upgrades);
		}
		cont_upgrade.set_size(scr_size(UPGRADE_LIST_COLUMN_WIDTH - 12, ypos));
	}

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
		dinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, max(dinfo->get_image_height(), 40)));
		desc_info.append(dinfo);
		cont_desc.add_component(dinfo);
		ypos += max(dinfo->get_image_height(), 40);
		if (old_desc_for_display == desc_list.get_element(i + offset_index))
		{
			selected_desc_index = i + offset_index;
			dinfo->set_selection(true);
			set_desc_scroll_position = desc_info[i]->get_pos().y - 60;
			reposition_desc_scroll = true;
		}
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
				if (v->get_desc() == desc_for_display)
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
				if (v->get_desc() == desc_for_display)
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
			gui_veh_info_t* const vinfo = new gui_veh_info_t(veh_list.get_element(i+ offset_index));
			vinfo->set_pos(scr_coord(0, ypos));
			vinfo->set_size(scr_size(VEHICLE_NAME_COLUMN_WIDTH - 12, max(vinfo->get_image_height(), 50)));
			vinfo->set_selection(veh_selection[i + offset_index]);
			veh_info.append(vinfo);
			cont_veh.add_component(vinfo);
			ypos += max(vinfo->get_image_height(), 50);
		}
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

// Here we model up each entries in the lists:
// We start with the upgrade entries, ie what desc a particular vehicle can upgrade to:

// ***************************************** //
// Upgrade entries:
// ***************************************** //
gui_upgrade_info_t::gui_upgrade_info_t(vehicle_desc_t* upgrade_, vehicle_desc_t* existing_)
{
	this->upgrade = upgrade_;
	existing = existing_;
	player_nr = welt->get_active_player_nr();
	draw(scr_coord(0, 0));
}

/**
* Events werden hiermit an die GUI-components
* gemeldet
* @author Hj. Malthaner
*/
bool gui_upgrade_info_t::infowin_event(const event_t *ev)
{
	if (IS_LEFTRELEASE(ev)) {
		selected = !selected;
		return true;
	}
	else if (IS_RIGHTRELEASE(ev)) {
		open_info = true;
		return true;
	}

	return false;
}


/**
* Draw the component
* @author Hj. Malthaner
*/
void gui_upgrade_info_t::draw(scr_coord offset)
{
	clip_dimension clip = display_get_clip_wh();
	if (!((pos.y + offset.y) > clip.yy || (pos.y + offset.y) < clip.y - 32)) {
		// Name colors:
		// Black:		ok!
		// Dark blue:	obsolete
		// Pink:		Can upgrade

		int max_caracters = D_BUTTON_WIDTH / 5; // each character is ca 5 pixels wide.
		int look_for_spaces_or_separators = 5;
		char name[256] = "\0"; // Translated name of the vehicle. Will not be modified
		char name_display[256] = "\0"; // The string that will be sent to the screen
		int y_pos = 0;
		int x_pos = 0;
		int suitable_break;
		int used_caracters = 0;
		image_height = 0;
		COLOR_VAL text_color = COL_BLACK;
		const uint16 month_now = welt->get_timeline_year_month();
		bool upgrades = false;
		bool retired = false;
		bool only_as_upgrade = false;
		int fillbox_height = 4 * LINESPACE;
		bool display_bakground = false;

		sprintf(name, translator::translate(upgrade->get_name()));
		sprintf(name_display, name);

		// First, we need to know how much text is going to be, since the "selected" blue field has to be drawn before all the text
		// Upgrades. We only want to show that a vehicle can upgrade if we own it


		if (upgrade->is_retired(month_now)) {
			retired = true;
			fillbox_height += LINESPACE;
		}


		scr_coord_val x, y, w, h;
		const image_id image = upgrade->get_base_image();
		display_get_base_image_offset(image, &x, &y, &w, &h);
		const int xoff = 0;
		int left = pos.x + offset.x + xoff + 4;


		if (selected)
		{
			display_fillbox_wh_clip(offset.x + pos.x, offset.y + pos.y + 1, VEHICLE_NAME_COLUMN_WIDTH, max(max(h, fillbox_height), 40) - 2, COL_DARK_BLUE, true);
			text_color = COL_WHITE;
		}


		display_base_img(image, left - x, pos.y + offset.y + 21 - y - h / 2, player_nr, false, true);

		int image_offset = min(w + 10,D_BUTTON_WIDTH);
		if (w > image_offset)
		{
			display_bakground = true;
		}
		if (display_bakground)
		{
			if (selected)
			{
				display_blend_wh(offset.x + pos.x + image_offset, pos.y + offset.y + 6 + y_pos, VEHICLE_NAME_COLUMN_WIDTH - image_offset, LINESPACE * 3, COL_DARK_BLUE, 50);
			}
			else
			{
				display_blend_wh(offset.x + pos.x + image_offset, pos.y + offset.y + 6 + y_pos, VEHICLE_NAME_COLUMN_WIDTH - image_offset, LINESPACE * 3, MN_GREY4, 50);
			}
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
					display_proportional_clip(pos.x + offset.x + image_offset, pos.y + offset.y + 6 + y_pos, name_display, ALIGN_LEFT, text_color, true);
					y_pos += LINESPACE;
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

					display_proportional_clip(pos.x + offset.x + image_offset, pos.y + offset.y + 6 + y_pos, name_display, ALIGN_LEFT, text_color, true);

					// Reset the string
					name_display[0] = '\0';
					for (int j = 0; j < 256; j++)
					{
						name_display[j] = name[used_caracters + j];
					}
				}
				y_pos += LINESPACE;
			}
		}
		else
		{ // Ok, name is short enough to fit one line
			display_proportional_clip(pos.x + offset.x + image_offset, pos.y + offset.y + 6 + y_pos, name, ALIGN_LEFT, text_color, true);
		}

		y_pos = 0;
		x_pos = UPGRADE_LIST_COLUMN_WIDTH - 14;

		// Byggår
		char year[20];
		sprintf(year, "%s: %u", translator::translate("available_until"), upgrade->get_retire_year_month() / 12);
		display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + 6 + y_pos, year, ALIGN_RIGHT, text_color, true);
		y_pos += LINESPACE;

		// Following section compares different values between the old and the new vehicle, to see what is upgraded

		COLOR_VAL difference_color = COL_DARK_GREEN;
		char tmp[50];
		// Load
		if (upgrade->get_total_capacity() != existing->get_total_capacity())
		{
			char extra_pass[8];

			int difference = upgrade->get_total_capacity() - existing->get_total_capacity();
			if (difference > 0)
			{
				sprintf(tmp, "+%i", difference);
				difference_color = COL_DARK_GREEN;
			}
			else
			{
				sprintf(tmp, "%i", difference);
				difference_color = COL_RED;
			}

			int entry = display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + 6 + y_pos, tmp, ALIGN_RIGHT, difference_color, true);
			if (upgrade->get_overcrowded_capacity() > 0)
			{
				sprintf(extra_pass, " (%i)", upgrade->get_overcrowded_capacity());
			}
			else
			{
				extra_pass[0] = '\0';
			}

			sprintf(tmp, "%i%s%s %s ", upgrade->get_total_capacity(), extra_pass, translator::translate(upgrade->get_freight_type()->get_mass()),
				upgrade->get_freight_type()->get_catg() == 0 ? translator::translate(upgrade->get_freight_type()->get_name()) : translator::translate(upgrade->get_freight_type()->get_catg_name()));

			entry += display_proportional_clip(pos.x + offset.x + x_pos - entry, pos.y + offset.y + 6 + y_pos, tmp, ALIGN_RIGHT, text_color, true);		

			if (upgrade->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_PAS)
			{
				display_color_img(skinverwaltung_t::passengers->get_image_id(0), pos.x + offset.x + 2 + x_pos - entry - 16, pos.y + offset.y + 7 + y_pos, 0, false, false);
			}
			else if (upgrade->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_MAIL)
			{
				display_color_img(skinverwaltung_t::mail->get_image_id(0), pos.x + offset.x + 2 + x_pos - entry - 16, pos.y + offset.y + 7 + y_pos, 0, false, false);
			}
			else
			{
				display_color_img(skinverwaltung_t::goods->get_image_id(0), pos.x + offset.x + 2 + x_pos - entry - 16, pos.y + offset.y + 7 + y_pos, 0, false, false);
			}

			y_pos += LINESPACE;
		}

		// top speed
		if (upgrade->get_topspeed() != existing->get_topspeed())
		{
			int difference = upgrade->get_topspeed() - existing->get_topspeed();
			if (difference > 0)
			{
				sprintf(tmp, "+%i", difference);
				difference_color = COL_DARK_GREEN;
			}
			else
			{
				sprintf(tmp, "%i", difference);
				difference_color = COL_RED;
			}
			int entry = display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + 6 + y_pos, tmp, ALIGN_RIGHT, difference_color, true);
			sprintf(tmp, translator::translate("top_speed: %i km/h "), upgrade->get_topspeed());
			display_proportional_clip(pos.x + offset.x + x_pos - entry, pos.y + offset.y + 6 + y_pos, tmp, ALIGN_RIGHT, text_color, true);
		}
		// weight
		// loading time
		// brake force
		// power
		// tractive effort



		// Issues

		COLOR_VAL issue_color = COL_BLACK;
		char issues[100] = "\0";

		if (retired)
		{
			if (upgrade->get_running_cost(welt) > upgrade->get_running_cost())
			{
				sprintf(issues, "%s", translator::translate("obsolete"));
				issue_color = selected ? text_color : COL_DARK_BLUE;
			}
			else
			{
				sprintf(issues, "%s", translator::translate("out_of_production"));
				issue_color = selected ? text_color : SYSCOL_EDIT_TEXT_DISABLED;
			}
			display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + 6 + y_pos, issues, ALIGN_LEFT, issue_color, true);
			y_pos += LINESPACE;
		}

		if (only_as_upgrade)
		{
			sprintf(issues, "%s", translator::translate("only_as_upgrade"));
			issue_color = selected ? text_color : SYSCOL_EDIT_TEXT_DISABLED;
			display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + 6 + y_pos, issues, ALIGN_LEFT, issue_color, true);
			y_pos += LINESPACE;
		}




		y_pos += LINESPACE * 2;
		image_height = max(h, y_pos);

	}
}




// Here we model up each entries in the lists:
// We start with the vehicle_desc entries, ie vehicle models:


// ***************************************** //
// "Desc" entries:
// ***************************************** //
gui_desc_info_t::gui_desc_info_t(vehicle_desc_t* veh, uint16 vehicleamount, int sortmode_index, int displaymode_index)
{
	this->veh = veh;
	amount = vehicleamount;
	player_nr = welt->get_active_player_nr();
	sort_mode = sortmode_index;
	display_mode = displaymode_index;

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
			selected = !selected;
			//selected = true;
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
		const uint16 month_now = welt->get_timeline_year_month();
		bool upgrades = false;
		bool retired = false;
		bool only_as_upgrade = false;
		int window_height = 6 * LINESPACE; // minimum size of the entry

		sprintf(name, translator::translate(veh->get_name()));
		sprintf(name_display, name);

		// First, we need to know how much text is going to be, since the "selected" blue field has to be drawn before all the text
		// Upgrades: If uncommented, will only show entry if we own it
		if (veh->get_upgrades_count() > 0/* && amount > 0*/)
		{
			for (int i = 0; i < veh->get_upgrades_count(); i++)
			{
				if (veh->get_upgrades(i)) {
					if (!veh->get_upgrades(i)->is_future(month_now) && (!veh->get_upgrades(i)->is_retired(month_now))) {
						upgrades = true;
						window_height += LINESPACE;
						break;
					}
				}
			}
		}

		if (veh->is_retired(month_now)) {
			retired = true;
			window_height += LINESPACE;
		}

		if (veh->is_available_only_as_upgrade())
		{
			only_as_upgrade = true;
			window_height += LINESPACE;
		}

		scr_coord_val x, y, w, h;
		const image_id image = veh->get_base_image();
		display_get_base_image_offset(image, &x, &y, &w, &h);
		if (h > window_height)
		{
			window_height = h;
		}
		if (selected)
		{
			display_fillbox_wh_clip(offset.x + pos.x, offset.y + pos.y + 1, VEHICLE_NAME_COLUMN_WIDTH, window_height - 2, COL_DARK_BLUE, true);
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
		char year[20];
		if (veh->get_retire_year_month() != DEFAULT_RETIRE_DATE * 12)
		{
			sprintf(year, "%s: %u-%u", translator::translate("available"), veh->get_intro_year_month() / 12, veh->get_retire_year_month() / 12);
		}
		else
		{
			sprintf(year, "%s: %u", translator::translate("available_from"), veh->get_intro_year_month() / 12);
		}
		display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, year, ALIGN_RIGHT, text_color, true);
		ypos_name += LINESPACE;

		// Antal
		char amount_of_vehicles[10];
		sprintf(amount_of_vehicles, "%s: %u", translator::translate("amount"), amount);
		display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, amount_of_vehicles, ALIGN_RIGHT, text_color, true);
		ypos_name += LINESPACE;

		// Load
		if (veh->get_total_capacity() > 0)
		{
			char load[20];
			//sprintf(load, ": %u", veh->get_total_capacity());
			char extra_pass[8];
			if (veh->get_overcrowded_capacity() > 0)
			{
				sprintf(extra_pass, " (%i)", veh->get_overcrowded_capacity());
			}
			else
			{
				extra_pass[0] = '\0';
			}

			sprintf(load, "%i%s%s %s\n", veh->get_total_capacity(), extra_pass, translator::translate(veh->get_freight_type()->get_mass()),
				veh->get_freight_type()->get_catg() == 0 ? translator::translate(veh->get_freight_type()->get_name()) : translator::translate(veh->get_freight_type()->get_catg_name()));

			int entry = display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, load, ALIGN_RIGHT, text_color, true);

			if (veh->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_PAS)
			{
				display_color_img(skinverwaltung_t::passengers->get_image_id(0), pos.x + offset.x + 2 + xpos_extra - entry - 16, pos.y + offset.y + 7 + ypos_name, 0, false, false);
			}
			else if (veh->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_MAIL)
			{
				display_color_img(skinverwaltung_t::mail->get_image_id(0), pos.x + offset.x + 2 + xpos_extra - entry - 16, pos.y + offset.y + 7 + ypos_name, 0, false, false);
			}
			else
			{
				display_color_img(skinverwaltung_t::goods->get_image_id(0), pos.x + offset.x + 2 + xpos_extra - entry - 16, pos.y + offset.y + 7 + ypos_name, 0, false, false);
			}

			ypos_name += LINESPACE;
		}
		// Issues
		{
			COLOR_VAL issue_color = COL_BLACK;
			char issues[100] = "\0";

			if (upgrades) {
				sprintf(issues, "%s", translator::translate("upgrades_available"));
				issue_color = selected ? text_color : COL_PURPLE;
				display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, issues, ALIGN_RIGHT, issue_color, true);
				ypos_name += LINESPACE;

			}

			if (retired) {
				if (veh->get_running_cost(welt) > veh->get_running_cost())
				{
					sprintf(issues, "%s", translator::translate("obsolete"));
					issue_color = selected ? text_color : COL_DARK_BLUE;
				}
				else
				{
					sprintf(issues, "%s", translator::translate("out_of_production"));
					issue_color = selected ? text_color : SYSCOL_EDIT_TEXT_DISABLED;
				}
				display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, issues, ALIGN_RIGHT, issue_color, true);
				ypos_name += LINESPACE;
			}

			if (only_as_upgrade) {
				sprintf(issues, "%s", translator::translate("only_as_upgrade"));
				issue_color = selected ? text_color : SYSCOL_EDIT_TEXT_DISABLED;
				display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, issues, ALIGN_RIGHT, issue_color, true);
				ypos_name += LINESPACE;
			}
		}

		// Sort- and display mode dependent text:
		char sort_mode_text[100];
		if (sort_mode == vehicle_manager_t::sort_mode_desc_t::by_desc_axle_load || display_mode == vehicle_manager_t::display_mode_desc_t::displ_desc_axle_load)
		{
			sprintf(sort_mode_text, translator::translate("axle_load: %ut"), veh->get_axle_load());
			display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, sort_mode_text, ALIGN_RIGHT, text_color, true);
			ypos_name += LINESPACE;
		}
		if (sort_mode == vehicle_manager_t::sort_mode_desc_t::by_desc_catering_level || display_mode == vehicle_manager_t::display_mode_desc_t::displ_desc_catering_level)
		{
			if (veh->get_catering_level() > 0)
			{
				sprintf(sort_mode_text, translator::translate("catering_level: %u"), veh->get_catering_level());
			}
			else
			{
				sprintf(sort_mode_text, translator::translate("no_catering"));
			}
			display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, sort_mode_text, ALIGN_RIGHT, text_color, true);
			ypos_name += LINESPACE;
		}
		if (sort_mode == vehicle_manager_t::sort_mode_desc_t::by_desc_comfort || display_mode == vehicle_manager_t::display_mode_desc_t::displ_desc_comfort)
		{
			if (veh->get_freight_type()->get_catg() == 0) // This is a passenger vehicle
			{
				uint8 base_comfort = 0;
				uint8 added_comfort = 0;
				uint8 class_comfort = 0;
				char extra_comfort[8];

				for (int i = 0; i < veh->get_number_of_classes(); i++)
				{
					if (veh->get_comfort(i) > base_comfort)
					{
						base_comfort = veh->get_comfort(i);
						class_comfort = i;
					}
				}
				// If added comfort due to the vehicle being a catering vehicle is desired, uncomment this line:
				//added_comfort = veh->get_catering_level() > 0 ? veh->get_adjusted_comfort(veh->get_catering_level(), class_comfort) - base_comfort : 0;
				if (added_comfort > 0)
				{
					sprintf(extra_comfort, "+%i", added_comfort);
				}
				else
				{
					extra_comfort[0] = '\0';
				}
				sprintf(sort_mode_text, "%s %i%s", translator::translate("Comfort:"), base_comfort, extra_comfort);
				display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, sort_mode_text, ALIGN_RIGHT, text_color, true);
				ypos_name += LINESPACE;
			}
		}
		if (sort_mode == vehicle_manager_t::sort_mode_desc_t::by_desc_power || display_mode == vehicle_manager_t::display_mode_desc_t::displ_desc_power)
		{
			if (veh->get_power() > 0)
			{
				sprintf(sort_mode_text, translator::translate("Power/tractive force (%s): %4d kW / %d kN\n"), translator::translate(engine_type_names[veh->get_engine_type() + 1]), veh->get_power(), veh->get_tractive_effort());
			}
			else
			{
				sprintf(sort_mode_text, translator::translate("unpowered"));
			}
			display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, sort_mode_text, ALIGN_RIGHT, text_color, true);
			ypos_name += LINESPACE;
		}
		if (sort_mode == vehicle_manager_t::sort_mode_desc_t::by_desc_runway_length || display_mode == vehicle_manager_t::display_mode_desc_t::displ_desc_runway_length)
		{
			if (veh->get_waytype() == waytype_t::air_wt)
			{
				sprintf(sort_mode_text, translator::translate("minimum_runway_length: %um"), veh->get_minimum_runway_length());
				display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, sort_mode_text, ALIGN_RIGHT, text_color, true);
				ypos_name += LINESPACE;
			}
		}
		if (sort_mode == vehicle_manager_t::sort_mode_desc_t::by_desc_speed || display_mode == vehicle_manager_t::display_mode_desc_t::displ_desc_speed)
		{
			sprintf(sort_mode_text, translator::translate("max_speed: %ukm/h"), veh->get_topspeed());
			display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, sort_mode_text, ALIGN_RIGHT, text_color, true);
			ypos_name += LINESPACE;
		}
		if (sort_mode == vehicle_manager_t::sort_mode_desc_t::by_desc_tractive_effort || display_mode == vehicle_manager_t::display_mode_desc_t::displ_desc_tractive_effort)
		{
			if (veh->get_power() > 0)
			{
				sprintf(sort_mode_text, translator::translate("Power/tractive force (%s): %4d kW / %d kN\n"), translator::translate(engine_type_names[veh->get_engine_type() + 1]), veh->get_power(), veh->get_tractive_effort());
			}
			else
			{
				sprintf(sort_mode_text, translator::translate("unpowered"));
			}
			display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, sort_mode_text, ALIGN_RIGHT, text_color, true);
			ypos_name += LINESPACE;
		}
		if (sort_mode == vehicle_manager_t::sort_mode_desc_t::by_desc_weight || display_mode == vehicle_manager_t::display_mode_desc_t::displ_desc_weight)
		{
			sprintf(sort_mode_text, translator::translate("weight: %ut"), veh->get_axle_load());
			display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, sort_mode_text, ALIGN_RIGHT, text_color, true);
			ypos_name += LINESPACE;
		}
		//if (display_sort_text)
		//{
		//	display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, sort_mode_text, ALIGN_RIGHT, text_color, true);
		//}
		//ypos_name += LINESPACE;

		const int xoff = VEHICLE_NAME_COLUMN_WIDTH - D_BUTTON_WIDTH;
		int left = pos.x + offset.x + xoff + 4;
		display_base_img(image, left - x, pos.y + offset.y + 21 - y - h / 2, player_nr, false, true);

		ypos_name += LINESPACE * 2;
		image_height = max(window_height, ypos_name);

	}
}

// ***************************************** //
// Actual vehicles:
// ***************************************** //
gui_veh_info_t::gui_veh_info_t(vehicle_t* veh)
{
	this->veh = veh;
	draw(scr_coord(0, 0));
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
			display_fillbox_wh_clip(offset.x + pos.x, offset.y + pos.y + 1, VEHICLE_NAME_COLUMN_WIDTH, max(h, 50) - 2, COL_DARK_BLUE, true);
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
					sprintf(speed_text, translator::translate("%i km/h"), speed_to_kmh(cnv->get_akt_speed()));
					speed_color = text_color;
				}
			}
			display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, speed_text, ALIGN_LEFT, speed_color, true) + 2;
			ypos_name += LINESPACE;
		}

		// WHERE is it (coordinate?, closest city?, show which line it is traversing?)
		// Near a city?
		grund_t *gr = welt->lookup(veh->get_pos());
		char city_text[256];
		sprintf(city_text, "<%i, %i>", veh->get_pos().x, veh->get_pos().y);
		if (veh->get_pos().x >= 0 && veh->get_pos().y >= 0)
		{
			stadt_t *city = welt->get_city(gr->get_pos().get_2d());
			if (city)
			{
				sprintf(city_text, "%s", city->get_name());
			}
			else
			{
				city = welt->find_nearest_city(gr->get_pos().get_2d());
				if (city)
				{
					const uint32 tiles_to_city = shortest_distance(gr->get_pos().get_2d(), (koord)city->get_pos());
					const double km_per_tile = welt->get_settings().get_meters_per_tile() / 1000.0;
					const double km_to_city = (double)tiles_to_city * km_per_tile;
					sprintf(city_text, translator::translate("vicinity_of %s (%ikm)"), city->get_name(), (int)km_to_city);
				}
			}
		}
		display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, city_text, ALIGN_LEFT, text_color, true) + 2;
		ypos_name += LINESPACE;

		// only show assigned line, if there is one!
		if (veh->get_convoi()->in_depot()) {

		}
		else if (veh->get_convoi()->get_line().is_bound()) {
			w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + 6 + ypos_name, translator::translate("Line"), ALIGN_LEFT, text_color, true) + 2;
			w += display_proportional_clip(pos.x + offset.x + 2 + w + 5, pos.y + offset.y + 6 + ypos_name, veh->get_convoi()->get_line()->get_name(), ALIGN_LEFT, veh->get_convoi()->get_line()->get_state_color(), true);
			max_x = max(max_x, w + 5);
		}
		ypos_name += LINESPACE;

		ypos_name = 0;
		const int xpos_extra = VEHICLE_NAME_COLUMN_WIDTH - D_BUTTON_WIDTH - 10;

		// Carried amount
		if (veh->get_desc()->get_total_capacity() > 0)
		{
			char amount[256];
			sprintf(amount, "%i%s %s\n", veh->get_cargo_carried(), translator::translate(veh->get_desc()->get_freight_type()->get_mass()),
				veh->get_desc()->get_freight_type()->get_catg() == 0 ? translator::translate(veh->get_desc()->get_freight_type()->get_name()) : translator::translate(veh->get_desc()->get_freight_type()->get_catg_name()));
			display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, amount, ALIGN_RIGHT, text_color, true) + 2;
		}
		ypos_name += LINESPACE;

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
		display_base_img(image, left - x, pos.y + offset.y + 21 - y - h / 2, veh->get_owner()->get_player_nr(), false, true);

		image_height = h;

	}
}