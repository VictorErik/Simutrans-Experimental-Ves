/*
 * Copyright (c) 1997 - 2001 Hj. Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#ifndef boden_boden_h
#define boden_boden_h

#include "grund.h"

/**
 * boden_t are nature tiles (maybe with ways, powerlines, trees and beware: harbor buildings)
 *
 * @author Hj. Malthaner
 */

class boden_t : public grund_t
{
protected:
	virtual void calc_image_internal(const bool calc_only_snowline_change);

public:
	boden_t(loadsave_t *file, koord pos );
	boden_t(koord3d pos, slope_t::type slope);

	virtual void rdwr(loadsave_t *file);

	inline bool ist_natur() const { return !hat_wege()  &&  !is_halt(); }

	const char *get_name() const;

	grund_t::typ get_typ() const {return boden;}
};

#endif
