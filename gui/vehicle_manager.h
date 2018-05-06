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

class player_t;

#ifndef gui_vehicle_desc_info_h
#define gui_vehicle_desc_info_h
/**
* One element of the vehicle list display
*
* @author Hj. Malthaner
*/
class gui_vehicle_desc_info_t : public gui_world_component_t
{
private:
	player_t *player;
	uint8 player_nr;
	/**
	* Handle Convois to be displayed.
	* @author Hj. Malthaner
	*/
	vehicle_desc_t* veh;

	uint16 amount = 0;

public:
	/**
	* @param cnv, the handler for the Convoi to be displayed.
	* @author Hj. Malthaner
	*/
	gui_vehicle_desc_info_t::gui_vehicle_desc_info_t(vehicle_desc_t* veh, uint16 amount);

	bool infowin_event(event_t const*) OVERRIDE;
	bool selected = false;

	bool is_selected() { return selected; }
	bool set_selection(bool sel) { return selected = sel; }
	
	/**
	* Draw the component
	* @author Hj. Malthaner
	*/
	void draw(scr_coord offset);

	int image_height = 0;
};

#endif
#ifndef gui_vehicleinfo_h
#define gui_vehicleinfo_h
/**
* One element of the vehicle list display
*
* @author Hj. Malthaner
*/
class gui_vehicle_info_t : public gui_world_component_t
{
private:
	player_t *player;
	/**
	* Handle Convois to be displayed.
	* @author Hj. Malthaner
	*/
	vehicle_t* veh;

	bool selected;

public:
	/**
	* @param cnv, the handler for the Convoi to be displayed.
	* @author Hj. Malthaner
	*/
	gui_vehicle_info_t::gui_vehicle_info_t(vehicle_t* veh);

	bool infowin_event(event_t const*) OVERRIDE;

	/**
	* Draw the component
	* @author Hj. Malthaner
	*/
	void draw(scr_coord offset);

	int image_height = 0;

	bool is_selected() { return selected; }
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
		//by_desc_issues = 3, // Experimental: If the vehicle_desc has any issues, like out of production, can upgrade, etc
		SORT_MODES_DESC = 3
	};

	enum sort_mode_t {
		by_name = 0,
		by_odometer = 1,
		by_issues = 2, // Experimental: If the vehicle has any issues, like out of production, can upgrade, etc
		SORT_MODES = 3
	};
private:
	player_t *player;

	gui_container_t cont_veh_desc, cont_veh, dummy;
	gui_scrollpane_t scrolly_vehicle_descs, scrolly_vehicles;
	gui_tab_panel_t tabs;
	gui_combobox_t combo_sorter_desc;
	gui_combobox_t combo_sorter;

	button_t bt_show_only_owned;
	bool show_only_owned;

	static sort_mode_desc_t sortby_desc;
	static sort_mode_t sortby;
	static bool sortreverse;

	// All vehicles
	//vector_tpl<quickstone_tpl<vehicle_desc_t>> vehicle_descs;
	//vector_tpl<vehiclehandle_t> vehicles;
	//vehicle_t *veh;
	//vector_tpl<gui_image_list_t::image_data_t*> veh_im;


	waytype_t way_type;
	vector_tpl<vehicle_t*> vehicles;
	vector_tpl<vehicle_desc_t*> vehicle_descs;
	vector_tpl<vehicle_desc_t*> vehicle_descs_pr_name;
	vector_tpl<vehicle_t*> selected_vehicles;

	// vector of convoy info objects that are being displayed
	vector_tpl<gui_convoiinfo_t *> convoy_infos;
	vector_tpl<gui_vehicle_info_t *> vehicle_infos;
	vector_tpl<gui_vehicle_desc_info_t *> vehicle_desc_infos;
	
	void display(scr_coord pos);

	uint32 amount_of_vehicle_descs;
	uint32 amount_of_vehicles;

	gui_label_t lb_amount_of_vehicle_descs;
	gui_label_t lb_amount_of_vehicles;

	vehicle_desc_t* vehicle_for_display;
	int selected_index;
	bool show_all_individual_vehicles;

	// This stores the amount of the same kind of vehicles that player owns. Have to be resorted whenever the vehicles are sorted
	uint16 *owned_vehicles = 0;

	static const char *sort_text_desc[SORT_MODES_DESC];
	static const char *sort_text[SORT_MODES];

	char *name_with_amount[512];


public:
	vehicle_manager_t(player_t* player);
	~vehicle_manager_t();

	vehicle_manager_t();

	void vehicle_manager_t::build_vehicle_list();
	void vehicle_manager_t::build_desc_list();
	void vehicle_manager_t::sort_vehicles(bool);

	void vehicle_manager_t::vehicle_on_display();

	static bool vehicle_manager_t::compare_vehicle_desc(vehicle_desc_t*, vehicle_desc_t*);

	static bool vehicle_manager_t::compare_vehicle_desc_amount(char*, char*);

	static void vehicle_manager_t::display_this_vehicle(vehicle_desc_t* veh);

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

	bool infowin_event(event_t const*) OVERRIDE;

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;


	//// following: rdwr stuff
	//void rdwr( loadsave_t *file );
	uint32 get_rdwr_id();
};

#endif
