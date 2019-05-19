/*
 * New OO tool system
 *
 * This file is part of the Simutrans project under the artistic license.
 */

#ifndef SIMTOOL_H
#define SIMTOOL_H

#include "simtypes.h"
#include "simworld.h"
#include "simmenu.h"
#include "simobj.h"

#include "boden/wege/schiene.h"
#include "boden/wege/strasse.h"

#include "dataobj/environment.h"
#include "dataobj/translator.h"

#include "obj/baum.h"

#include "player/simplay.h"

#include "tpl/slist_tpl.h"

class koord3d;
class koord;
class way_builder_t;
class building_desc_t;
class roadsign_desc_t;
class way_desc_t;
class route_t;
class way_obj_desc_t;

/****************************** helper functions: *****************************/

char *tooltip_with_price(const char * tip, sint64 price);

/************************ general tool *******************************/

// query tile info: default tool
class tool_query_t : public tool_t {
public:
	tool_query_t() : tool_t(TOOL_QUERY | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Abfrage"); }
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};


// remove uppermost object from tile
class tool_remover_t : public tool_t {
private:
	bool tool_remover_intern(player_t *player, koord3d pos, sint8 type, const char *&msg);
public:
	tool_remover_t() : tool_t(TOOL_REMOVER | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Abriss"); }
	char const* check_diversionary_route(koord3d pos, weg_t* w, player_t* player);
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

// alter land height tools
class tool_raise_lower_base_t : public tool_t {
protected:
	bool is_dragging;
	sint16 drag_height;

	const char* drag(player_t*, koord k, sint16 h, int &n, bool allow_deep_water);
	virtual sint16 get_drag_height(koord k) = 0;
	bool check_dragging();

public:
	tool_raise_lower_base_t(uint16 id) : tool_t(id | GENERAL_TOOL), is_dragging(false), drag_height(0) { offset = Z_GRID; }
	image_id get_icon(player_t*) const OVERRIDE { return grund_t::underground_mode==grund_t::ugm_all ? IMG_EMPTY : icon; }
	bool init(player_t*) OVERRIDE { is_dragging = false; return true; }
	bool exit(player_t*) OVERRIDE { is_dragging = false; return true; }
	/**
	 * technically move is not network safe, however its implementation is:
	 * it sends work commands over network itself
	 */
	char const* move(player_t*, uint16 /* buttonstate */, koord3d) OVERRIDE;
	bool move_has_effects() const OVERRIDE { return true; }

	bool is_init_network_save() const OVERRIDE { return true; }
	/**
	 * work() is only called when not dragging
	 * if work() is called with is_dragging==true then is_dragging is reset
	 */
	bool is_work_network_save() const OVERRIDE { return is_dragging;}

	/**
	 * @return true if this tool operates over the grid, not the map tiles.
	 */
	bool is_grid_tool() const {return true;}

	bool update_pos_after_use() const OVERRIDE { return true; }
};

class tool_raise_t : public tool_raise_lower_base_t {
public:
	tool_raise_t() : tool_raise_lower_base_t(TOOL_RAISE_LAND) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return tooltip_with_price("Anheben", welt->get_settings().cst_alter_land); }
	char const* check_pos(player_t*, koord3d) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	sint16 get_drag_height(koord k) OVERRIDE;
};

class tool_lower_t : public tool_raise_lower_base_t {
public:
	tool_lower_t() : tool_raise_lower_base_t(TOOL_LOWER_LAND) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return tooltip_with_price("Absenken", welt->get_settings().cst_alter_land); }
	char const* check_pos(player_t*, koord3d) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	sint16 get_drag_height(koord k) OVERRIDE;
};

/* slope tool definitions */
class tool_setslope_t : public tool_t {
public:
	tool_setslope_t() : tool_t(TOOL_SETSLOPE | GENERAL_TOOL) {}
	/**
	* Create an artificial slope
	* @param player the player doing the task
	* @param pos position where the slope will be generated
	* @param slope the slope type
	* @author Hj. Malthaner
	*/
	static const char *tool_set_slope_work( player_t *player, koord3d pos, int slope );
	char const* get_tooltip(player_t const*) const OVERRIDE { return tooltip_with_price("Built artifical slopes", welt->get_settings().cst_set_slope); }
	bool is_init_network_save() const OVERRIDE { return true; }
	char const* check_pos(player_t*, koord3d) OVERRIDE;
	char const* work(player_t* const player, koord3d const k) OVERRIDE { return tool_set_slope_work(player, k, atoi(default_param)); }
};

class tool_restoreslope_t : public tool_t {
public:
	tool_restoreslope_t() : tool_t(TOOL_RESTORESLOPE | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return tooltip_with_price("Restore natural slope", welt->get_settings().cst_set_slope); }
	bool is_init_network_save() const OVERRIDE { return true; }
	char const* check_pos(player_t*, koord3d) OVERRIDE;
	char const* work(player_t* const player, koord3d const k) OVERRIDE { return tool_setslope_t::tool_set_slope_work(player, k, RESTORE_SLOPE); }
};

class tool_marker_t : public kartenboden_tool_t {
public:
	tool_marker_t() : kartenboden_tool_t(TOOL_MARKER | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return tooltip_with_price("Marker", welt->get_settings().cst_buy_land); }
char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

class tool_clear_reservation_t : public tool_t {
public:
	tool_clear_reservation_t() : tool_t(TOOL_CLEAR_RESERVATION | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Clear block reservation"); }
	bool init(player_t*) OVERRIDE;
	bool exit(player_t*) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

class tool_transformer_t : public kartenboden_tool_t {
private:
	bool is_powerline_available() const;
public:
	tool_transformer_t() : kartenboden_tool_t(TOOL_TRANSFORMER | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE;
	image_id get_icon(player_t*) const OVERRIDE;
	bool init(player_t*) OVERRIDE;
	char const* check_pos(player_t*, koord3d) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	waytype_t get_waytype() const OVERRIDE { return powerline_wt; }
};

class tool_add_city_t : public kartenboden_tool_t {
public:
	tool_add_city_t() : kartenboden_tool_t(TOOL_ADD_CITY | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return tooltip_with_price("Found new city", welt->get_settings().cst_found_city); }
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

// buy a house to protect it from renovating
class tool_buy_house_t : public kartenboden_tool_t {
public:
	tool_buy_house_t() : kartenboden_tool_t(TOOL_BUY_HOUSE | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Haus kaufen"); }
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};
/************** the following tools need a valid default_param ************************/

// step size by default_param
class tool_change_city_size_t : public tool_t {
public:
	tool_change_city_size_t() : tool_t(TOOL_CHANGE_CITY_SIZE | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate( atoi(default_param)>=0 ? "Grow city" : "Shrink city" ); }
	bool init(player_t*) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

// height change by default_param
class tool_change_water_height_t : public tool_t {
public:
	tool_change_water_height_t() : tool_t(TOOL_CHANGE_WATER_HEIGHT | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate( atoi(default_param)>=0 ? "Increase water height" : "Decrease water height" ); }
	bool init(player_t*) OVERRIDE;
	image_id get_icon(player_t *player) const OVERRIDE { return (!env_t::networkmode  ||  player->is_public_service()) ? icon : IMG_EMPTY; }
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

// height change by default_param
class tool_set_climate_t : public two_click_tool_t {
public:
	tool_set_climate_t() : two_click_tool_t(TOOL_SET_CLIMATE | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE;
private:
	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

class tool_plant_tree_t : public kartenboden_tool_t {
public:
	tool_plant_tree_t() : kartenboden_tool_t(TOOL_PLANT_TREE | GENERAL_TOOL) {}
	image_id get_icon(player_t *) const { return baum_t::get_count() > 0 ? icon : IMG_EMPTY; }
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate( "Plant tree" ); }
	bool init(player_t*) { return baum_t::get_count() > 0; }
	char const* move(player_t* const player, uint16 const b, koord3d const k) OVERRIDE;
	bool move_has_effects() const OVERRIDE { return true; }
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

/* only called directly from schedule => no tooltip!
 * default_param must point to a schedule!
 */
class tool_schedule_add_t : public tool_t {
public:
	tool_schedule_add_t() : tool_t(TOOL_SCHEDULE_ADD | GENERAL_TOOL) {}
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_schedule_ins_t : public tool_t {
public:
	tool_schedule_ins_t() : tool_t(TOOL_SCHEDULE_INS | GENERAL_TOOL) {}
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_build_way_t : public two_click_tool_t {
public:
	static const way_desc_t *defaults[18];	// default ways for all types
private:
	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;

protected:
	const way_desc_t *desc;
	overtaking_mode_t overtaking_mode;
	bool look_toolbar = false;

	virtual way_desc_t const* get_desc(uint16, bool) const;
	void calc_route( way_builder_t &bauigel, const koord3d &, const koord3d & );

public:
	tool_build_way_t(uint16 const id = TOOL_BUILD_WAY | GENERAL_TOOL) : two_click_tool_t(id), desc() {
		overtaking_mode = twoway_mode;
	}
	image_id get_icon(player_t*) const OVERRIDE;
	char const* get_tooltip(player_t const*) const OVERRIDE;
	char const* get_default_param(player_t*) const OVERRIDE;
	bool is_selected() const OVERRIDE;
	void rdwr_custom_data(memory_rw_t*) OVERRIDE;
	bool init(player_t* player) OVERRIDE { return init(player, false); }
	bool init(player_t*,bool called_from_move);
	bool exit(player_t*) OVERRIDE;
	void draw_after(scr_coord, bool dirty) const OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	waytype_t get_waytype() const OVERRIDE;
	// remove preview necessary while building elevated ways
	bool remove_preview_necessary() const OVERRIDE { return !is_first_click()  &&  (desc  &&  (desc->get_styp() == type_elevated  &&  desc->get_wtyp() != air_wt)); }
	void set_overtaking_mode(overtaking_mode_t ov) { overtaking_mode = ov; }
	overtaking_mode_t get_overtaking_mode() const { return overtaking_mode; }
	static void set_mode_str(char* str, overtaking_mode_t overtaking_mode);
	void set_look_toolbar() { look_toolbar = true; }
};

class tool_build_cityroad : public tool_build_way_t {
private:
	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
public:
	tool_build_cityroad() : tool_build_way_t(TOOL_BUILD_CITYROAD | GENERAL_TOOL) {}
	way_desc_t const* get_desc(uint16, bool) const OVERRIDE;
	image_id get_icon(player_t* const player) const OVERRIDE { return tool_t::get_icon(player); }
	bool is_selected() const OVERRIDE { return tool_t::is_selected(); }
	bool is_init_network_save() const OVERRIDE { return true; }
	waytype_t get_waytype() const OVERRIDE { return road_wt; }
};

class tool_build_bridge_t : public two_click_tool_t {
private:
	ribi_t::ribi ribi;
	const way_desc_t* way_desc;
	overtaking_mode_t overtaking_mode;

	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;
public:
	tool_build_bridge_t() : two_click_tool_t(TOOL_BUILD_BRIDGE | GENERAL_TOOL) {
		way_desc = NULL;
		overtaking_mode = twoway_mode;
	}
	image_id get_icon(player_t*) const OVERRIDE { return grund_t::underground_mode==grund_t::ugm_all ? IMG_EMPTY : icon; }
	char const* get_tooltip(player_t const*) const OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	waytype_t get_waytype() const OVERRIDE;
	bool remove_preview_necessary() const OVERRIDE { return !is_first_click(); }
	void rdwr_custom_data(memory_rw_t*) OVERRIDE;
	bool init(player_t*) OVERRIDE;
	bool exit(player_t*) OVERRIDE;
	void draw_after(scr_coord, bool dirty) const OVERRIDE;
	void set_overtaking_mode(overtaking_mode_t ov) { overtaking_mode = ov; }
	overtaking_mode_t get_overtaking_mode() const { return overtaking_mode; }
};

class tool_build_tunnel_t : public two_click_tool_t {
private:
	const way_desc_t* way_desc;
	overtaking_mode_t overtaking_mode;

	void calc_route( way_builder_t &bauigel, const koord3d &, const koord3d &);
	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;
public:
	tool_build_tunnel_t();
	char const* get_tooltip(player_t const*) const OVERRIDE;
	char const* check_pos(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	waytype_t get_waytype() const OVERRIDE;
	bool remove_preview_necessary() const OVERRIDE { return !is_first_click(); }
	void rdwr_custom_data(memory_rw_t*) OVERRIDE;
	bool init(player_t*) OVERRIDE;
	bool exit(player_t*) OVERRIDE;
	void draw_after(scr_coord, bool dirty) const OVERRIDE;
	void set_overtaking_mode(overtaking_mode_t ov) { overtaking_mode = ov; }
	overtaking_mode_t get_overtaking_mode() const { return overtaking_mode; }
};

class tool_wayremover_t : public two_click_tool_t {
private:
	bool calc_route( route_t &, player_t *, const koord3d& start, const koord3d &to );
	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;
	// error message to be returned by calc_route
	const char *calc_route_error;
public:
	tool_wayremover_t() : two_click_tool_t(TOOL_WAYREMOVER | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE;
	image_id get_icon(player_t*) const OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	waytype_t get_waytype() const OVERRIDE;
};

class tool_change_roadsign_t : public tool_t {
public:
	tool_change_roadsign_t() : tool_t(TOOL_CHANGE_ROADSIGN | SIMPLE_TOOL) {}
	bool init(player_t*) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return false; }
};

class tool_build_wayobj_t : public two_click_tool_t {
protected:
	const bool build;
private:
	static const way_obj_desc_t *default_electric;
	const way_obj_desc_t *desc;
	const way_obj_desc_t *get_desc() const;
	waytype_t wt;

	bool calc_route( route_t &, player_t *, const koord3d& start, const koord3d &to );

	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;

public:
	tool_build_wayobj_t(uint16 const id = TOOL_BUILD_WAYOBJ | GENERAL_TOOL, bool b = true) : two_click_tool_t(id), build(b) {}
	char const* get_tooltip(player_t const*) const OVERRIDE;
	bool is_selected() const OVERRIDE;
	bool init(player_t*) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	waytype_t get_waytype() const OVERRIDE;
};

class tool_remove_wayobj_t : public tool_build_wayobj_t {
public:
	tool_remove_wayobj_t() : tool_build_wayobj_t(TOOL_REMOVE_WAYOBJ | GENERAL_TOOL, false) {}
	bool is_selected() const OVERRIDE { return tool_t::is_selected(); }
	bool is_init_network_save() const OVERRIDE { return true; }
};

class tool_build_station_t : public tool_t {
private:
	const char *tool_station_building_aux(player_t *, bool, koord3d, const building_desc_t *, sint8 rotation );
	const char *tool_station_dock_aux(player_t *, koord3d, const building_desc_t * );
	const char *tool_station_flat_dock_aux(player_t *, koord3d, const building_desc_t *, sint8 );
	const char *tool_station_aux(player_t *, koord3d, const building_desc_t *, waytype_t, const char *halt_suffix );
	const building_desc_t *get_desc( sint8 &rotation ) const;

public:
	tool_build_station_t() : tool_t(TOOL_BUILD_STATION | GENERAL_TOOL) {}
	image_id get_icon(player_t*) const OVERRIDE;
	char const* get_tooltip(player_t const*) const OVERRIDE;
	bool init(player_t*) OVERRIDE;
	char const* check_pos(player_t*, koord3d) OVERRIDE;
	char const* move(player_t*, uint16 /* buttonstate */, koord3d) OVERRIDE;
	bool move_has_effects() const OVERRIDE { return true; }
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	waytype_t get_waytype() const OVERRIDE;
};

// builds roadsigns and signals
class tool_build_roadsign_t : public two_click_tool_t {

private:
	const roadsign_desc_t* desc;
	const char *place_sign_intern( player_t *, grund_t*, const roadsign_desc_t* b = NULL);

public:
	struct signal_info {
		signal_info() : remove_intermediate(true), replace_other(true), signalbox(koord3d::invalid) {}

		static uint8 spacing; // place signals every n tiles
		bool  remove_intermediate;
		bool  replace_other;
		koord3d signalbox;
	} signal[MAX_PLAYER_COUNT];

	void rotate90(sint16 y_diff);

private:
	static char toolstring[256];
	// read the variables from the default_param
	void read_default_param(player_t *player);

	const char* check_pos_intern(player_t *, koord3d);
	bool calc_route( route_t &, player_t *, const koord3d& start, const koord3d &to );

	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;

	/// save direction of new signs
	vector_tpl< ribi_t::ribi > directions;

public:
	tool_build_roadsign_t() : two_click_tool_t(TOOL_BUILD_ROADSIGN | GENERAL_TOOL), desc() {}

	char const* get_tooltip(player_t const*) const OVERRIDE;
	bool init(player_t*) OVERRIDE;
	bool exit(player_t*) OVERRIDE;

	void set_values(player_t *player, uint8 spacing, bool remove, bool replace, koord3d signalbox );
	void get_values(player_t *player, uint8 &spacing, bool &remove, bool &replace, koord3d &signalbox );
	bool is_init_network_save() const OVERRIDE { return true; }
	void draw_after(scr_coord, bool dirty) const OVERRIDE;
	char const* get_default_param(player_t*) const OVERRIDE;
	waytype_t get_waytype() const OVERRIDE;
};

class tool_build_depot_t : public tool_t {
private:
	static char toolstring[256];
	const char *tool_depot_aux(player_t *player, koord3d pos, const building_desc_t *desc, waytype_t wegtype, sint64 cost);
public:
	tool_build_depot_t() : tool_t(TOOL_BUILD_DEPOT | GENERAL_TOOL) {}
	image_id get_icon(player_t*) const OVERRIDE;
	char const* get_tooltip(player_t const*) const OVERRIDE;
	bool init(player_t*) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	waytype_t get_waytype() const OVERRIDE;
};

class tool_signalbox_t : public tool_t
{
private:
	static char toolstring[256];
	const char* tool_signalbox_aux(player_t* player, koord3d pos, const building_desc_t* desc, sint64 cost);
public:
	tool_signalbox_t() : tool_t(TOOL_BUILD_SIGNALBOX | GENERAL_TOOL) {}
	const char *check_pos(player_t *, koord3d pos);
	image_id get_icon(player_t*) const OVERRIDE;
	char const* get_tooltip(player_t const*) const OVERRIDE;
	bool init(player_t*) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

/* builds (random) tourist attraction (default_param==NULL) and maybe adds it to the next city
 * the parameter string is a follow (or NULL):
 * 1#theater
 * first letter: ignore climates
 * second letter: rotation (0,1,2,3,#=random)
 * finally building name
 * @author prissi
 */
class tool_build_house_t : public kartenboden_tool_t {
public:
	tool_build_house_t() : kartenboden_tool_t(TOOL_BUILD_HOUSE | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Built random attraction"); }
	bool init(player_t*) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

/* build an (if param=NULL random) industry chain starting here *
 * the parameter string is a follow (or NULL):
 * 1#34,oelfeld
 * first letter: ignore climates
 * second letter: rotation (0,1,2,3,#=random)
 * next number is production value
 * finally industry name
 * NULL means random chain!
 */
class tool_build_land_chain_t : public kartenboden_tool_t {
public:
	tool_build_land_chain_t() : kartenboden_tool_t(TOOL_BUILD_LAND_CHAIN | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Build land consumer"); }
	bool init(player_t*) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

class tool_city_chain_t : public kartenboden_tool_t {
public:
	tool_city_chain_t() : kartenboden_tool_t(TOOL_CITY_CHAIN | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Build city market"); }
	bool init(player_t*) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

class tool_build_factory_t : public kartenboden_tool_t {
public:
	tool_build_factory_t() : kartenboden_tool_t(TOOL_BUILD_FACTORY | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Build city market"); }
	bool init(player_t*) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

class tool_link_factory_t : public two_click_tool_t {
public:
	tool_link_factory_t() : two_click_tool_t(TOOL_LINK_FACTORY | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Connect factory"); }
	bool is_init_network_save() const OVERRIDE { return true; }
private:
	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE {}
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;
	image_id get_marker_image() OVERRIDE;
};

class tool_headquarter_t : public kartenboden_tool_t {
private:
	const building_desc_t *next_level( const player_t *player ) const;
public:
	tool_headquarter_t() : kartenboden_tool_t(TOOL_HEADQUARTER | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE;
	bool init(player_t*) OVERRIDE;
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

/* protects map from further change (here because two clicks to confirm it!) */
class tool_lock_game_t : public tool_t {
public:
	tool_lock_game_t() : tool_t(TOOL_LOCK_GAME | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return env_t::networkmode ? translator::translate("deactivated in online mode") : translator::translate("Lock game"); }
	image_id get_icon(player_t*) const OVERRIDE { return env_t::networkmode ? IMG_EMPTY : icon; }
	// deactivate in network mode
	bool init(player_t *) { return !env_t::networkmode; }
	const char *work( player_t *, koord3d );
	bool is_init_network_save() const OVERRIDE { return true; }
};

/* add random citycar if no default is set; else add a certain city car */
class tool_add_citycar_t : public tool_t {
public:
	tool_add_citycar_t() : tool_t(TOOL_ADD_CITYCAR | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Add random citycar"); }
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

/* make forest */
class tool_forest_t : public two_click_tool_t {
public:
	tool_forest_t() : two_click_tool_t(TOOL_FOREST | GENERAL_TOOL) {}
	image_id get_icon(player_t *) const { return baum_t::get_count() > 0 ? icon : IMG_EMPTY; }
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Add forest"); }
	bool init( player_t *player) { return  baum_t::get_count() > 0  &&  two_click_tool_t::init(player); }
private:
	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};

/* stop moving tool */
class tool_stop_mover_t : public two_click_tool_t {
private:
	waytype_t waytype[2];
	halthandle_t last_halt;
public:
	tool_stop_mover_t() : two_click_tool_t(TOOL_STOP_MOVER | GENERAL_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("replace stop"); }
	bool is_init_network_save() const OVERRIDE { return true; }

private:
	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE {}
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;
	image_id get_marker_image() OVERRIDE;

	void read_start_position(player_t *player, const koord3d &pos);
};

/* Signal reassignment tool */
class tool_reassign_signal_t : public two_click_tool_t
{
private:
	koord3d last_selected_location;
public:
	tool_reassign_signal_t() : two_click_tool_t(TOOL_REASSIGN_SIGNAL | GENERAL_TOOL) { last_selected_location = koord3d::invalid; }
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Reassign signals"); }
	bool is_init_network_save() const OVERRIDE { return true; }

private:
	char const* do_work(player_t*, koord3d const&, koord3d const&) OVERRIDE;
	void mark_tiles(player_t*, koord3d const&, koord3d const&) OVERRIDE {}
	uint8 is_valid_pos(player_t*, koord3d const&, char const*&, koord3d const&) OVERRIDE;
	image_id get_marker_image() OVERRIDE;

	void read_start_position(const koord3d &pos);
};

/* make all tiles of this player a public stop
 * if this player is public, make all connected tiles a public stop */
class tool_make_stop_public_t : public tool_t {
public:
	tool_make_stop_public_t() : tool_t(TOOL_MAKE_STOP_PUBLIC | GENERAL_TOOL) {}
	bool init(player_t * );
	bool exit(player_t *s ) { return init(s); }
	char const* get_tooltip(player_t const*) const OVERRIDE;
	char const* move(player_t*, uint16 /* buttonstate */, koord3d) OVERRIDE;
	bool move_has_effects() const OVERRIDE { return true; }
	char const* work(player_t*, koord3d) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
};


// internal tool: show error message at specific coordinate
// used for scenario error messages send by server
class tool_error_message_t : public tool_t {
public:
	tool_error_message_t() : tool_t(TOOL_ERROR_MESSAGE | GENERAL_TOOL) {}
	bool init(player_t*) OVERRIDE { return true; }
	bool is_init_network_save() const OVERRIDE { return true; }
	char const* work(player_t*, koord3d) OVERRIDE { return default_param ? default_param : ""; }
};

/********************* one click tools ****************************/

class tool_pause_t : public tool_t {
public:
	tool_pause_t() : tool_t(TOOL_PAUSE | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return env_t::networkmode ? translator::translate("deactivated in online mode") : translator::translate("Pause"); }
	image_id get_icon(player_t*) const OVERRIDE { return env_t::networkmode ? IMG_EMPTY : icon; }
	bool is_selected() const OVERRIDE { return welt->is_paused(); }
	bool init( player_t * ) {
		if(  !env_t::networkmode  ) {
			welt->set_fast_forward(0);
			welt->set_pause( welt->is_paused()^1 );
		}
		return false;
	}
	bool exit(player_t *s ) { return init(s); }
	bool is_init_network_save() const OVERRIDE { return !env_t::networkmode; }
	bool is_work_network_save() const OVERRIDE { return !env_t::networkmode; }
};

class tool_fastforward_t : public tool_t {
public:
	tool_fastforward_t() : tool_t(TOOL_FASTFORWARD | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return env_t::networkmode ? translator::translate("deactivated in online mode") : translator::translate("Fast forward"); }
	image_id get_icon(player_t*) const OVERRIDE { return env_t::networkmode ? IMG_EMPTY : icon; }
	bool is_selected() const OVERRIDE { return welt->is_fast_forward(); }
	bool init( player_t * ) {
		if(  !env_t::networkmode  ) {
			if(  welt->is_fast_forward()  &&  env_t::simple_drawing_fast_forward  ) {
				welt->set_dirty();
			}
			welt->set_pause(0);
			welt->set_fast_forward( welt->is_fast_forward()^1 );
		}
		return false;
	}
	bool exit(player_t *s ) { return init(s); }
	bool is_init_network_save() const OVERRIDE { return !env_t::networkmode; }
	bool is_work_network_save() const OVERRIDE { return !env_t::networkmode; }
};

class tool_screenshot_t : public tool_t {
public:
	tool_screenshot_t() : tool_t(TOOL_SCREENSHOT | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Screenshot"); }
	bool init(player_t * ) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

// build next chain
class tool_increase_industry_t : public tool_t {
public:
	tool_increase_industry_t() : tool_t(TOOL_INCREASE_INDUSTRY | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Increase Industry density"); }
	bool init( player_t * );
};

/* prissi: undo building */
class tool_undo_t : public tool_t {
public:
	tool_undo_t() : tool_t(TOOL_UNDO | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Undo last ways construction"); }
	bool init(player_t *player ) OVERRIDE;
};

/* switch to next player
 * @author prissi
 */
class tool_switch_player_t : public tool_t {
public:
	tool_switch_player_t() : tool_t(TOOL_SWITCH_PLAYER | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Change player"); }
	bool init( player_t * ) {
		welt->switch_active_player( welt->get_active_player_nr()+1, true );
		return false;
	}
	// since it is handled internally
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

// step one year forward
class tool_step_year_t : public tool_t {
public:
	tool_step_year_t() : tool_t(TOOL_STEP_YEAR | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Step timeline one year"); }
	bool init( player_t * ) {
		welt->step_year();
		return false;
	}
};

class tool_change_game_speed_t : public tool_t {
public:
	tool_change_game_speed_t() : tool_t(TOOL_CHANGE_GAME_SPEED | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE {
		int factor = atoi(default_param);
		return factor>0 ? translator::translate("Accelerate time") : translator::translate("Deccelerate time");
	}
	bool init( player_t *player ) {
		if(  !env_t::networkmode  ||  player->get_player_nr()==1  ) {
			// in networkmode only for public player
			welt->change_time_multiplier( atoi(default_param) );
		}
		return false;
	}
};


class tool_zoom_in_t : public tool_t {
public:
	tool_zoom_in_t() : tool_t(TOOL_ZOOM_IN | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("zooming in"); }
	bool init( player_t * ) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_zoom_out_t : public tool_t {
public:
	tool_zoom_out_t() : tool_t(TOOL_ZOOM_OUT | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("zooming out"); }
	bool init( player_t * ) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_show_coverage_t : public tool_t {
public:
	tool_show_coverage_t() : tool_t(TOOL_SHOW_COVERAGE | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("show station coverage"); }
	bool is_selected() const OVERRIDE { return env_t::station_coverage_show; }
	bool init( player_t * ) {
		env_t::station_coverage_show = !env_t::station_coverage_show;
		welt->set_dirty();
		return false;
	}
	bool exit(player_t *s ) { return init(s); }
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_show_name_t : public tool_t {
public:
	tool_show_name_t() : tool_t(TOOL_SHOW_NAME | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE {
		return translator::translate(
			(env_t::show_names>>2)==2 ? "hide station names" :
			(env_t::show_names&1) ? "show waiting bars" : "show station names");
	}
	bool init( player_t * ) {
		if(  env_t::show_names>=11  ) {
			if(  (env_t::show_names&3)==3  ) {
				env_t::show_names = 0;
			}
			else {
				env_t::show_names = 2;
			}
		}
		else if(  env_t::show_names==2  ) {
				env_t::show_names = 3;
		}
		else if(  env_t::show_names==0  ) {
			env_t::show_names = 1;
		}
		else {
			env_t::show_names += 4;
		}
		welt->set_dirty();
		return false;
	}
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_show_grid_t : public tool_t {
public:
	tool_show_grid_t() : tool_t(TOOL_SHOW_GRID | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("show grid"); }
	bool is_selected() const OVERRIDE { return grund_t::show_grid; }
	bool init( player_t * ) {
		grund_t::toggle_grid();
		welt->set_dirty();
		return false;
	}
	bool exit(player_t *s ) { return init(s); }
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_show_trees_t : public tool_t {
public:
	tool_show_trees_t() : tool_t(TOOL_SHOW_TREES | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("hide trees"); }
	bool is_selected() const OVERRIDE {return env_t::hide_trees; }
	bool init( player_t * );
	bool exit(player_t *s ) { return init(s); }
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_show_houses_t : public tool_t {
public:
	tool_show_houses_t() : tool_t(TOOL_SHOW_HOUSES | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE {
		return translator::translate(
			env_t::hide_buildings==0 ? "hide city building" :
			(env_t::hide_buildings==1) ? "hide all building" : "show all building");
	}
	bool init( player_t * ) {
		env_t::hide_buildings ++;
		if(env_t::hide_buildings>env_t::ALL_HIDDEN_BUILDING) {
			env_t::hide_buildings = env_t::NOT_HIDE;
		}
		welt->set_dirty();
		return false;
	}
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_show_underground_t : public tool_t {
public:
	tool_show_underground_t() : tool_t(TOOL_SHOW_UNDERGROUND | SIMPLE_TOOL) {}
	static sint8 save_underground_level;
	char const* get_tooltip(player_t const*) const OVERRIDE;
	bool is_selected() const OVERRIDE;
	void draw_after(scr_coord, bool dirty) const OVERRIDE;
	bool init( player_t * );
	char const* work(player_t*, koord3d) OVERRIDE;
	bool exit(player_t * ) { return false; }
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_rotate90_t : public tool_t {
public:
	tool_rotate90_t() : tool_t(TOOL_ROTATE90 | SIMPLE_TOOL) {}
	image_id get_icon(player_t*) const OVERRIDE { return env_t::networkmode ? IMG_EMPTY : icon; }
	char const* get_tooltip(player_t const*) const OVERRIDE { return env_t::networkmode ? translator::translate("deactivated in online mode") : translator::translate("Rotate map"); }
	bool init( player_t * ) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return !env_t::networkmode; }
	bool is_work_network_save() const OVERRIDE { return !env_t::networkmode; }
};

class tool_quit_t : public tool_t {
public:
	tool_quit_t() : tool_t(TOOL_QUIT | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Beenden"); }
	bool init( player_t * ) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

// step size by default_param
class tool_fill_trees_t : public tool_t {
public:
	tool_fill_trees_t() : tool_t(TOOL_FILL_TREES | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Fill trees"); }
	image_id get_icon(player_t *) const { return baum_t::get_count() > 0 ? icon : IMG_EMPTY; }
	bool init(player_t * ) {
		if(  baum_t::get_count() > 0  &&  default_param  ) {
			baum_t::fill_trees( atoi(default_param) );
		}
		return false;
	}
};

/* change day/night view manually */
class tool_daynight_level_t : public tool_t {
public:
	tool_daynight_level_t() : tool_t(TOOL_DAYNIGHT_LEVEL | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE;
	bool init(player_t * );
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_vehicle_tooltips_t : public tool_t {
public:
	tool_vehicle_tooltips_t() : tool_t(TOOL_VEHICLE_TOOLTIPS | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("Toggle vehicle tooltips"); }
	bool init( player_t * ) {
		env_t::show_vehicle_states = (env_t::show_vehicle_states+1)%3;
		welt->set_dirty();
		return false;
	}
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_toggle_pax_station_t : public tool_t {
public:
	tool_toggle_pax_station_t() : tool_t(TOOL_TOOGLE_PAX | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("5LIGHT_CHOOSE"); }
	bool is_selected() const OVERRIDE { return welt->get_settings().get_show_pax(); }
	bool init( player_t * ) {
		if( !env_t::networkmode) {
			settings_t& s = welt->get_settings();
			s.set_show_pax(!s.get_show_pax());
		}
		return false;
	}
	bool exit(player_t *s ) { return init(s); }
	bool is_init_network_save() const OVERRIDE { return false; }
};

class tool_toggle_pedestrians_t : public tool_t {
public:
	tool_toggle_pedestrians_t() : tool_t(TOOL_TOOGLE_PEDESTRIANS | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("6LIGHT_CHOOSE"); }
	bool is_selected() const OVERRIDE { return welt->get_settings().get_random_pedestrians(); }
	bool init( player_t * ) {
		if( !env_t::networkmode) {
			settings_t& s = welt->get_settings();
			s.set_random_pedestrians(!s.get_random_pedestrians());
		}
		return false;
	}
	bool exit(player_t *s ) { return init(s); }
	bool is_init_network_save() const OVERRIDE { return false; }
};

class tool_toggle_reservation_t : public tool_t {
public:
	tool_toggle_reservation_t() : tool_t(TOOL_TOGGLE_RESERVATION | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("show/hide block reservations"); }
	bool is_selected() const OVERRIDE { return schiene_t::show_reservations; }
	bool init( player_t * ) {
		schiene_t::show_reservations ^= 1;
		welt->set_dirty();
		return false;
	}
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_show_ribi_t : public tool_t {
public:
	tool_show_ribi_t() : tool_t(TOOL_SHOW_RIBI| SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("view masked ribi"); }
	bool is_selected() const OVERRIDE { return strasse_t::show_masked_ribi; }
	bool init( player_t * ) {
		if(  skinverwaltung_t::ribi_arrow  ) {
			strasse_t::show_masked_ribi ^= 1;
			welt->set_dirty();
		} else {
			// no ribi_arrow pak.
			dbg->warning("tool_show_ribi_t::init()", "the ribi_arrow pak is not installed!");
		}
		return false;
	}
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_view_owner_t : public tool_t {
public:
	tool_view_owner_t() : tool_t(TOOL_VIEW_OWNER | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("show/hide object owner"); }
	bool is_selected() const OVERRIDE { return obj_t::show_owner; }
	bool init( player_t * ) {
		obj_t::show_owner ^= 1;
		welt->set_dirty();
		return false;
	}
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

class tool_hide_under_cursor_t : public tool_t {
public:
	tool_hide_under_cursor_t() : tool_t(TOOL_HIDE_UNDER_CURSOR | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("hide objects under cursor"); }
	bool is_selected() const OVERRIDE { return env_t::hide_under_cursor; }
	bool init( player_t * ) {
		env_t::hide_under_cursor = !env_t::hide_under_cursor  &&  env_t::cursor_hide_range>0;
		welt->set_dirty();
		return false;
	}
	bool exit(player_t *s ) { return init(s); }
	bool is_init_network_save() const OVERRIDE { return true; }
	bool is_work_network_save() const OVERRIDE { return true; }
};

/******************************** Internal tools ***********/
/* internal simple tools needed for network synchronisation*/
class tool_traffic_level_t : public tool_t {
public:
	tool_traffic_level_t() : tool_t(TOOL_TRAFFIC_LEVEL | SIMPLE_TOOL) {}
	char const* get_tooltip(player_t const*) const OVERRIDE { return translator::translate("6WORLD_CHOOSE"); }
	bool is_selected() const OVERRIDE { return false; }
	bool init( player_t * ) {
		assert(  default_param  );
		sint16 level = min( max( atoi(default_param), 0), 16);
		welt->get_settings().set_traffic_level(level);
		return false;
	}
	bool is_init_network_save() const OVERRIDE { return false; }
};

class tool_change_convoi_t : public tool_t {
public:
	tool_change_convoi_t() : tool_t(TOOL_CHANGE_CONVOI | SIMPLE_TOOL) {}
	bool init(player_t*) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return false; }
};

class tool_change_line_t : public tool_t {
public:
	tool_change_line_t() : tool_t(TOOL_CHANGE_LINE | SIMPLE_TOOL) {}
	bool init(player_t*) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return false; }
};

class tool_change_depot_t : public tool_t {
public:
	tool_change_depot_t() : tool_t(TOOL_CHANGE_DEPOT | SIMPLE_TOOL) {}
	bool init(player_t*) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return false; }
};

// adds a new player of certain type to the map
class tool_change_player_t : public tool_t {
public:
	tool_change_player_t() : tool_t(TOOL_CHANGE_PLAYER | SIMPLE_TOOL) {}
	bool init(player_t*) OVERRIDE;
};

// change timing of traffic light
class tool_change_traffic_light_t : public tool_t {
public:
	tool_change_traffic_light_t() : tool_t(TOOL_CHANGE_TRAFFIC_LIGHT | SIMPLE_TOOL) {}
	bool init(player_t*) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return false; }
};

// change city: (dis)allow growth
class tool_change_city_t : public tool_t {
public:
	tool_change_city_t() : tool_t(TOOL_CHANGE_CITY | SIMPLE_TOOL) {}
	bool init(player_t*) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return false; }
};

// internal tool: rename stuff
class tool_rename_t : public tool_t {
public:
	tool_rename_t() : tool_t(TOOL_RENAME | SIMPLE_TOOL) {}
	bool init(player_t*) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return false; }
};

// internal tool: change player colours
class tool_recolour_t : public tool_t {
public:
	tool_recolour_t() : tool_t(TOOL_RECOLOUR_TOOL | SIMPLE_TOOL) {}
	virtual bool init(player_t *);
	virtual bool is_init_network_save() const { return false; }
};

// internal tool: allow/disallow access
class tool_access_t : public tool_t {
public:
	tool_access_t() : tool_t(TOOL_ACCESS_TOOL | SIMPLE_TOOL) {}
	virtual bool init(player_t *);
	virtual bool is_init_network_save() const { return false; }
};

// internal tool: send message (could be used for chats)
class tool_add_message_t : public tool_t {
public:
	tool_add_message_t() : tool_t(TOOL_ADD_MESSAGE | SIMPLE_TOOL) {}
	bool init(player_t*) OVERRIDE;
	bool is_init_network_save() const OVERRIDE { return false; }
};


#endif
