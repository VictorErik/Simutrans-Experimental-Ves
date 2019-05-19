/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

/** @file zeiger.cc object to mark tiles */

#include <stdio.h>

#include "../simworld.h"
#include "../simobj.h"
#include "../simhalt.h"
#include "../boden/grund.h"
#include "../dataobj/environment.h"
#include "zeiger.h"

#ifdef INLINE_OBJ_TYPE
zeiger_t::zeiger_t(loadsave_t *file) : obj_no_info_t(obj_t::zeiger)
#else
zeiger_t::zeiger_t(loadsave_t *file) : obj_no_info_t()
#endif
{
	image = IMG_EMPTY;
	foreground_image = IMG_EMPTY;
	area = koord(0,0);
	offset = koord(0,0);
	rdwr(file);
}


zeiger_t::zeiger_t(koord3d pos, player_t *player) :
#ifdef INLINE_OBJ_TYPE
    obj_no_info_t(obj_t::zeiger, pos)
#else
    obj_no_info_t(pos)
#endif
{
	set_owner( player );
	image = IMG_EMPTY;
	foreground_image = IMG_EMPTY;
	area = koord(0,0);
	offset = koord(0,0);
}


/**
 * We want to be able to highlight the current tile.
 * Unmarks area around old and marks area around new position.
 * Use this routine to change position.
 * @author Hj. Malthaner
 */
void zeiger_t::change_pos(koord3d k )
{
	if(  k != get_pos()  ) {
		// remove from old position
		// and clear mark
		grund_t *gr = welt->lookup( get_pos() );
		if(gr==NULL) {
			gr = welt->lookup_kartenboden( get_pos().get_2d() );
		}
		if(gr) {
			if(  gr->get_halt().is_bound()  ) {
				gr->get_halt()->mark_unmark_coverage( false );
			}
			welt->mark_area( get_pos()- offset, area, false );
		}
		if(  get_pos().x >= welt->get_size().x-1  ||  get_pos().y >= welt->get_size().y-1  ) {
			// the raise and lower tool actually can go to size!
			welt->set_background_dirty();
			// this removes crap form large cursors overlapping into the nirvana
		}
		mark_image_dirty( get_image(), 0 );
		mark_image_dirty( get_front_image(), 0 );
		set_flag( obj_t::dirty );

		obj_t::set_pos( k );
		if(  get_yoff() == Z_PLAN  ) {
			gr = welt->lookup( k );
			if(  gr == NULL  ) {
				gr = welt->lookup_kartenboden( k.get_2d() );
			}
			if(gr) {
				if(  gr->get_halt().is_bound()  &&  env_t::station_coverage_show  ) {
					if (gr->get_halt()->get_pax_enabled() || gr->get_halt()->get_mail_enabled()) {
						gr->get_halt()->mark_unmark_coverage( true );
					}
					else if(gr->get_halt()->get_ware_enabled()) {
						gr->get_halt()->mark_unmark_coverage(true, true);
					}
				}
				welt->mark_area( k-offset, area, true );
			}
		}
	}
}


void zeiger_t::set_image( image_id b )
{
	// mark dirty
	mark_image_dirty( image, 0 );
	mark_image_dirty( b, 0 );
	image = b;
}

void zeiger_t::set_after_image( image_id b )
{
	// mark dirty
	mark_image_dirty( foreground_image, 0 );
	mark_image_dirty( b, 0 );
	foreground_image = b;
}

void zeiger_t::set_area(koord new_area, bool center, koord new_offset)
{
	if (center) {
		new_offset = new_area/2;
	}

	if(new_area==area  &&  new_offset==offset) {
		return;
	}
	welt->mark_area( get_pos()-offset, area, false );
	area = new_area;
	offset = new_offset;
	welt->mark_area( get_pos()-offset, area, true );
}
