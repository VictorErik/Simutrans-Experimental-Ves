/*
 * field, which can extend factories
 *
 * Hj. Malthaner
 */

#include <string.h>

#include "../simworld.h"
#include "../simdings.h"
#include "../simfab.h"
#include "../simimg.h"

#include "../boden/grund.h"

#include "../dataobj/loadsave.h"

#include "../player/simplay.h"

#include "field.h"


#ifdef INLINE_DING_TYPE
field_t::field_t(karte_t *welt, koord3d p, spieler_t *sp, const field_class_besch_t *besch, fabrik_t *fab) : ding_t(welt, ding_t::field)
#else
field_t::field_t(karte_t *welt, koord3d p, spieler_t *sp, const field_class_besch_t *besch, fabrik_t *fab) : ding_t(welt)
#endif
{
	this->besch = besch;
	this->fab = fab;
	set_besitzer( sp );
	p.z = welt->max_hgt(p.get_2d());
	set_pos( p );
}



field_t::~field_t()
{
	fab->remove_field_at( get_pos().get_2d() );
}



const char *field_t::ist_entfernbar(const spieler_t *)
{
	// Allow removal provided that the number of fields do not fall below half the minimum
	return (fab->get_field_count() > fab->get_besch()->get_field_group()->get_min_fields() / 2) ? NULL : "Not enough fields would remain.";
}



// remove costs
void field_t::entferne(spieler_t *sp)
{
	spieler_t::book_construction_costs(sp, welt->get_settings().cst_multiply_remove_field, get_pos().get_2d(), ignore_wt);
	mark_image_dirty( get_bild(), 0 );
}



// return the  right month graphic for factories
image_id field_t::get_bild() const
{
	const skin_besch_t *s=besch->get_bilder();
	uint16 anzahl=s->get_bild_anzahl() - besch->has_snow_image();
	if(besch->has_snow_image()  &&  get_pos().z>=welt->get_snowline()) {
		// last images will be shown above snowline
		return s->get_bild_nr(anzahl);
	}
	else {
		// welt->get_yearsteps(): resolution 1/8th month (0..95)
		int anzahl_yearsteps = anzahl * (welt->get_yearsteps() + 1) - 1;
		const image_id bild = s->get_bild_nr( anzahl_yearsteps / 96 );
		if(anzahl_yearsteps % 96 < anzahl) {
			mark_image_dirty( bild, 0 );
		}
		return bild;
	}
}



/**
 * @return Einen Beschreibungsstring f�r das Objekt, der z.B. in einem
 * Beobachtungsfenster angezeigt wird.
 * @author Hj. Malthaner
 */
void field_t::zeige_info()
{
	// show the info of the corresponding factory
	fab->zeige_info();
}
