/*
 * Dialog with list of schedules.
 * Contains buttons: edit new remove
 * Resizable.
 *
 * @author Niels Roest
 * @author hsiegeln: major redesign
 */


#include "gui_frame.h"
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

#ifndef gui_upgrade_info_h
#define gui_upgrade_info_h
/**
* One element of the vehicle list display
*
* @author Hj. Malthaner
*/
class gui_upgrade_info_t : public gui_world_component_t
{
private:
	player_t *player;
	uint8 player_nr;
	vehicle_desc_t* upgrade;
	vehicle_desc_t* existing;

public:
	gui_upgrade_info_t(vehicle_desc_t* upgrade, vehicle_desc_t* existing);

	bool infowin_event(event_t const*) OVERRIDE;
	bool selected = false;
	bool open_info = false;

	bool is_selected() { return selected; }
	bool set_selection(bool sel) { return selected = sel; }
	bool is_open_info() { return open_info; }

	vehicle_desc_t* get_upgrade() { return upgrade; }
	vehicle_desc_t* get_existing() { return existing; }

	/**
	* Draw the component
	* @author Hj. Malthaner
	*/
	void draw(scr_coord offset);

	int image_height;
};

#endif
#ifndef gui_desc_info_h
#define gui_desc_info_h
/**
* One element of the vehicle list display
*
* @author Hj. Malthaner
*/
class gui_desc_info_t : public gui_world_component_t
{
private:
	player_t *player;
	uint8 player_nr;
	/**
	* Handle Convois to be displayed.
	* @author Hj. Malthaner
	*/
	vehicle_desc_t* veh;

	uint16 amount;

public:
	/**
	* @param cnv, the handler for the Convoi to be displayed.
	* @author Hj. Malthaner
	*/
	gui_desc_info_t(vehicle_desc_t* veh, uint16 amount);

	bool infowin_event(event_t const*) OVERRIDE;
	bool selected = false;

	bool is_selected() { return selected; }
	bool set_selection(bool sel) { return selected = sel; }
	vehicle_desc_t* get_desc() { return veh; }
	
	/**
	* Draw the component
	* @author Hj. Malthaner
	*/
	void draw(scr_coord offset);

	int image_height;
};

#endif
#ifndef gui_veh_info_h
#define gui_veh_info_h
/**
* One element of the vehicle list display
*
* @author Hj. Malthaner
*/
class gui_veh_info_t : public gui_world_component_t
{
private:
	player_t *player;
	/**
	* Handle Convois to be displayed.
	* @author Hj. Malthaner
	*/
	vehicle_t* veh;

	sint32 mean_convoi_speed;

	bool selected = false;

public:
	/**
	* @param cnv, the handler for the Convoi to be displayed.
	* @author Hj. Malthaner
	*/
	gui_veh_info_t(vehicle_t* veh);

	bool infowin_event(event_t const*) OVERRIDE;

	/**
	* Draw the component
	* @author Hj. Malthaner
	*/
	void draw(scr_coord offset);
	
	bool is_selected() { return selected; }
	bool set_selection(bool sel) { return selected = sel; }

	int image_height;
};


#endif
#ifndef gui_vehicle_manager_h
#define gui_vehicle_manager_h
class vehicle_manager_t : public gui_frame_t, public action_listener_t
{
public:
	enum sort_mode_desc_t {
		by_desc_name = 0,
		by_desc_intro_year = 1,
		by_desc_amount = 2,
		by_desc_cargo_type_and_capacity = 3,
		by_desc_speed = 4,
		//by_desc_issues = 3, // Experimental: If the vehicle_desc has any issues, like out of production, can upgrade, etc
		SORT_MODES_DESC = 5
	};

	enum sort_mode_veh_t {
		by_age = 0,
		by_odometer = 1,
		by_issue = 2,
		by_location = 3,
		SORT_MODES_VEH = 4
	};
private:
	player_t *player;
	uint8 player_nr;

	gui_container_t cont_desc, cont_veh, cont_upgrade, cont_maintenance_info, dummy;
	gui_scrollpane_t scrolly_desc, scrolly_veh, scrolly_upgrade;
	gui_tab_panel_t tabs_waytype;
	gui_tab_panel_t tabs_info;
	gui_combobox_t combo_sorter_desc;
	gui_combobox_t combo_sorter_veh;

	button_t bt_show_available_vehicles;
	button_t bt_select_all;
	button_t bt_veh_next_page, bt_veh_prev_page, bt_desc_next_page, bt_desc_prev_page;
	button_t bt_upgrade;
	button_t bt_upgrade_to_from;

	bool display_upgrade_into;
	int amount_of_upgrades;

	bool show_available_vehicles;
	bool select_all;
	bool just_selected_all = false;

	static sort_mode_desc_t sortby_desc;
	static sort_mode_veh_t sortby_veh;
	static bool sortreverse;

	const int desc_pr_page = 500;
	const int veh_pr_page = 500;

	// All vehicles
	//vector_tpl<quickstone_tpl<vehicle_desc_t>> vehicle_descs;
	//vehicle_t *veh;
	//vector_tpl<gui_image_list_t::image_data_t*> veh_im;

	
	waytype_t way_type;
	
	// Total list of desc's.
	vector_tpl<vehicle_desc_t*> desc_list;
	// List of desc's that is actually displayed
	vector_tpl<gui_desc_info_t *> desc_info;
	// For some sort modes, when "desc_list" is being sorted, this keeps track of old index numbers
	vector_tpl<vehicle_desc_t*> desc_list_old_index;

	// Total list of veh's.
	vector_tpl<vehicle_t*> veh_list;
	// List of veh's that is actually displayed
	vector_tpl<gui_veh_info_t *> veh_info;
	// All vehicles we own for a given waytype
	vector_tpl<vehicle_t*> vehicle_we_own;

	// List of upgrades that is displayed
	vector_tpl<gui_upgrade_info_t *> upgrade_info;

	// Array of bool's to keep track of which veh's is selected
	bool* veh_selection;

	// If true, remember to delete "veh_selection"
	bool bool_veh_selection_exists = false;

	// True if any 'veh' is selected, false if none are selected
	bool veh_is_selected;

	// vector of convoy info objects that are being displayed
	//vector_tpl<gui_convoiinfo_t *> cnv_info;



	
	void display(scr_coord pos);

	uint32 amount_desc;
	uint32 amount_veh;
	uint32 amount_veh_owned;

	gui_label_t lb_amount_desc;
	gui_label_t lb_amount_veh;
	gui_label_t lb_desc_page;
	gui_label_t lb_veh_page;

	vehicle_desc_t* desc_for_display = NULL;
	vehicle_desc_t* old_desc_for_display;
	vehicle_desc_t* goto_this_desc = NULL;
	int selected_desc_index;
	int new_count_veh_selection;
	int old_count_veh_selection = -1;
	int new_count_owned_veh;
	int old_count_owned_veh = -1;
	bool show_all_individual_vehicles;

	int page_display_desc = 1;
	int page_display_veh = 1;
	int page_amount_desc = 1;
	int page_amount_veh = 1;

	bool page_turn_desc = false;
	bool page_turn_veh = false;

	static const char *sort_text_desc[SORT_MODES_DESC];
	static const char *sort_text_veh[SORT_MODES_VEH];

	vehicle_desc_t* desc_info_text;
	bool restore_desc_selection = false;

	scr_coord desc_info_text_pos;
	scr_coord scrolly_desc_pos;

	vehicle_desc_t* vehicle_as_upgrade = NULL;
	int selected_upgrade_index;

	int set_desc_scroll_position = 0;
	bool reposition_desc_scroll = false;

public:
	vehicle_manager_t(player_t* player);
	~vehicle_manager_t();

	vehicle_manager_t();

	void build_desc_list();
	void build_veh_list();
	void display_desc_list();
	void display_veh_list();

	void build_upgrade_list();

	void draw_general_information(const scr_coord& pos);
	void draw_maintenance_information(const scr_coord& pos);

	void update_tabs();
	void update_veh_selection();

	void save_previously_selected_desc();

	void sort_desc();
	void sort_veh();
	static bool compare_desc(vehicle_desc_t*, vehicle_desc_t*);
	static bool compare_veh(vehicle_t*, vehicle_t*);

	static bool compare_desc_amount(char*, char*);

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
	//void rdwr( loadsave_t *file );
	uint32 get_rdwr_id();
};

#endif
