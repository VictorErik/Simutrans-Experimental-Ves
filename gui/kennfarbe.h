/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

/*
 * Company colors window
 */

#ifndef gui_kennfarbe_h
#define gui_kennfarbe_h

#include "../utils/cbuffer_t.h"
#include "gui_frame.h"
#include "components/action_listener.h"
#include "components/gui_textarea.h"
#include "components/gui_label.h"
#include "components/gui_button.h"
#include "components/gui_image.h"


/**
 * Dialog to set the player's color
 *
 * @author Hj. Malthaner, Max Kielland 2013
 */

class farbengui_t : public gui_frame_t, action_listener_t
{
	private:
		player_t *player;
		cbuffer_t buf;
		gui_textarea_t txt;
		gui_label_t c1, c2;
		gui_image_t image;

		button_t player_color_1[28];
		button_t player_color_2[28];

	public:
		farbengui_t(player_t *player_);

		/**
		 * Set the window associated helptext
		 * @return the filename for the helptext, or NULL
		 * @author Hj. Malthaner
		 */
		const char * get_help_filename() const { return "color.txt"; }

		bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
};

#endif
