#include "base_info.h"

static scr_coord default_margin(LINESPACE, LINESPACE);


base_infowin_t::base_infowin_t(const char *name, const player_t *player) :
	gui_frame_t(name, player),
	textarea(&buf, 16*LINESPACE),
	embedded(NULL)
{
	buf.clear();
	textarea.set_pos(scr_coord(D_MARGIN_LEFT,D_MARGIN_TOP));
	add_component(&textarea);
	recalc_size();
}


void base_infowin_t::set_embedded(gui_component_t *other)
{
	if (embedded) {
		remove_component(embedded);
	}
	embedded = other;
	add_component(embedded);

	if (embedded) {
		scr_size size = embedded->get_size();
		textarea.set_reserved_area( size + default_margin );
		textarea.recalc_size();

		embedded->set_pos(textarea.get_pos() + scr_coord(textarea.get_size().w - size.w, 0) );
		add_component(embedded);
	}
	recalc_size();
}


void base_infowin_t::recalc_size()
{
	textarea.recalc_size();
	scr_size size = textarea.get_size();
	if (embedded) {
		size.clip_lefttop( embedded->get_size() );
	}
	size += scr_size(D_MARGIN_LEFT + D_MARGIN_RIGHT, D_TITLEBAR_HEIGHT + D_MARGIN_TOP + D_MARGIN_BOTTOM);
	set_windowsize( size );
}
