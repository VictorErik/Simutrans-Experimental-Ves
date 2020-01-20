/*
 * Vehicle management
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 *
 * Author: Ves
 */


#include "gui_frame.h"
#include "vehicle_manager_gui_list_info.h"
#include "components/gui_container.h"
#include "components/gui_label.h"
#include "components/gui_chart.h"
#include "components/gui_textinput.h"
#include "components/gui_scrolled_list.h"
#include "components/gui_scrollpane.h"
#include "components/gui_tab_panel.h"
#include "components/gui_combobox.h"
#include "components/gui_label.h"
#include "components/gui_convoiinfo.h"
#include "components/gui_image_list.h"
#include "halt_list_stats.h"
#include "../simline.h"
#include "../simconvoi.h"
#include "../simworld.h"

class player_t;


#ifndef gui_vehicle_manager_h
#define gui_vehicle_manager_h
class vehicle_manager_t : public gui_frame_t, public action_listener_t
{
public:
	enum sort_mode_desc_t {

		by_desc_name,
		by_desc_issues,
		by_desc_intro_year,
		by_desc_amount,
		by_desc_cargo_type_and_capacity,
		by_desc_speed,
		by_desc_catering_level,
		by_desc_comfort,
		by_desc_classes,
		by_desc_power,
		by_desc_tractive_effort,
		by_desc_weight,
		by_desc_axle_load,
		by_desc_runway_length,
		SORT_MODES_DESC
	};

	enum sort_mode_veh_t {
		by_age = 0,
		by_odometer = 1,
		by_issue = 2,
		by_location = 3,
		SORT_MODES_VEH = 4
	};

	enum display_mode_desc_t {
		displ_desc_name,
		displ_desc_amount,
		displ_desc_intro_year,
		displ_desc_speed,
		displ_desc_cargo_cat,
		displ_desc_comfort,
		displ_desc_classes,
		displ_desc_catering_level,
		displ_desc_power,
		displ_desc_tractive_effort,
		displ_desc_weight,
		displ_desc_axle_load,
		displ_desc_runway_length,
		DISPLAY_MODES_DESC
	};

	enum display_mode_veh_t {
		displ_veh_age,
		displ_veh_odometer,
		displ_veh_location,
		displ_veh_cargo,
		DISPLAY_MODES_VEH
	};

	enum info_tab_t {
		infotab_general,
		infotab_economics,
		infotab_maintenance,
		infotab_advanced,
		DISPLAY_MODES_INFOTAB
	};
private:
	player_t *player;
	uint8 player_nr;

	gui_container_t cont_desc, cont_veh, cont_upgrade, cont_livery, cont_maintenance_info, cont_economics_info, dummy;
	gui_scrollpane_t scrolly_desc, scrolly_veh, scrolly_upgrade, scrolly_livery;
	gui_tab_panel_t tabs_waytype;
	gui_tab_panel_t tabs_vehicletype;
	gui_tab_panel_t tabs_info;
	gui_combobox_t combo_sorter_desc;
	gui_combobox_t combo_sorter_veh;
	gui_combobox_t combo_display_desc, combo_display_veh;

	button_t bt_show_available_vehicles, bt_show_out_of_production_vehicles, bt_show_obsolete_vehicles;
	button_t bt_select_all, bt_hide_in_depot;
	button_t bt_veh_next_page, bt_veh_prev_page, bt_desc_next_page, bt_desc_prev_page;
	button_t bt_upgrade_im, bt_upgrade_ov;
	button_t bt_upgrade_to_from;
	button_t bt_desc_sortreverse, bt_veh_sortreverse ;
	button_t bt_append_livery, bt_show_obsolete_liveries, bt_reset_all_classes;

	gui_label_t lb_amount_desc, lb_amount_veh;
	gui_label_t lb_desc_page, lb_veh_page;
	gui_label_t lb_desc_sortby, lb_veh_sortby, lb_display_desc, lb_display_veh;
	gui_label_t lb_available_liveries, lb_available_classes, lb_reassign_class_to;

	gui_textinput_t ti_desc_display, ti_veh_display;
	gui_combobox_t combo_desc_display, combo_veh_display;

	uint8 pass_classes = goods_manager_t::passengers->get_number_of_classes();
	uint8 mail_classes = goods_manager_t::mail->get_number_of_classes();
	slist_tpl<gui_combobox_t*> pass_class_sel;
	slist_tpl<gui_combobox_t*> mail_class_sel;
	slist_tpl<gui_label_t*> lb_pass_class;
	slist_tpl<gui_label_t*> lb_mail_class;
	char* pass_class_name_untranslated[32];
	char* mail_class_name_untranslated[32];
	uint32 pass_capacity_at_accommodation[255] = { 0 };
	uint32 mail_capacity_at_accommodation[255] = { 0 };
	bool any_mail_cargo, any_pass_cargo;

	char text_show_all_vehicles[50];
	char text_show_out_of_production_vehicles[50];
	char text_show_obsolete_vehicles[50];
	char text_upgrade_to[50];
	char text_upgrade_from[50];

	char sortby_text[50];
	char displayby_text[50];

	bool display_upgrade_into;
	int amount_of_upgrades;
	int amount_of_liveries;

	int initial_upgrade_entry_width;
	int initial_livery_entry_width;

	static bool show_available_vehicles;
	static bool show_out_of_production_vehicles;
	static bool show_obsolete_vehicles;
	static bool select_all;
	static bool hide_veh_in_depot;
	static bool show_obsolete_liveries;

	static sort_mode_desc_t sortby_desc;
	static sort_mode_veh_t sortby_veh;

	static display_mode_desc_t display_desc;
	static display_mode_veh_t display_veh;
	
	static bool desc_sortreverse;
	static bool veh_sortreverse;

	const uint16 desc_pr_page = 400;
	const uint16 veh_pr_page = 400;
		
	waytype_t way_type;
	
	vector_tpl<vehicle_desc_t*> desc_list;			// Total list of desc's.	
	vector_tpl<gui_desc_info_t *> desc_info;		// List of desc's that is actually displayed	
	vector_tpl<vehicle_desc_t*> desc_list_old_index;// For some sort modes, when "desc_list" is being sorted, this keeps track of old index numbers	
	vector_tpl<vehicle_t*> veh_list;				// Total list of veh's.	
	vector_tpl<gui_veh_info_t *> veh_info;			// List of veh's that is actually displayed	
	vector_tpl<vehicle_t*> vehicle_we_own;			// All vehicles we own for a given waytype	
	vector_tpl<gui_upgrade_info_t *> upgrade_info;	// List of upgrades that is displayed	
	vector_tpl<gui_livery_info_t*> livery_info;		// List of liveries that is displayed	
	bool* veh_selection;							// Array of bool's to keep track of which veh's is selected	
	bool bool_veh_selection_exists = false;			// If true, remember to delete "veh_selection"
	int count_veh_selection;						// The amount of selected vehicles


	// This section defines some sizes
	int minwidth;
	int minheight;
	int width;
	int height;

	// The added width and height to the window from default
	int extra_width;
	int extra_height;

	// Upper, middle and lower section
	int header_section;
	int upper_section;
	int middle_section;
	int lower_section;

	// Start by determining which of these translations is the longest, since the GUI depends upon it:
	int label_length;
	int combobox_width;

	// Header columns
	int h_column_1;
	int h_column_2;
	int h_column_3;

	// Upper columns
	int u_column_1;
	int u_column_2;
	int u_column_3;
	int u_column_4;
	int u_column_5;
	int u_column_6;

	// (Middle section doesnt use columns)
	
	// Lower columns
	int l_column_1;
	int l_column_2;
	int l_column_3;
	int l_column_4;
	int l_column_5;
	int l_column_6;
	int l_column_7; // Right hand edge of the window. Only things left from here!!

	void display(scr_coord pos);

	uint32 amount_desc;
	uint32 amount_veh;
	uint32 amount_veh_owned;

	cbuffer_t cargo_buf; // Freight text

	vehicle_desc_t* old_desc_for_display;
	vehicle_desc_t* goto_this_desc = NULL;
	int selected_desc_index;
	int new_count_veh_selection;
	int old_count_veh_selection = -1;
	int new_count_owned_veh;
	int old_count_owned_veh = -1;
	int new_count_cargo_carried;
	int old_count_cargo_carried = -1;

	int page_display_desc = 1;
	int page_display_veh = 1;
	int page_amount_desc = 1;
	int page_amount_veh = 1;

	bool page_turn_desc = false;
	bool page_turn_veh = false;

	static const char *sort_text_desc[SORT_MODES_DESC];
	static const char *sort_text_veh[SORT_MODES_VEH]; 
	static const char *display_text_desc[DISPLAY_MODES_DESC];
	static const char *display_text_veh[DISPLAY_MODES_VEH];

	vehicle_desc_t* desc_info_text;
	bool restore_desc_selection = false;

	scr_coord desc_info_text_pos;
	scr_coord scrolly_desc_pos;

	vehicle_desc_t* vehicle_as_upgrade = NULL;
	int selected_upgrade_index;

	int set_desc_scroll_position = 0;
	bool reposition_desc_scroll = false;

	char desc_display_param[64], old_desc_display_param[64];
	char veh_display_param[64], old_veh_display_param[64];

	char ch_desc_first_logic[1] = { 0 };
	char ch_desc_second_logic[1] = { 0 };
	char ch_desc_first_value[10] = { 0 };
	char ch_desc_second_value[10] = { 0 };

	bool ti_desc_first_logic_exists = false;
	bool ti_desc_second_logic_exists = false;
	bool ti_desc_first_value_exists = false;
	bool ti_desc_second_value_exists = false;
	bool ti_desc_no_logics = false;

	char ch_veh_first_logic[1] = { 0 };
	char ch_veh_second_logic[1] = { 0 };
	char ch_veh_first_value[10] = { 0 };
	char ch_veh_second_value[10] = { 0 };

	bool ti_veh_first_logic_exists = false;
	bool ti_veh_second_logic_exists = false;
	bool ti_veh_first_value_exists = false;
	bool ti_veh_second_value_exists = false;
	bool ti_veh_no_logics = false;

	bool ti_veh_distance_to_city_exists = false;
	bool ti_desc_invalid_entry_form = false;
	bool ti_veh_invalid_entry_form = false;

	uint16 desc_display_first_value;
	uint16 desc_display_second_value;

	uint16 veh_display_first_value;
	uint16 veh_display_second_value;

	char desc_display_name[64];
	char veh_display_location[64];
	int letters_to_compare;

	bool display_show_any;
	vector_tpl<uint8> veh_display_combobox_indexes;

public:
	vehicle_manager_t(player_t* player);
	~vehicle_manager_t();

	vehicle_manager_t();

	static int display_desc_by_good;
	static int display_desc_by_class;

	static int display_veh_by_cargo;

	void build_desc_list();
	void build_veh_list();
	void display_desc_list();
	void display_veh_list();

	void build_upgrade_list();
	void build_livery_list();

	void draw_general_information(const scr_coord& pos);
	void draw_economics_information(const scr_coord& pos);
	void draw_maintenance_information(const scr_coord& pos);

	void vehicle_manager_t::display_tab_objects();

	void update_desc_text_input_display();
	void update_veh_text_input_display();

	void reset_desc_text_input_display();
	void reset_veh_text_input_display();

	void set_desc_display_rules();
	void set_veh_display_rules();

	bool is_desc_displayable(vehicle_desc_t *desc);
	bool is_veh_displayable(vehicle_t *veh);
	
	uint8 return_desc_category(vehicle_desc_t *desc);

	void update_tabs();
	void update_vehicle_type_tabs();
	void update_veh_selection();
	void update_cargo_manifest(cbuffer_t& buf);
	void build_class_entries();

	bool freight_info_resort;

	void save_previously_selected_desc();

	void sort_desc();
	void sort_veh();
	static bool compare_desc(vehicle_desc_t*, vehicle_desc_t*);
	static bool compare_veh(vehicle_t*, vehicle_t*);

	static bool compare_desc_amount(char*, char*);

	static int find_desc_issue_level(vehicle_desc_t*);
	static int find_veh_issue_level(vehicle_t*);
	
	/**
	* in top-level windows the name is displayed in titlebar
	* @return the non-translated component name
	* @author Hj. Malthaner
	*/
	const char* get_name() const { return "vehicle_manager"; }

	/**
	* Set the window associated helptext
	* @return the filename for the helptext, or NULL
	* @author Hj. Malthaner
	*/
	const char* get_help_filename() const { return "vehicle_manager_t.txt"; }

	/**
	* Does this window need a min size button in the title bar?
	* @return true if such a button is needed
	* @author Hj. Malthaner
	*/
	bool has_min_sizer() const {return true;}

	/**
	* Draw new component. The values to be passed refer to the window
	* i.e. It's the screen coordinates of the window where the
	* component is displayed.
	* @author Hj. Malthaner
	*/
	void draw(scr_coord pos, scr_size size);

	/**
	* Set window size and adjust component sizes and/or positions accordingly
	* @author Hj. Malthaner
	*/
	virtual void set_windowsize(scr_size size);

	//bool infowin_event(event_t const*) OVERRIDE;

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;




	//// following: rdwr stuff
	void rdwr( loadsave_t *file );
	uint32 get_rdwr_id();
};

#endif
