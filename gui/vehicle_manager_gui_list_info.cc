/*
* 
* Info entries for scrolled lists of vehicles
*
* This file is part of the Simutrans project under the artistic licence.
* (see licence.txt)
*
* Author: Ves
*
*/

#include <stdio.h>
#include <algorithm>

#include "vehicle_manager_gui_list_info.h"
#include "vehicle_manager.h"
#include "../simcity.h"
#include "../player/simplay.h"
#include "../display/viewport.h"
#include "../descriptor/intro_dates.h"

// This section of #define's is identical to the one found in vehicle_manager
// TODO: fetch the needed values directly from there
#define VEHICLE_NAME_COLUMN_WIDTH ((D_BUTTON_WIDTH*5)+15)
#define VEHICLE_NAME_COLUMN_HEIGHT (250)
#define INFORMATION_COLUMN_HEIGHT (20*LINESPACE)
#define UPGRADE_LIST_COLUMN_WIDTH ((D_BUTTON_WIDTH*4) + 15)
#define UPGRADE_LIST_COLUMN_HEIGHT (INFORMATION_COLUMN_HEIGHT - LINESPACE*3)
#define RIGHT_HAND_COLUMN (D_MARGIN_LEFT + VEHICLE_NAME_COLUMN_WIDTH + 10)
#define SCL_HEIGHT (15*LINESPACE)

#define MIN_WIDTH (VEHICLE_NAME_COLUMN_WIDTH * 2) + (D_MARGIN_LEFT * 3)
#define MIN_HEIGHT (D_BUTTON_HEIGHT + 11 + VEHICLE_NAME_COLUMN_HEIGHT + SCL_HEIGHT + INFORMATION_COLUMN_HEIGHT)

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



// We start with the "special" entries. This is used to display a message in the scrolled lists, such as class change, "no vehicles" etc
gui_special_info_t::gui_special_info_t(int entry_width, cbuffer_t message, COLOR_VAL color)
{
	translated_text_string.clear();
	translated_text_string = message;
	background_color = color;
	draw(scr_coord(0, 0));
}

/**
* Events werden hiermit an die GUI-components
* gemeldet
* @author Hj. Malthaner
*/
bool gui_special_info_t::infowin_event(const event_t *ev)
{

	if (IS_LEFTRELEASE(ev)) {
		//selected = !selected;
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
void gui_special_info_t::draw(scr_coord offset)
{
	clip_dimension clip = display_get_clip_wh();
	if (!((pos.y + offset.y) > clip.yy || (pos.y + offset.y) < clip.y - 32)) {

		COLOR_VAL text_color = MN_GREY3;
		COLOR_VAL box_color = MN_GREY1;
		int box_height = LINESPACE * 3;
		int width = get_client().get_width(); // Width of the scrolled list

		display_fillbox_wh_clip(offset.x + pos.x, offset.y + pos.y, width, box_height, background_color, true);
		display_proportional_clip(pos.x + offset.x + (width/2), pos.y + offset.y + LINESPACE, translated_text_string, ALIGN_CENTER_H, text_color, true);
	}
}



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
		int y_pos = 5;
		int x_pos = 0;
		int suitable_break;
		int used_caracters = 0;
		entry_height = 0;
		COLOR_VAL text_color = COL_BLACK;
		const uint16 month_now = welt->get_timeline_year_month();
		bool upgrades = false;
		bool retired = false;
		bool only_as_upgrade = false;
		int fillbox_height = 4 * LINESPACE;
		bool display_bakground = false;
		scr_coord_val x, y, w, h;
		bool counting = true;
		int lines_of_text = 0;
		int width = get_client().get_width(); // Width of the scrolled list

		sprintf(name, translator::translate(upgrade->get_name()));
		sprintf(name_display, name);

		// Since we need to draw the fillbox before any text is drawn, we need to figure out how much text is needed.
		// Therefore this forloop will travel two times, first time it will count the number of lines, and second time, it will draw everything and write all the texts

		for (int i = 0; i < 2; i++)
		{
			const image_id image = upgrade->get_base_image();
			display_get_base_image_offset(image, &x, &y, &w, &h);
			const int xoff = 0;
			int left = pos.x + offset.x + xoff + 4;
			
			if (!counting)
			{
				if (selected)
				{
					display_fillbox_wh_clip(offset.x + pos.x, offset.y + pos.y + 1, UPGRADE_LIST_COLUMN_WIDTH, max(max(h, fillbox_height), 40) - 2, COL_DARK_BLUE, true);
					text_color = COL_WHITE;
				}


				display_base_img(image, left - x, pos.y + offset.y + 21 - y - h / 2, player_nr, false, true);

				int image_offset = min(w + 10, D_BUTTON_WIDTH);
				if (w > image_offset)
				{
					display_bakground = true;
				}
				if (display_bakground)
				{
					if (selected)
					{
						display_blend_wh(offset.x + pos.x + image_offset, pos.y + offset.y + y_pos, UPGRADE_LIST_COLUMN_WIDTH - image_offset, LINESPACE * 3, COL_DARK_BLUE, 50);
					}
					else
					{
						display_blend_wh(offset.x + pos.x + image_offset, pos.y + offset.y + y_pos, UPGRADE_LIST_COLUMN_WIDTH - image_offset, LINESPACE * 3, MN_GREY4, 50);
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
							display_proportional_clip(pos.x + offset.x + image_offset, pos.y + offset.y + y_pos, name_display, ALIGN_LEFT, text_color, true);
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

							display_proportional_clip(pos.x + offset.x + image_offset, pos.y + offset.y + y_pos, name_display, ALIGN_LEFT, text_color, true);

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
					display_proportional_clip(pos.x + offset.x + image_offset, pos.y + offset.y + y_pos, name, ALIGN_LEFT, text_color, true);
				}
			}

			y_pos = 0;
			x_pos = UPGRADE_LIST_COLUMN_WIDTH - 14;

			// Byggår
			if (counting)
			{
				lines_of_text++;
			}
			else
			{
				char year[50];
				sprintf(year, "%s: %u", translator::translate("available_until"), upgrade->get_retire_year_month() / 12);
				display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + y_pos, year, ALIGN_RIGHT, text_color, true);
				y_pos += LINESPACE;
			}

			// Following section compares different values between the old and the new vehicle, to see what is upgraded

			COLOR_VAL difference_color = COL_DARK_GREEN;
			char tmp[50];

			// Load
			// TODO: Add class stuff and possibly comfort levels here // classes // comfort		
			if (upgrade->get_total_capacity() != existing->get_total_capacity())
			{
				if (counting)
				{
					lines_of_text++;
				}
				else
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

					int entry = display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + y_pos, tmp, ALIGN_RIGHT, difference_color, true);
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

					entry += display_proportional_clip(pos.x + offset.x + x_pos - entry, pos.y + offset.y + y_pos, tmp, ALIGN_RIGHT, text_color, true);

					if (upgrade->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_PAS)
					{
						display_color_img(skinverwaltung_t::passengers->get_image_id(0), pos.x + offset.x + 2 + x_pos - entry - 16, pos.y + offset.y + y_pos, 0, false, false);
					}
					else if (upgrade->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_MAIL)
					{
						display_color_img(skinverwaltung_t::mail->get_image_id(0), pos.x + offset.x + 2 + x_pos - entry - 16, pos.y + offset.y + y_pos, 0, false, false);
					}
					else
					{
						display_color_img(skinverwaltung_t::goods->get_image_id(0), pos.x + offset.x + 2 + x_pos - entry - 16, pos.y + offset.y + y_pos, 0, false, false);
					}

					y_pos += LINESPACE;
				}
			}

			// top speed
			if (upgrade->get_topspeed() != existing->get_topspeed())
			{
				if (counting)
				{
					lines_of_text++;
				}
				else
				{
					int difference = upgrade->get_topspeed() - existing->get_topspeed();
					if (difference > 0)
					{
						sprintf(tmp, "+%i%s", difference, translator::translate("km/h"));
						difference_color = COL_DARK_GREEN;
					}
					else
					{
						sprintf(tmp, "%i%s", difference, translator::translate("km/h"));
						difference_color = COL_RED;
					}
					int entry = display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + y_pos, tmp, ALIGN_RIGHT, difference_color, true);
					sprintf(tmp, translator::translate("top_speed: %ikm/h "), upgrade->get_topspeed());
					display_proportional_clip(pos.x + offset.x + x_pos - entry, pos.y + offset.y + y_pos, tmp, ALIGN_RIGHT, text_color, true);
					y_pos += LINESPACE;
				}
			}
			// weight
			if (upgrade->get_weight() != existing->get_weight())
			{
				if (counting)
				{
					lines_of_text++;
				}
				else
				{
					int difference = (upgrade->get_weight() / 1000) - (existing->get_weight() / 1000);
					if (difference > 0)
					{
						sprintf(tmp, "+%i%s", difference, translator::translate("tonnen"));
						difference_color = COL_DARK_GREEN;
					}
					else
					{
						sprintf(tmp, "%i%s", difference, translator::translate("tonnen"));
						difference_color = COL_RED;
					}
					int entry = display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + y_pos, tmp, ALIGN_RIGHT, difference_color, true);
					sprintf(tmp, translator::translate("weight: %it "), upgrade->get_weight() / 1000);
					display_proportional_clip(pos.x + offset.x + x_pos - entry, pos.y + offset.y + y_pos, tmp, ALIGN_RIGHT, text_color, true);
					y_pos += LINESPACE;
				}
			}
			// loading time

			// brake force
			if (upgrade->get_brake_force() != existing->get_brake_force())
			{
				if (counting)
				{
					lines_of_text++;
				}
				else
				{
					int difference = upgrade->get_brake_force() - existing->get_brake_force();
					if (difference > 0)
					{
						sprintf(tmp, "+%i%s", difference, translator::translate("kn"));
						difference_color = COL_DARK_GREEN;
					}
					else
					{
						sprintf(tmp, "%i%s", difference, translator::translate("kn"));
						difference_color = COL_RED;
					}
					int entry = display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + y_pos, tmp, ALIGN_RIGHT, difference_color, true);
					sprintf(tmp, translator::translate("brake_force: %ikn "), upgrade->get_brake_force());
					display_proportional_clip(pos.x + offset.x + x_pos - entry, pos.y + offset.y + y_pos, tmp, ALIGN_RIGHT, text_color, true);
					y_pos += LINESPACE;
				}
			}

			// power
			if (upgrade->get_power() != existing->get_power())
			{
				if (counting)
				{
					lines_of_text++;
				}
				else
				{
					int difference = upgrade->get_power() - existing->get_power();
					if (difference > 0)
					{
						sprintf(tmp, "+%i%s", difference, translator::translate("kw"));
						difference_color = COL_DARK_GREEN;
					}
					else
					{
						sprintf(tmp, "%i%s", difference, translator::translate("kw"));
						difference_color = COL_RED;
					}
					int entry = display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + y_pos, tmp, ALIGN_RIGHT, difference_color, true);
					sprintf(tmp, translator::translate("power: %ikw "), upgrade->get_power());
					display_proportional_clip(pos.x + offset.x + x_pos - entry, pos.y + offset.y + y_pos, tmp, ALIGN_RIGHT, text_color, true);
					y_pos += LINESPACE;
				}
			}
			// tractive effort

			// running_cost
			// fixed_cost
			// 	engine_type
			// is_tilting
			// catering_level
			// air_resistance
			// rolling_resistance
			// minimum_runway_length
			// range
			// way_wear_factor




			// Issues

			COLOR_VAL issue_color = COL_BLACK;
			char issues[100] = "\0";

			if (upgrade->is_retired(month_now))
			{
				if (counting)
				{
					lines_of_text++;
				}
				else
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
					display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + y_pos, issues, ALIGN_RIGHT, issue_color, true);
					y_pos += LINESPACE;
				}
			}

			if (upgrade->is_available_only_as_upgrade())
			{
				if (counting)
				{
					lines_of_text++;
				}
				else
				{
					sprintf(issues, "%s", translator::translate("only_as_upgrade"));
					issue_color = selected ? text_color : SYSCOL_EDIT_TEXT_DISABLED;
					display_proportional_clip(pos.x + offset.x + x_pos, pos.y + offset.y + y_pos, issues, ALIGN_RIGHT, issue_color, true);
					y_pos += LINESPACE;
				}
			}
			counting = false;
			fillbox_height = lines_of_text*LINESPACE;
		}



		y_pos += LINESPACE * 2;
		entry_height = max(h, y_pos);

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

	draw(scr_coord(0, 0));
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
		entry_height = 0;
		COLOR_VAL text_color = COL_BLACK;
		const uint16 month_now = welt->get_timeline_year_month();
		bool upgrades = false;
		bool retired = false;
		bool only_as_upgrade = false;
		int window_height = 6 * LINESPACE; // minimum size of the entry
		int width = get_client().get_width(); // Width of the scrolled list

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
			display_fillbox_wh_clip(offset.x + pos.x, offset.y + pos.y + 1, width, window_height - 2, COL_DARK_BLUE, true);
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
		char year[50];
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
		char amount_of_vehicles[50];
		sprintf(amount_of_vehicles, "%s: %u", translator::translate("amount"), amount);
		display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, amount_of_vehicles, ALIGN_RIGHT, text_color, true);
		ypos_name += LINESPACE;

		// Load
		if (veh->get_total_capacity() > 0)
		{
			char load[100];
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

		if (sort_mode == vehicle_manager_t::sort_mode_desc_t::by_desc_classes || sort_mode == vehicle_manager_t::sort_mode_desc_t::by_desc_cargo_type_and_capacity || display_mode == vehicle_manager_t::display_mode_desc_t::displ_desc_classes || display_mode == vehicle_manager_t::display_mode_desc_t::displ_desc_cargo_cat)
		{
			bool pass_veh = veh->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_PAS;
			bool mail_veh = veh->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_MAIL;

			if ((pass_veh || mail_veh) && veh->get_total_capacity()>0)
			{
				uint8 classes_amount = veh->get_number_of_classes() < 1 ? 1 : veh->get_number_of_classes();
				bool entry_made = false;
				int n = 0;
				char buf[100];
				for (uint8 j = 0; j < classes_amount; j++)
				{
					int i = classes_amount - 1 - j;
					if (veh->get_capacity(i) > 0)
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

						if (entry_made)
						{
							n += sprintf(buf + n, ", ");
						}
						n += sprintf(buf + n, "%s: %3d", class_name, veh->get_capacity(i));
						entry_made = true;
					}
				}
				sprintf(sort_mode_text, buf);
				display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + 6 + ypos_name, sort_mode_text, ALIGN_RIGHT, text_color, true);
				ypos_name += LINESPACE;
			}
		}
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
			if (veh->get_freight_type()->get_catg_index() == goods_manager_t::INDEX_PAS) // This is a passenger vehicle
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
			sprintf(sort_mode_text, translator::translate("weight: %ut"), veh->get_weight() / 1000);
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
		entry_height = max(window_height, ypos_name);

	}
}

// ***************************************** //
// Actual vehicles:
// ***************************************** //
gui_veh_info_t::gui_veh_info_t(vehicle_t* veh, int sortmode_index, int displaymode_index)
{
	this->veh = veh;
	draw(scr_coord(0, 0));
	sort_mode = sortmode_index;
	display_mode = displaymode_index;

	if (veh->get_cargo_max() > 0)
	{
		load_percentage = (veh->get_cargo_carried() * 100) / veh->get_cargo_max();
		filled_bar.set_pos(scr_coord(0, 0));
		filled_bar.set_size(scr_size(D_BUTTON_WIDTH, 4));
		filled_bar.add_color_value(&load_percentage, COL_GREEN);
	}
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

		int ypos_name = LINESPACE / 2;
		int min_size = LINESPACE * 5;
		entry_height = min_size;
		static cbuffer_t freight_info;
		freight_info.clear();
		int x_size = get_size().w - 51 - pos.x;
		int width = get_client().get_width(); // Width of the scrolled list

		COLOR_VAL text_color = COL_BLACK;
		scr_coord_val x, y, w, h;
		const image_id image = veh->get_base_image();
		display_get_base_image_offset(image, &x, &y, &w, &h);
		if (h > entry_height)
		{
			for (int i = 0; entry_height < h; i++)
			{
				entry_height += LINESPACE;
			}
		}
		if (selected)
		{
			display_fillbox_wh_clip(offset.x + pos.x, offset.y + pos.y + 1, width, entry_height, COL_DARK_BLUE, true);
			text_color = COL_WHITE;
		}
		// name wont be necessary, since we get the name from the left hand column and also elsewhere.
		// However, some method to ID the vehicle is desirable
		// TODO: ID-tag or similar.
		int max_x = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + ypos_name, translator::translate(veh->get_name()), ALIGN_LEFT, text_color, true) + 2;
		ypos_name += LINESPACE;

		// current speed/What is it doing (loading, waiting for time etcetc...)
		if (veh->get_convoi()->in_depot())
		{
			const char *txt = translator::translate("(in depot)");
			w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + ypos_name, txt, ALIGN_LEFT, text_color, true) + 2;
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
			display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + ypos_name, speed_text, ALIGN_LEFT, speed_color, true) + 2;
			ypos_name += LINESPACE;
		}

		// WHERE is it?
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
		display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + ypos_name, city_text, ALIGN_LEFT, text_color, true) + 2;
		ypos_name += LINESPACE;

		// only show assigned line, if there is one!
		if (veh->get_convoi()->in_depot()) {

		}
		else if (veh->get_convoi()->get_line().is_bound()) {
			w = display_proportional_clip(pos.x + offset.x + 2, pos.y + offset.y + ypos_name, translator::translate("Line:"), ALIGN_LEFT, text_color, true) + 2;

			// If the line state color is OK it will be black, and therefore very hard to see if the entry is also selected. Therefore, change the color to white like the rest of the text if the entry is selected.
			COLOR_VAL line_color = (COLOR_VAL)veh->get_convoi()->get_line()->get_state_color();
			if (selected && veh->get_convoi()->get_line()->get_state() == simline_t::states::line_normal_state)
			{
				line_color = text_color;
			}
			w += display_proportional_clip(pos.x + offset.x + 2 + w + 5, pos.y + offset.y + ypos_name, veh->get_convoi()->get_line()->get_name(), ALIGN_LEFT, line_color, true);
			max_x = max(max_x, w + 5);
		}
		ypos_name += LINESPACE;

		ypos_name = LINESPACE / 2;
		const int xpos_extra = VEHICLE_NAME_COLUMN_WIDTH - D_BUTTON_WIDTH - 10;

		// age		
		char year[50];
		sprintf(year, "%s: %i", translator::translate("bought"), veh->get_purchase_time() / 12);
		display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + ypos_name, year, ALIGN_RIGHT, text_color, true) + 2;
		ypos_name += LINESPACE;

		// Carried amount, extended display when "displ_veh_cargo"
		char freight[256];
		if (veh->get_desc()->get_total_capacity() > 0)
		{
			if (display_mode == vehicle_manager_t::display_mode_veh_t::displ_veh_cargo) // Extended display
			{

				int returns = 0;

				// We get the freight info via the freight_list_sorter, so no need to do anything but fetch it
				veh->get_cargo_info(freight_info, true);
				display_multiline_text(pos.x + offset.x + 2 + xpos_extra - (D_BUTTON_WIDTH*2), pos.y + offset.y + ypos_name, freight_info, text_color);

				returns = 0;
				const char *p = freight_info;
				for (int i = 0; i < freight_info.len(); i++)
				{
					if (p[i] == '\n')
					{
						returns++;
					}
				}
				ypos_name += (returns*LINESPACE) + (2 * LINESPACE);
			}
			else // Simple display
			{
				sprintf(freight, "%i%s %s", veh->get_cargo_carried(), translator::translate(veh->get_desc()->get_freight_type()->get_mass()),
					veh->get_desc()->get_freight_type()->get_catg() == 0 ? translator::translate(veh->get_desc()->get_freight_type()->get_name()) : translator::translate(veh->get_desc()->get_freight_type()->get_catg_name()));
				display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + ypos_name, freight, ALIGN_RIGHT, text_color, true) + 2;
				ypos_name += LINESPACE;
			}
			
		}
		else if (display_mode == vehicle_manager_t::display_mode_veh_t::displ_veh_cargo) // Carries no good
		{
			sprintf(freight, "%s", translator::translate("carries_no_good"));
			display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + ypos_name, freight, ALIGN_RIGHT, text_color, true) + 2;
			ypos_name += LINESPACE;
		}
		

		// TODO: odometer	
		//char odometer[20];
		//sprintf(odometer, "%s: %i", translator::translate("odometer"), /*vehicle odometer should be displayed here*/);
		//display_proportional_clip(pos.x + offset.x + 2 + xpos_extra, pos.y + offset.y + ypos_name, odometer, ALIGN_RIGHT, text_color, true) + 2;

		const int xoff = VEHICLE_NAME_COLUMN_WIDTH - D_BUTTON_WIDTH;
		int left = pos.x + offset.x + xoff + 4;
		display_base_img(image, left - x, pos.y + offset.y + 21 - y - h / 2, veh->get_owner()->get_player_nr(), false, true);
		
		if (veh->get_cargo_max() > 0)
		{
			load_percentage = (veh->get_cargo_carried() * 100) / veh->get_cargo_max();
			filled_bar.draw(scr_coord(offset.x + width - D_BUTTON_WIDTH, pos.y + offset.y + entry_height - (LINESPACE / 2)));
		}

		entry_height = ypos_name > entry_height ? ypos_name : entry_height;
		entry_height += 2;

	}
}