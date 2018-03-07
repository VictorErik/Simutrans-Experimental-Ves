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

	gui_speedbar_t filled_bar;

public:
	/**
	* @param cnv, the handler for the Convoi to be displayed.
	* @author Hj. Malthaner
	*/
	gui_vehicle_desc_info_t::gui_vehicle_desc_info_t(vehicle_desc_t* veh);

	bool infowin_event(event_t const*) OVERRIDE;

	/**
	* Draw the component
	* @author Hj. Malthaner
	*/
	void draw(scr_coord offset);
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
	vehiclehandle_t veh;

	gui_speedbar_t filled_bar;

public:
	/**
	* @param cnv, the handler for the Convoi to be displayed.
	* @author Hj. Malthaner
	*/
	gui_vehicle_info_t::gui_vehicle_info_t(vehiclehandle_t veh);

	bool infowin_event(event_t const*) OVERRIDE;

	/**
	* Draw the component
	* @author Hj. Malthaner
	*/
	void draw(scr_coord offset);
};


#endif
#ifndef gui_vehicle_manager_h
#define gui_vehicle_manager_h
class vehicle_manager_t : public gui_frame_t, public action_listener_t
{
private:
	player_t *player;

	gui_container_t cont_veh_desc, cont_veh;
	gui_scrollpane_t scrolly_vehicle_descs, scrolly_vehicles;
	//gui_scrolled_list_t scl;

	// vector of convoy info objects that are being displayed
	vector_tpl<gui_convoiinfo_t *> convoy_infos;

	// All vehicles
	//vector_tpl<quickstone_tpl<vehicle_desc_t>> vehicle_descs;
	vector_tpl<vehiclehandle_t> vehicles;
	vehiclehandle_t veh;
	vector_tpl<vehicle_desc_t*> vehicle_descs;

	vector_tpl<gui_image_list_t::image_data_t*> veh_im;
	waytype_t way_type;


	vector_tpl<gui_vehicle_info_t *> vehicle_infos;
	vector_tpl<gui_vehicle_desc_info_t *> vehicle_desc_infos;
	
	void display(scr_coord pos);




public:
	vehicle_manager_t(player_t* player);
	~vehicle_manager_t();


	void vehicle_manager_t::build_vehicle_lists();

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
