/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

/*
 * Convoi details window
 */

#include "gui_frame.h"
#include "components/gui_container.h"
#include "components/gui_scrollpane.h"
#include "components/gui_textarea.h"
#include "components/gui_textinput.h"
#include "components/gui_speedbar.h"
#include "components/gui_button.h"
#include "components/gui_label.h"                  // 09-Dec-2001      Markus Weber    Added
#include "components/gui_combobox.h"
#include "components/action_listener.h"
#include "../convoihandle_t.h"
#include "../gui/simwin.h"

class scr_coord;


class gui_class_vehicleinfo_t : public gui_container_t
{
private:
	/**
	 * Handle the convoi to be displayed.
	 * @author Hj. Malthaner
	 */
	convoihandle_t cnv;

	vector_tpl<uint16> class_indices;
	vector_tpl<gui_combobox_t *> class_selectors;
	gui_container_t cont;

public:
	/**
	 * @param cnv, the handler for displaying the convoi.
	 * @author Hj. Malthaner
	 */
	gui_class_vehicleinfo_t(convoihandle_t cnv);


	bool action_triggered(gui_action_creator_t*, value_t);

	void set_cnv( convoihandle_t c ) { cnv = c; }

	/**
	 * Draw the component
	 * @author Hj. Malthaner
	 */
	void draw(scr_coord offset);
};



class vehicle_class_manager_t : public gui_frame_t , private action_listener_t
{
public:
	enum sort_mode_t { by_destination=0, by_via=1, by_amount_via=2, by_amount=3, SORT_MODES=4 };

private:

	gui_scrollpane_t scrolly;
	gui_class_vehicleinfo_t veh_info;


	convoihandle_t cnv;
	button_t	sale_button;
	button_t	withdraw_button;
	button_t	retire_button;

	gui_combobox_t pass_class_selector;
	vector_tpl<uint16> pass_class_selector_indices;
	static uint16 pass_class_selector_index;

	gui_combobox_t class_changer;
	vector_tpl<uint16> class_changer_indices;

public:
	vehicle_class_manager_t(convoihandle_t cnv);

	/**
	 * Draw new component. The values to be passed refer to the window
	 * i.e. It's the screen coordinates of the window where the
	 * component is displayed.
	 * @author Hj. Malthaner
	 */
	void draw(scr_coord pos, scr_size size);

	/**
	 * Set the window associated helptext
	 * @return the filename for the helptext, or NULL
	 * @author V. Meyer
	 */
	const char * get_help_filename() const {return "vehicle_class_manager.txt"; }

	/**
	 * Set window size and adjust component sizes and/or positions accordingly
	 * @author Hj. Malthaner
	 */
	virtual void set_windowsize(scr_size size);

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;

	/**
	 * called when convoi was renamed
	 */
	void update_data() { set_dirty(); }

	// this constructor is only used during loading
	vehicle_class_manager_t();

	void rdwr( loadsave_t *file );

	uint32 get_rdwr_id() { return magic_convoi_detail; }
};
