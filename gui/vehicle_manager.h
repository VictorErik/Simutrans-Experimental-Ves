/*
 * Dialog with list of schedules.
 * Contains buttons: edit new remove
 * Resizable.
 *
 * @author Niels Roest
 * @author hsiegeln: major redesign
 */

#ifndef gui_vehicle_manager_h
#define gui_vehicle_manager_h

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
#include "halt_list_stats.h"
#include "../simline.h"
#include "../simconvoi.h"

class player_t;
class vehicle_manager_t : public gui_frame_t, public action_listener_t
{
private:
	player_t *player;

	button_t but;
	gui_container_t cont;
	//gui_scrollpane_t scrolly_convois, scrolly_haltestellen;
	gui_scrolled_list_t scl;
	gui_speedbar_t filled_bar;
	gui_textinput_t inp_name, inp_filter;
	gui_label_t lbl_filter;
	gui_chart_t chart;
	button_t filterButtons[MAX_LINE_COST];
	gui_tab_panel_t tabs;

	// vector of convoy info objects that are being displayed
	vector_tpl<gui_convoiinfo_t *> convoy_infos;

	// All convoys we own
	vector_tpl<convoihandle_t> convois;
	convoihandle_t cnv;

	// vector of stop info objects that are being displayed
	vector_tpl<halt_list_stats_t *> stop_infos;

	gui_combobox_t livery_selector;

	sint32 selection, capacity, load, loadfactor;

	uint32 old_line_count;
	schedule_t *last_schedule;
	uint32 last_vehicle_count;

	// only show schedules containing ...
	char schedule_filter[512], old_schedule_filter[512];

	// so even japanese can have long enough names ...
	char line_name[512], old_line_name[512];

	// resets textinput to current line name
	// necessary after line was renamed
	void reset_line_name();

	void display(scr_coord pos);

	void update_lineinfo(linehandle_t new_line);


	void build_vehicle_list(int filter);

	static uint16 livery_scheme_index;
	vector_tpl<uint16> livery_scheme_indices;

public:
	vehicle_manager_t(player_t* player);
	~vehicle_manager_t();
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

	/**
	 * Select line and show its info
	 * @author isidoro
	 */
	void show_lineinfo(linehandle_t line);

	/**
	 * called after renaming of line
	 */
	void update_data(linehandle_t changed_line);

	//// following: rdwr stuff
	//void rdwr( loadsave_t *file );
	uint32 get_rdwr_id();
};

#endif
