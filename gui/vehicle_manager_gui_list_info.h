




#include "components/gui_image_list.h"
#include "../simconvoi.h"

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
	int image_height;

public:
	gui_upgrade_info_t(vehicle_desc_t* upgrade, vehicle_desc_t* existing);

	bool infowin_event(event_t const*) OVERRIDE;
	bool selected = false;
	bool open_info = false;

	bool is_selected() { return selected; }
	bool set_selection(bool sel) { return selected = sel; }
	bool is_open_info() { return open_info; }
	int get_image_height() { return image_height; }

	vehicle_desc_t* get_upgrade() { return upgrade; }
	vehicle_desc_t* get_existing() { return existing; }

	/**
	* Draw the component
	* @author Hj. Malthaner
	*/
	void draw(scr_coord offset);

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
	int sort_mode;
	int display_mode;
	/**
	* Handle Convois to be displayed.
	* @author Hj. Malthaner
	*/
	vehicle_desc_t* veh;
	int image_height;

	uint16 amount;

public:
	/**
	* @param cnv, the handler for the Convoi to be displayed.
	* @author Hj. Malthaner
	*/
	gui_desc_info_t(vehicle_desc_t* veh, uint16 amount, int sortmode_index, int displaymode_index);

	bool infowin_event(event_t const*) OVERRIDE;
	bool selected = false;

	bool is_selected() { return selected; }
	bool set_selection(bool sel) { return selected = sel; }
	vehicle_desc_t* get_desc() { return veh; }
	int get_image_height() { return image_height; }

	/**
	* Draw the component
	* @author Hj. Malthaner
	*/
	void draw(scr_coord offset);

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
	int image_height;

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

	int get_image_height() { return image_height; }
};
#endif