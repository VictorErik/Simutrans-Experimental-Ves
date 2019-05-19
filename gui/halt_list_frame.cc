/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 * Written (w) 2001 by Markus Weber
 * filtering added by Volker Meyer
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

/*
 * Displays a scrollable list of all stations of a player
 *
 * @author Markus Weber
 * @date 02-Jan-02
 */

#include <algorithm>
#include <string.h>

#include "halt_list_frame.h"
#include "halt_list_filter_frame.h"

#include "../simhalt.h"
#include "../simware.h"
#include "../simfab.h"
#include "../gui/simwin.h"
#include "../descriptor/skin_desc.h"

#include "../bauer/goods_manager.h"

#include "../dataobj/translator.h"

#include "../utils/cbuffer_t.h"

#define HALT_SCROLL_START (D_MARGIN_TOP + LINESPACE + D_V_SPACE + D_BUTTON_HEIGHT)

/**
 * All filter and sort settings are static, so the old settings are
 * used when the window is reopened.
 */

/**
 * This variable defines by which column the table is sorted
 * @author Markus Weber
 */
halt_list_frame_t::sort_mode_t halt_list_frame_t::sortby = nach_name;

/**
 * This variable defines the sort order (ascending or descending)
 * Values: 1 = ascending, 2 = descending)
 * @author Markus Weber
 */
bool halt_list_frame_t::sortreverse = false;

/**
 * Default filter: no Oil rigs!
 */
int halt_list_frame_t::filter_flags = 0;

char halt_list_frame_t::name_filter_value[64] = "";

slist_tpl<const goods_desc_t *> halt_list_frame_t::waren_filter_ab;
slist_tpl<const goods_desc_t *> halt_list_frame_t::waren_filter_an;

const char *halt_list_frame_t::sort_text[SORT_MODES] = {
	"hl_btn_sort_name",
	"hl_btn_sort_waiting",
	"hl_btn_sort_type"
};


/**
* This function compares two stations
*
* @return halt1 < halt2
*/
bool halt_list_frame_t::compare_halts(halthandle_t const halt1, halthandle_t const halt2)
{
	int order;

	/***********************************
	* Compare station 1 and station 2
	***********************************/
	switch (sortby) {
		default:
		case nach_name: // sort by station name
			order = 0;
			break;
		case nach_wartend: // sort by waiting goods
			order = (int)(halt1->get_finance_history( 0, HALT_WAITING ) - halt2->get_finance_history( 0, HALT_WAITING ));
			break;
		case nach_typ: // sort by station type
			order = halt1->get_station_type() - halt2->get_station_type();
			break;
	}
	/**
	 * use name as an additional sort, to make sort more stable.
	 */
	if(order == 0) {
		order = strcmp(halt1->get_name(), halt2->get_name());
	}
	/***********************************
	 * Consider sorting order
	 ***********************************/
	return sortreverse ? order > 0 : order < 0;
}


static bool passes_filter_name(haltestelle_t const& s)
{
	if (!halt_list_frame_t::get_filter(halt_list_frame_t::name_filter)) return true;

	return strstr(s.get_name(), halt_list_frame_t::access_name_filter());
}


static bool passes_filter_type(haltestelle_t const& s)
{
	if (!halt_list_frame_t::get_filter(halt_list_frame_t::typ_filter)) return true;

	haltestelle_t::stationtyp const t = s.get_station_type();
	if (halt_list_frame_t::get_filter(halt_list_frame_t::frachthof_filter)       && t & haltestelle_t::loadingbay)      return true;
	if (halt_list_frame_t::get_filter(halt_list_frame_t::bahnhof_filter)         && t & haltestelle_t::railstation)     return true;
	if (halt_list_frame_t::get_filter(halt_list_frame_t::bushalt_filter)         && t & haltestelle_t::busstop)         return true;
	if (halt_list_frame_t::get_filter(halt_list_frame_t::airport_filter)         && t & haltestelle_t::airstop)         return true;
	if (halt_list_frame_t::get_filter(halt_list_frame_t::dock_filter)            && t & haltestelle_t::dock)            return true;
	if (halt_list_frame_t::get_filter(halt_list_frame_t::monorailstop_filter)    && t & haltestelle_t::monorailstop)    return true;
	if (halt_list_frame_t::get_filter(halt_list_frame_t::maglevstop_filter)      && t & haltestelle_t::maglevstop)      return true;
	if (halt_list_frame_t::get_filter(halt_list_frame_t::narrowgaugestop_filter) && t & haltestelle_t::narrowgaugestop) return true;
	if (halt_list_frame_t::get_filter(halt_list_frame_t::tramstop_filter)        && t & haltestelle_t::tramstop)        return true;
	return false;
}

typedef quickstone_hashtable_tpl<haltestelle_t, haltestelle_t::connexion*> connexions_map_single_remote;

static bool passes_filter_special(haltestelle_t & s)
{
	if (!halt_list_frame_t::get_filter(halt_list_frame_t::spezial_filter)) return true;

	if (halt_list_frame_t::get_filter(halt_list_frame_t::ueberfuellt_filter)) {
		COLOR_VAL const farbe = s.get_status_farbe();
		if (farbe == COL_RED || farbe == COL_ORANGE) {
			return true; // overcrowded
		}
	}

	if(halt_list_frame_t::get_filter(halt_list_frame_t::ohneverb_filter))
	{
		bool walking_connexion_only = true; // Walking connexion or no connexion at all.
		for(uint8 i = 0; i < goods_manager_t::get_max_catg_index(); ++i)
		{
			// TODO: Add UI to show different connexions for multiple classes
			uint8 g_class = 0;
			if(i == goods_manager_t::INDEX_PAS)
			{
				g_class = goods_manager_t::passengers->get_number_of_classes() - 1;
			}
			else if(i == goods_manager_t::INDEX_MAIL)
			{
				g_class = goods_manager_t::mail->get_number_of_classes() - 1;
			}

			if(!s.get_connexions(i, g_class, g_class + 1)->empty())
			{
				// There might be a walking connexion here - do not count a walking connexion.
				FOR(connexions_map_single_remote, &c, *s.get_connexions(i, g_class, g_class + 1) )
				{
					if(c.value->best_line.is_bound() || c.value->best_convoy.is_bound())
					{
						walking_connexion_only = false;
					}
				}
			}
		}
		return walking_connexion_only; //only display stations with NO connexion (other than a walking connexion)
	}

	return false;
}


static bool passes_filter_out(haltestelle_t const& s)
{
	if (!halt_list_frame_t::get_filter(halt_list_frame_t::ware_ab_filter)) return true;

	/*
	 * Die Unterkriterien werden gebildet aus:
	 * - die Ware wird produziert (pax/post_enabled bzw. factory vorhanden)
	 * - es existiert eine Zugverbindung mit dieser Ware (!ziele[...].empty())
	 */

	// Hajo: todo: check if there is a destination for the good (?)

	for (uint32 i = 0; i != goods_manager_t::get_count(); ++i) {
		goods_desc_t const* const ware = goods_manager_t::get_info(i);
		if (!halt_list_frame_t::get_ware_filter_ab(ware)) continue;

		if (ware == goods_manager_t::passengers) {
			if (s.get_pax_enabled()) return true;
		} else if (ware == goods_manager_t::mail) {
			if (s.get_mail_enabled()) return true;
		} else if (ware != goods_manager_t::none) {
			// Oh Mann - eine doublese Schleife und das noch pro Haltestelle
			// Zum Gl�ck ist die Anzahl der Fabriken und die ihrer Ausg�nge
			// begrenzt (Normal 1-2 Fabriken mit je 0-1 Ausgang) -  V. Meyer
			FOR(slist_tpl<fabrik_t*>, const f, s.get_fab_list()) {
				FOR(array_tpl<ware_production_t>, const& j, f->get_output()) {
					if (j.get_typ() == ware) return true;
				}
			}
		}
	}

	return false;
}


static bool passes_filter_in(haltestelle_t const& s)
{
	if (!halt_list_frame_t::get_filter(halt_list_frame_t::ware_an_filter)) return true;
	/*
	 * Die Unterkriterien werden gebildet aus:
	 * - die Ware wird verbraucht (pax/post_enabled bzw. factory vorhanden)
	 * - es existiert eine Zugverbindung mit dieser Ware (!ziele[...].empty())
	 */

	// Hajo: todo: check if there is a destination for the good (?)

	for (uint32 i = 0; i != goods_manager_t::get_count(); ++i) {
		goods_desc_t const* const ware = goods_manager_t::get_info(i);
		if (!halt_list_frame_t::get_ware_filter_an(ware)) continue;

		if (ware == goods_manager_t::passengers) {
			if (s.get_pax_enabled()) return true;
		} else if (ware == goods_manager_t::mail) {
			if (s.get_mail_enabled()) return true;
		} else if (ware != goods_manager_t::none) {
			// Oh Mann - eine doublese Schleife und das noch pro Haltestelle
			// Zum Gl�ck ist die Anzahl der Fabriken und die ihrer Ausg�nge
			// begrenzt (Normal 1-2 Fabriken mit je 0-1 Ausgang) -  V. Meyer
			FOR(slist_tpl<fabrik_t*>, const f, s.get_fab_list()) {
				FOR(array_tpl<ware_production_t>, const& j, f->get_input()) {
					if (j.get_typ() == ware) return true;
				}
			}
		}
	}

	return false;
}


/**
 * Check all filters for one halt.
 * returns true, if it is not filtered away.
 * @author V. Meyer
 */
static bool passes_filter(haltestelle_t & s)
{
	if (halt_list_frame_t::get_filter(halt_list_frame_t::any_filter)) {
		if (!passes_filter_name(s))    return false;
		if (!passes_filter_type(s))    return false;
		if (!passes_filter_special(s)) return false;
		if (!passes_filter_out(s))     return false;
		if (!passes_filter_in(s))      return false;
	}
	return true;
}


halt_list_frame_t::halt_list_frame_t(player_t *player) :
	gui_frame_t( translator::translate("hl_title"), player),
	vscroll( scrollbar_t::vertical ),
	sort_label(translator::translate("hl_txt_sort")),
	filter_label(translator::translate("hl_txt_filter"))
{
	m_player = player;
	filter_frame = NULL;

	sort_label.set_pos(scr_coord(BUTTON1_X, 2));
	add_component(&sort_label);
	sortedby.init(button_t::roundbox, "", scr_coord(BUTTON1_X, 14), scr_size(D_BUTTON_WIDTH,D_BUTTON_HEIGHT));
	sortedby.add_listener(this);
	add_component(&sortedby);

	sorteddir.init(button_t::roundbox, "", scr_coord(BUTTON2_X, 14), scr_size(D_BUTTON_WIDTH,D_BUTTON_HEIGHT));
	sorteddir.add_listener(this);
	add_component(&sorteddir);

	filter_label.set_pos(scr_coord(BUTTON3_X, 2));
	add_component(&filter_label);

	filter_on.init(button_t::roundbox, translator::translate(get_filter(any_filter) ? "hl_btn_filter_enable" : "hl_btn_filter_disable"), scr_coord(BUTTON3_X, 14), scr_size(D_BUTTON_WIDTH,D_BUTTON_HEIGHT));
	filter_on.add_listener(this);
	add_component(&filter_on);

	filter_details.init(button_t::roundbox, translator::translate("hl_btn_filter_settings"), scr_coord(BUTTON4_X, 14), scr_size(D_BUTTON_WIDTH,D_BUTTON_HEIGHT));
	filter_details.add_listener(this);
	add_component(&filter_details);

	display_list();

	set_windowsize(scr_size(D_DEFAULT_WIDTH, D_TITLEBAR_HEIGHT+7*(28)+31+1));
	set_min_windowsize(scr_size(D_DEFAULT_WIDTH, D_TITLEBAR_HEIGHT + 3 * (28) + HALT_SCROLL_START));

	set_resizemode(diagonal_resize);
	resize (scr_coord(0,0));
}


halt_list_frame_t::~halt_list_frame_t()
{
	if(filter_frame) {
		destroy_win(filter_frame);
	}
}


/**
* This function refreshes the station-list
* @author Markus Weber/Volker Meyer
*/
void halt_list_frame_t::display_list()
{
	last_world_stops = haltestelle_t::get_alle_haltestellen().get_count();				// count of stations

	ALLOCA(halthandle_t, a, last_world_stops);
	int n = 0; // temporary variable
	int i;

	/**************************
	 * Sort the station list
	 *************************/

	// create a unsorted station list
	num_filtered_stops = 0;
	FOR(vector_tpl<halthandle_t>, const halt, haltestelle_t::get_alle_haltestellen()) {
		if(  halt->get_owner() == m_player  ) {
			a[n++] = halt;
			if (passes_filter(*halt)) {
				num_filtered_stops++;
			}
		}
	}
	std::sort(a, a + n, compare_halts);

	sortedby.set_text(sort_text[get_sortierung()]);
	sorteddir.set_text(get_reverse() ? "hl_btn_sort_desc" : "hl_btn_sort_asc");

	/****************************
	 * Display the station list
	 ***************************/

	stops.clear();   // clear the list
	stops.resize(n);

	// display stations
	for (i = 0; i < n; i++) {
		stops.append(halt_list_stats_t(a[i]));
		stops.back().set_pos(scr_coord(0,0));
	}
	// hide/show scroll bar
	resize(scr_coord(0,0));
}


bool halt_list_frame_t::infowin_event(const event_t *ev)
{
	const sint16 xr = vscroll.is_visible() ? D_SCROLLBAR_WIDTH : 1;

	if(ev->ev_class == INFOWIN  &&  ev->ev_code == WIN_CLOSE) {
		if(filter_frame) {
			filter_frame->infowin_event(ev);
		}
	}
	else if(IS_WHEELUP(ev)  ||  IS_WHEELDOWN(ev)) {
		// otherwise these events are only registered where directly over the scroll region
		// (and sometime even not then ... )
		return vscroll.infowin_event(ev);
	}
	else if ((IS_LEFTRELEASE(ev) || IS_RIGHTRELEASE(ev)) && ev->my>HALT_SCROLL_START + D_TITLEBAR_HEIGHT  &&  ev->mx<get_windowsize().w - xr && !stops.empty()) {
		const int y = (ev->my - HALT_SCROLL_START - D_TITLEBAR_HEIGHT) / stops[0].get_size().h + vscroll.get_knob_offset();

		if(  y<num_filtered_stops  ) {
			// find the 'y'th filtered stop in the unfiltered stops list
			uint32 i=0;
			for(  int j=0;  i<stops.get_count()  &&  j<=y;  i++  ){
				if (passes_filter(*stops[i].get_halt())) {
					j++;
				}
			}
			// let gui_convoiinfo_t() handle this, since then it will be automatically consistent
			return stops[i-1].infowin_event( ev );
		}
	}
	return gui_frame_t::infowin_event(ev);
}


/**
 * This method is called if an action is triggered
 * @author Markus Weber/Volker Meyer
 */
bool halt_list_frame_t::action_triggered( gui_action_creator_t *comp,value_t /* */)
{
	if (comp == &filter_on) {
		set_filter(any_filter, !get_filter(any_filter));
		filter_on.set_text(get_filter(any_filter) ? "hl_btn_filter_enable" : "hl_btn_filter_disable");
		display_list();
	}
	else if (comp == &sortedby) {
		set_sortierung((sort_mode_t)((get_sortierung() + 1) % SORT_MODES));
		display_list();
	}
	else if (comp == &sorteddir) {
		set_reverse(!get_reverse());
		display_list();
	}
	else if (comp == &filter_details) {
		if (filter_frame) {
			destroy_win(filter_frame);
		}
		else {
			filter_frame = new halt_list_filter_frame_t(m_player, this);
			create_win(filter_frame, w_info, (ptrdiff_t)this);
		}
	}
	return true;
}


/**
 * Resize the window
 * @author Markus Weber
 */
void halt_list_frame_t::resize(const scr_coord size_change)
{
	gui_frame_t::resize(size_change);
	scr_size size = get_windowsize() - scr_size(0, D_TITLEBAR_HEIGHT + HALT_SCROLL_START);
	vscroll.set_visible(false);
	remove_component(&vscroll);
	int halt_h = (stops.empty() ? 28 : stops[0].get_size().h);
	vscroll.set_knob(size.h / halt_h, num_filtered_stops);
	if (num_filtered_stops <= size.h / halt_h) {
		vscroll.set_knob_offset(0);
	}
	else {
		add_component(&vscroll);
		vscroll.set_visible(true);
		vscroll.set_pos(scr_coord(size.w - D_SCROLLBAR_WIDTH, HALT_SCROLL_START));
		vscroll.set_size(size-D_SCROLLBAR_SIZE);
		vscroll.set_scroll_amount( 1 );
	}
}


void halt_list_frame_t::draw(scr_coord pos, scr_size size)
{
	filter_details.pressed = filter_frame != NULL;

	gui_frame_t::draw(pos, size);

	const sint16 xr = vscroll.is_visible() ? D_SCROLLBAR_WIDTH+4 : 6;
	PUSH_CLIP(pos.x, pos.y+47, size.w-xr, size.h-48 );

	const sint32 start = vscroll.get_knob_offset();
	sint16 yoffset = 47;
	const int last_num_filtered_stops = num_filtered_stops;
	num_filtered_stops = 0;

	if(  last_world_stops != haltestelle_t::get_alle_haltestellen().get_count()  ) {
		// some deleted/ added => resort
		display_list();
	}

	FOR(vector_tpl<halt_list_stats_t>, & i, stops) {
		halthandle_t const halt = i.get_halt();
		if (halt.is_bound() && passes_filter(*halt)) {
			num_filtered_stops++;
			if(  num_filtered_stops>start  &&  yoffset<size.h+47  ) {
				i.draw(pos + scr_coord(0, yoffset));
				yoffset += i.get_size().h;
			}
		}
	}
	if(  num_filtered_stops!=last_num_filtered_stops  ) {
		resize (scr_coord(0,0));
	}

	POP_CLIP();
}


void halt_list_frame_t::set_ware_filter_ab(const goods_desc_t *ware, int mode)
{
	if(ware != goods_manager_t::none) {
		if(get_ware_filter_ab(ware)) {
			if(mode != 1) {
				waren_filter_ab.remove(ware);
			}
		}
		else {
			if(mode != 0) {
				waren_filter_ab.append(ware);
			}
		}
	}
}


void halt_list_frame_t::set_ware_filter_an(const goods_desc_t *ware, int mode)
{
	if(ware != goods_manager_t::none) {
		if(get_ware_filter_an(ware)) {
			if(mode != 1) {
				waren_filter_an.remove(ware);
			}
		}
		else {
			if(mode != 0) {
				waren_filter_an.append(ware);
			}
		}
	}
}


void halt_list_frame_t::set_alle_ware_filter_ab(int mode)
{
	if(mode == 0) {
		waren_filter_ab.clear();
	}
	else {
		for(unsigned int i = 0; i<goods_manager_t::get_count(); i++) {
			set_ware_filter_ab(goods_manager_t::get_info(i), mode);
		}
	}
}


void halt_list_frame_t::set_alle_ware_filter_an(int mode)
{
	if(mode == 0) {
		waren_filter_an.clear();
	}
	else {
		for(unsigned int i = 0; i<goods_manager_t::get_count(); i++) {
			set_ware_filter_an(goods_manager_t::get_info(i), mode);
		}
	}
}
