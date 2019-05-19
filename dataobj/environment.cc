#include <string>
#include "environment.h"
#include "loadsave.h"
#include "../simversion.h"
#include "../simconst.h"
#include "../simtypes.h"
#include "../simcolor.h"
#include "../simmesg.h"
#include "../display/simgraph.h"

#include "../utils/simrandom.h"

sint8 env_t::pak_tile_height_step = 16;
sint8 env_t::pak_height_conversion_factor = 1;
bool env_t::new_height_map_conversion = false;

bool env_t::simple_drawing = false;
bool env_t::simple_drawing_fast_forward = true;
sint16 env_t::simple_drawing_normal = 4;
sint16 env_t::simple_drawing_default = 24;

char env_t::program_dir[1024];
plainstring env_t::default_theme;
const char *env_t::user_dir = 0;
const char *env_t::savegame_version_str = SAVEGAME_VER_NR;
const char *env_t::savegame_ex_version_str = EXTENDED_VER_NR;
const char *env_t::savegame_ex_revision_str = EXTENDED_REVISION_NR;
bool env_t::straight_way_without_control = false;
bool env_t::networkmode = false;
bool env_t::restore_UI = false;
extern uint16 network_server_port;
uint16 const &env_t::server = network_server_port;

// Disable announce by default
uint32 env_t::server_announce = 0;
// Minimum is every 60 seconds, default is every 15 minutes (900 seconds), maximum is 86400 (1 day)
sint32 env_t::server_announce_interval = 900;
std::string env_t::server_dns;
std::string env_t::server_name;
std::string env_t::server_comments;
std::string env_t::server_email;
std::string env_t::server_pakurl;
std::string env_t::server_infurl;
std::string env_t::server_admin_pw;
std::string env_t::server_motd_filename;
vector_tpl<std::string> env_t::listen;
bool env_t::server_save_game_on_quit = false;
bool env_t::reload_and_save_on_quit = true;

sint32 env_t::server_frames_ahead = 4;
sint32 env_t::additional_client_frames_behind = 4;
sint32 env_t::network_frames_per_step = 4;
uint32 env_t::server_sync_steps_between_checks = 24;
bool env_t::pause_server_no_clients = false;

std::string env_t::nickname = "";

// this is explicitely and interactively set by user => we do not touch it in init
const char *env_t::language_iso = "en";
sint16 env_t::scroll_multi = 2;
sint16 env_t::global_volume = 127;
sint16 env_t::midi_volume = 127;
bool env_t::mute_sound = false;
bool env_t::mute_midi = true;
bool env_t::shuffle_midi = true;
sint16 env_t::window_snap_distance = 8;
scr_size env_t::iconsize( 32, 32 );
uint8 env_t::chat_window_transparency = 75;
bool env_t::show_delete_buttons = false;

// only used internally => do not touch further
bool env_t::quit_simutrans = false;

// default settings for new games
settings_t env_t::default_settings;

// what finances are shown? (default bank balance)
bool env_t::player_finance_display_account = true;


// the following initialisation is not important; set values in init()!
std::string env_t::objfilename;
bool env_t::night_shift;
bool env_t::hide_with_transparency;
bool env_t::hide_trees;
uint8 env_t::hide_buildings;
bool env_t::hide_under_cursor;
uint16 env_t::cursor_hide_range;
bool env_t::use_transparency_station_coverage;
uint8 env_t::station_coverage_show;
sint32 env_t::show_names;
sint32 env_t::message_flags[4];
uint32 env_t::water_animation;
uint32 env_t::ground_object_probability;
uint32 env_t::moving_object_probability;
bool env_t::road_user_info;
bool env_t::tree_info;
bool env_t::ground_info;
bool env_t::townhall_info;
bool env_t::single_info;
bool env_t::window_buttons_right;
bool env_t::window_frame_active;
uint8 env_t::verbose_debug;
uint8 env_t::default_sortmode;
uint32 env_t::default_mapmode;
uint8 env_t::show_month;
sint32 env_t::intercity_road_length;
plainstring env_t::river_type[10];
uint8 env_t::river_types;
sint32 env_t::autosave;
uint32 env_t::fps;
sint16 env_t::max_acceleration;
bool env_t::show_tooltips;
uint8 env_t::tooltip_color;
uint8 env_t::tooltip_textcolor;
uint8 env_t::toolbar_max_width;
uint8 env_t::toolbar_max_height;
uint8 env_t::cursor_overlay_color;
uint8 env_t::background_color;
uint8 env_t::show_vehicle_states;
bool env_t::visualize_schedule;
sint8 env_t::daynight_level;
bool env_t::hilly = false;
bool env_t::cities_ignore_height = false;

bool env_t::second_open_closes_win;
bool env_t::remember_window_positions;
uint8 env_t::num_threads;
bool env_t::draw_earth_border;
bool env_t::draw_outside_tile;

// constraints:
// number_of_big_cities <= city_count
// number_of_big_cities == 0 if city_count == 0
// number_of_big_cities >= 1 if city_count !=0
uint32 env_t::number_of_big_cities = 1;
//constraints:
// 0<= number_of_clusters <= anzahl_staedts/4
uint32 env_t::number_of_clusters = 0;
uint32 env_t::cluster_size = 200;
uint8 env_t::cities_like_water = 60;
bool env_t::left_to_right_graphs = true;
uint32 env_t::tooltip_delay;
uint32 env_t::tooltip_duration;

uint8 env_t::front_window_bar_color;
uint8 env_t::front_window_text_color;
uint8 env_t::bottom_window_bar_color;
uint8 env_t::bottom_window_text_color;

uint16 env_t::compass_map_position;
uint16 env_t::compass_screen_position;

uint32 env_t::default_ai_construction_speed;

bool env_t::hide_keyboard = false;

// Hajo: Define default settings.
void env_t::init()
{
	// settings for messages
	message_flags[0] = 0x017F;
	message_flags[1] = 0x0108;
	message_flags[2] = 0x00A0;
	message_flags[3] = 0;

	night_shift = false;

	hide_with_transparency = true;
	hide_trees = false;
	hide_buildings = env_t::NOT_HIDE;
	hide_under_cursor = false;
	cursor_hide_range = 5;

	visualize_schedule = true;

	/* station stuff */
	use_transparency_station_coverage = true;
	station_coverage_show = 0;

	show_names = 3;
	player_finance_display_account = true;

	water_animation = 250; // 250ms per wave stage
	ground_object_probability = 10; // every n-th tile
	moving_object_probability = 1000; // every n-th tile

	road_user_info = false;
	tree_info = true;
	ground_info = false;
	townhall_info = false;
	single_info = true;

	window_buttons_right = false;
	window_frame_active = false;
	second_open_closes_win = false;
	remember_window_positions = true;

	// debug level (0: only fatal, 1: error, 2: warning, 3: all
	verbose_debug = 0;

	default_sortmode = 1;	// sort by amount
	default_mapmode = 0;	// show cities

	savegame_version_str = SAVEGAME_VER_NR;
	savegame_ex_version_str = EXTENDED_VER_NR;
	savegame_ex_revision_str = EXTENDED_REVISION_NR;

	show_month = DATE_FMT_US;

	intercity_road_length = 512;

	river_types = 0;


	/* prissi: autosave every x months (0=off) */
	autosave = 0;

	// default: make 25 frames per second (if possible)
	fps=25;

	// maximum speedup set to 1000 (effectively no limit)
	max_acceleration=50;

#ifdef MULTI_THREAD
	num_threads = 4;
#else
	num_threads = 1;
#endif

	show_tooltips = true;
	tooltip_color = 4;
	tooltip_textcolor = COL_BLACK;

	toolbar_max_width = 0;
	toolbar_max_height = 0;

	cursor_overlay_color = COL_ORANGE;

	background_color = COL_GREY2;
	draw_earth_border = true;
	draw_outside_tile = false;

	show_vehicle_states = 1;

	daynight_level = 0;

	// midi/sound option
	global_volume = 127;
	midi_volume = 127;
	mute_sound = false;
	mute_midi = false;
	shuffle_midi = true;

	left_to_right_graphs = true;

	tooltip_delay = 500;
	tooltip_duration = 5000;

	front_window_bar_color = 1;
	front_window_text_color = COL_WHITE; // 215
	bottom_window_bar_color = 4;
	bottom_window_text_color = 209;	// dark grey

	default_ai_construction_speed = 8000;

	// upper right
	compass_map_position = ALIGN_RIGHT|ALIGN_TOP;
	// lower right
	compass_screen_position = 0, // disbale, other could be ALIGN_RIGHT|ALIGN_BOTTOM;

	// Listen on all addresses by default
	listen.append_unique("::");
	listen.append_unique("0.0.0.0");
}

// save/restore environment
void env_t::rdwr(loadsave_t *file)
{
	// env_t used to be called env_t - keep old name when saving and loading for compatibility
	xml_tag_t u( file, "env_t" );

	file->rdwr_short( scroll_multi );
	file->rdwr_bool( night_shift );
	file->rdwr_byte( daynight_level );
	file->rdwr_long( water_animation );
	if(  file->get_version()<110007  ) {
		bool dummy_b = 0;
		file->rdwr_bool( dummy_b );
	}
	file->rdwr_byte( show_month );

	file->rdwr_bool( use_transparency_station_coverage );
	file->rdwr_byte( station_coverage_show );
	file->rdwr_long( show_names );

	file->rdwr_bool( hide_with_transparency );
	file->rdwr_byte( hide_buildings );
	file->rdwr_bool( hide_trees );

	file->rdwr_long( message_flags[0] );
	file->rdwr_long( message_flags[1] );
	file->rdwr_long( message_flags[2] );
	file->rdwr_long( message_flags[3] );

	if (  file->is_loading()  ) {
		if(  file->get_version()<110000  ) {
			// did not know about chat message, so we enable it
			message_flags[0] |=  (1 << message_t::chat); // ticker
			message_flags[1] &= ~(1 << message_t::chat); // permanent window off
			message_flags[2] &= ~(1 << message_t::chat); // timed window off
			message_flags[3] &= ~(1 << message_t::chat); // do not ignore completely
		}
		if(  file->get_version()<=112002  ) {
			// did not know about scenario message, so we enable it
			message_flags[0] &= ~(1 << message_t::scenario); // ticker off
			message_flags[1] |=  (1 << message_t::scenario); // permanent window on
			message_flags[2] &= ~(1 << message_t::scenario); // timed window off
			message_flags[3] &= ~(1 << message_t::scenario); // do not ignore completely
		}
	}

	file->rdwr_bool( show_tooltips );
	file->rdwr_byte( tooltip_color );
	file->rdwr_byte( tooltip_textcolor );

	file->rdwr_long( autosave );
	file->rdwr_long( fps );
	file->rdwr_short( max_acceleration );

	file->rdwr_bool( road_user_info );
	file->rdwr_bool( tree_info );
	file->rdwr_bool( ground_info );
	file->rdwr_bool( townhall_info );
	file->rdwr_bool( single_info );

	file->rdwr_byte( default_sortmode );
	if(  file->get_version()<111004  ) {
		sint8 mode = log2(env_t::default_mapmode)-1;
		file->rdwr_byte( mode );
		env_t::default_mapmode = mode>=0 ? 1 << mode : 0;
	}
	else {
		file->rdwr_long( env_t::default_mapmode );
	}

	file->rdwr_bool( window_buttons_right );
	file->rdwr_bool( window_frame_active );

	if(  file->get_version()<=112000  ) {
		// set by command-line, it does not make sense to save it.
		uint8 v = verbose_debug;
		file->rdwr_byte( v );
	}

	file->rdwr_long( intercity_road_length );
	if(  file->get_version()<=102002  ) {
		bool no_tree = false;
		file->rdwr_bool( no_tree );
	}
	file->rdwr_long( ground_object_probability );
	file->rdwr_long( moving_object_probability );

	if(  file->is_loading()  ) {
		// these three bytes will be lost ...
		const char *c = NULL;
		file->rdwr_str( c );
		language_iso = c;
	}
	else {
		file->rdwr_str( language_iso );
	}

	file->rdwr_short( global_volume );
	file->rdwr_short( midi_volume );
	file->rdwr_bool( mute_sound );
	file->rdwr_bool( mute_midi );
	file->rdwr_bool( shuffle_midi );

	if(  file->get_version()>102001  ) {
		file->rdwr_byte( show_vehicle_states );
		if(  file->get_extended_version() >= 1 && file->get_extended_version() < 12  && file->get_version() < 112005 ) {
			// Extended (but not standard!) was carrying around a dummy variable.
			// Formerly finance_ltr_graphs.
			bool dummy = false;
			file->rdwr_bool(dummy);
		}
		file->rdwr_bool(left_to_right_graphs);
	}

	if(file->get_extended_version() >= 6)
	{
		file->rdwr_bool(hilly);
		file->rdwr_bool(cities_ignore_height);
	}
	
	if( file->get_extended_version() >= 9 || (file->get_extended_version() == 0 && file->get_version()>=102003)) 
	{
		file->rdwr_long( tooltip_delay );
		file->rdwr_long( tooltip_duration );
		file->rdwr_byte( front_window_bar_color );
		file->rdwr_byte( front_window_text_color );
		file->rdwr_byte( bottom_window_bar_color );
		file->rdwr_byte( bottom_window_text_color );
	}

	if(file->get_extended_version() >= 9)
	{
		file->rdwr_long(number_of_big_cities);
		file->rdwr_long(number_of_clusters);
		file->rdwr_long(cluster_size);
		file->rdwr_byte(cities_like_water);
	}

	if(  file->get_version()>=110000  ) {
		bool dummy = false;
		file->rdwr_bool(dummy); //was add_player_name_to_message
		file->rdwr_short( window_snap_distance );
	}

	if(  file->get_version()>=111001  ) {
		file->rdwr_bool( hide_under_cursor );
		file->rdwr_short( cursor_hide_range );
	}

	if(  file->get_version()>=111002  ) {
		file->rdwr_bool( visualize_schedule );
	}
	if(  file->get_version()>=111003  ) {
		plainstring str = nickname.c_str();
		file->rdwr_str(str);
		if (file->is_loading()) {
			nickname = str ? str.c_str() : "";
		}
	}
	if(  file->get_version()>=112006  ) {
		file->rdwr_byte( background_color );
		file->rdwr_bool( draw_earth_border );
		file->rdwr_bool( draw_outside_tile );
	}
	if(  file->get_version()>=112007  ) {
		file->rdwr_bool( second_open_closes_win );
		file->rdwr_bool( remember_window_positions );
	}
	if(  file->get_version()>=112008  ) {
		file->rdwr_bool( show_delete_buttons );
	}
	if( file->get_version()>=120001 ) {
	 file->rdwr_str( default_theme );
	}

	if(  file->get_version()>=120002 && (file->get_extended_version() == 0  || file->get_extended_revision() >= 10 || file->get_extended_version() >= 13))
	{
		file->rdwr_bool( new_height_map_conversion );
	}
	// server settings are not saved, since the are server specific and could be different on different servers on the save computers
}
