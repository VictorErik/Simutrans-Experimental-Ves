/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 *
 * Base class for Way in Simutrans.
 *
 * 14.06.00 derived from simgrund.cc
 * Revised January 2001
 *
 * derived from simobj.h in 2007
 *
 * Hj. Malthaner
 */

#include <stdio.h>

#include "../../tpl/slist_tpl.h"

#include "weg.h"

#include "schiene.h"
#include "strasse.h"
#include "monorail.h"
#include "maglev.h"
#include "narrowgauge.h"
#include "kanal.h"
#include "runway.h"


#include "../grund.h"
#include "../../simmesg.h"
#include "../../simworld.h"
#include "../../display/simimg.h"
#include "../../simhalt.h"
#include "../../simobj.h"
#include "../../player/simplay.h"
#include "../../obj/wayobj.h"
#include "../../obj/roadsign.h"
#include "../../obj/signal.h"
#include "../../obj/crossing.h"
#include "../../obj/bruecke.h"
#include "../../obj/tunnel.h"
#include "../../obj/gebaeude.h" // for ::should_city_adopt_this
#include "../../utils/cbuffer_t.h"
#include "../../dataobj/environment.h" // TILE_HEIGHT_STEP
#include "../../dataobj/translator.h"
#include "../../dataobj/loadsave.h"
#include "../../dataobj/environment.h"
#include "../../descriptor/way_desc.h"
#include "../../descriptor/tunnel_desc.h"
#include "../../descriptor/roadsign_desc.h"
#include "../../descriptor/building_desc.h" // for ::should_city_adopt_this
#include "../../utils/simstring.h"

#include "../../bauer/wegbauer.h"

#ifdef MULTI_THREAD
#include "../../utils/simthread.h"
static pthread_mutex_t weg_calc_image_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif

/**
 * Alle instantiierten Wege
 * @author Hj. Malthaner
 */
vector_tpl <weg_t *> alle_wege;


/**
 * Get list of all ways
 * @author Hj. Malthaner
 */
const vector_tpl <weg_t *> & weg_t::get_alle_wege()
{
	return alle_wege;
}

const uint32 weg_t::get_all_ways_count()
{
	return alle_wege.get_count();  
}

void weg_t::clear_list_of__ways()
{
	alle_wege.clear();
}


// returns a way with matching waytype
weg_t* weg_t::alloc(waytype_t wt)
{
	weg_t *weg = NULL;
	switch(wt) {
		case tram_wt:
		case track_wt:
			weg = new schiene_t();
			break;
		case monorail_wt:
			weg = new monorail_t();
			break;
		case maglev_wt:
			weg = new maglev_t();
			break;
		case narrowgauge_wt:
			weg = new narrowgauge_t();
			break;
		case road_wt:
			weg = new strasse_t();
			break;
		case water_wt:
			weg = new kanal_t();
			break;
		case air_wt:
			weg = new runway_t();
			break;
		default:
			// keep compiler happy; should never reach here anyway
			assert(0);
			break;
	}
	return weg;
}


// returns a string with the "official name of the waytype"
const char *weg_t::waytype_to_string(waytype_t wt)
{
	switch(wt) {
		case tram_wt:	return "tram_track";
		case track_wt:	return "track";
		case monorail_wt: return "monorail_track";
		case maglev_wt: return "maglev_track";
		case narrowgauge_wt: return "narrowgauge_track";
		case road_wt:	return "road";
		case water_wt:	return "water";
		case air_wt:	return "air";
		default:
			// keep compiler happy; should never reach here anyway
			break;
	}
	return "invalid waytype";
}


void weg_t::set_desc(const way_desc_t *b, bool from_saved_game)
{
	if(desc)
	{
		// Remove the old maintenance cost
		sint32 old_maint = get_desc()->get_maintenance();
		check_diagonal();
		if(is_diagonal())
		{
			old_maint *= 10;
			old_maint /= 14;
		}
		player_t::add_maintenance(get_owner(), -old_maint, get_desc()->get_finance_waytype());
	}
	
	desc = b;
	if (!from_saved_game)
	{
		// Add the new maintenance cost
		// Do not set this here if loading a saved game,
		// as this is already set in finish_rd
		sint32 maint = get_desc()->get_maintenance();
		if (is_diagonal())
		{
			maint *= 10;
			maint /= 14;
		}
		player_t::add_maintenance(get_owner(), maint, get_desc()->get_finance_waytype());
	}

	grund_t* gr = welt->lookup(get_pos());
	if(!gr)
	{
		gr = welt->lookup_kartenboden(get_pos().get_2d());
	}
	const bruecke_t *bridge = gr ? gr->find<bruecke_t>() : NULL;
	const tunnel_t *tunnel = gr ? gr->find<tunnel_t>() : NULL;
	const slope_t::type hang = gr ? gr->get_weg_hang() : slope_t::flat;

#if MULTI_THREAD
	const bool is_destroying = welt->is_destroying();
	if (env_t::networkmode && !is_destroying)
	{
		// In network mode, we cannot have set_desc running concurrently with
		// convoy path-finding because  whether the convoy path-finder is called
		// on this tile of way before or after this function is indeterminate.
		if (!world()->get_first_step())
		{
			simthread_barrier_wait(&karte_t::step_convoys_barrier_external);
			welt->set_first_step(1); 
		}
	}
#endif

	if(hang != slope_t::flat) 
	{
		const uint slope_height = (hang & 7) ? 1 : 2;
		if(slope_height == 1)
		{
			if(bridge)
			{
				max_speed = min(desc->get_topspeed_gradient_1(), bridge->get_desc()->get_topspeed_gradient_1());
			}
			else if(tunnel)
			{
				max_speed = min(desc->get_topspeed_gradient_1(), tunnel->get_desc()->get_topspeed_gradient_1());
			}
			else
			{
				max_speed = desc->get_topspeed_gradient_1();
			}
		}
		else
		{
			if(bridge)
			{
				max_speed = min(desc->get_topspeed_gradient_2(), bridge->get_desc()->get_topspeed_gradient_2());
			}
			else if(tunnel)
			{
				max_speed = min(desc->get_topspeed_gradient_2(), tunnel->get_desc()->get_topspeed_gradient_2());
			}
			else
			{
				max_speed = desc->get_topspeed_gradient_2();
			}
		}
	}
	else
	{
		if(bridge)
			{
				max_speed = min(desc->get_topspeed(), bridge->get_desc()->get_topspeed());
			}
		else if(tunnel)
			{
				max_speed = min(desc->get_topspeed(), tunnel->get_desc()->get_topspeed());
			}
			else
			{
				max_speed = desc->get_topspeed();
			}
	}

	const sint32 city_road_topspeed = welt->get_city_road()->get_topspeed();

	if(hat_gehweg() && desc->get_wtyp() == road_wt)
	{
		max_speed = min(max_speed, city_road_topspeed);
	}
	
	max_axle_load = desc->get_max_axle_load();
	
	// Clear the old constraints then add all sources of constraints again.
	// (Removing will not work in cases where a way and another object, 
	// such as a bridge, tunnel or wayobject, share a constraint).
	clear_way_constraints();
	add_way_constraints(desc->get_way_constraints()); // Add the way's own constraints
	if(bridge)
	{
		add_way_constraints(bridge->get_desc()->get_way_constraints());
	}
	if(tunnel)
	{
		add_way_constraints(tunnel->get_desc()->get_way_constraints());
	}
	const wayobj_t* wayobj = gr ? gr->get_wayobj(get_waytype()) : NULL;
	if(wayobj)
	{
		add_way_constraints(wayobj->get_desc()->get_way_constraints());
	}

	if(desc->is_mothballed())
	{	
		degraded = true;
		remaining_wear_capacity = 0;
		replacement_way = NULL;
	}
	else if(!from_saved_game)
	{
		remaining_wear_capacity = desc->get_wear_capacity();
		last_renewal_month_year = welt->get_timeline_year_month();
		degraded = false;
		replacement_way = desc;
		const grund_t* gr = welt->lookup(get_pos());
		if(gr)
		{
			roadsign_t* rs = gr->find<roadsign_t>();
			if(!rs)
			{
				rs =  gr->find<signal_t>();
			} 
			if(rs && rs->get_desc()->is_retired(welt->get_timeline_year_month()))
			{
				// Upgrade obsolete signals and signs when upgrading the underlying way if possible.
				rs->upgrade(welt->lookup_kartenboden(get_pos().get_2d())->get_hoehe() != get_pos().z); 
			}
		}
	}
}


/**
 * initializes statistic array
 * @author hsiegeln
 */
void weg_t::init_statistics()
{
	for(  int type=0;  type<MAX_WAY_STATISTICS;  type++  ) {
		for(  int month=0;  month<MAX_WAY_STAT_MONTHS;  month++  ) {
			statistics[month][type] = 0;
		}
	}
	creation_month_year = welt->get_timeline_year_month();
}


/**
 * Initializes all member variables
 * @author Hj. Malthaner
 */
void weg_t::init()
{
	ribi = ribi_maske = ribi_t::none;
	max_speed = 450;
	max_axle_load = 1000;
	bridge_weight_limit = UINT32_MAX_VALUE;
	desc = 0;
	init_statistics();
	alle_wege.append(this);
	flags = 0;
	image = IMG_EMPTY;
	foreground_image = IMG_EMPTY;
	public_right_of_way = false;
	degraded = false;
	remaining_wear_capacity = 100000000;
	replacement_way = NULL;
}


weg_t::~weg_t()
{
	if (!welt->is_destroying())
	{
		alle_wege.remove(this);
		player_t *player = get_owner();
		if (player  &&  desc)
		{
			sint32 maint = desc->get_maintenance();
			if (is_diagonal())
			{
				maint *= 10;
				maint /= 14;
			}
			player_t::add_maintenance(player, -maint, desc->get_finance_waytype());
		}
	}
}


void weg_t::rdwr(loadsave_t *file)
{
	xml_tag_t t( file, "weg_t" );

	// save owner
	if(  file->get_version() >= 99006  ) {
		sint8 spnum=get_player_nr();
		file->rdwr_byte(spnum);
		set_player_nr(spnum);
	}

	// all connected directions
	uint8 dummy8 = ribi;
	file->rdwr_byte(dummy8);
	if(  file->is_loading()  ) {
		ribi = dummy8 & 15;	// before: high bits was maske
		ribi_maske = 0;	// maske will be restored by signal/roadsing
	}

	sint16 dummy16=max_speed;
	file->rdwr_short(dummy16);
	max_speed=dummy16;

	if(  file->get_version() >= 89000  ) {
		dummy8 = flags;
		file->rdwr_byte(dummy8);
		if(  file->is_loading()  ) {
			// all other flags are restored afterwards
			flags = dummy8 & HAS_SIDEWALK;
		}
	}

	for(  int type=0;  type<MAX_WAY_STATISTICS;  type++  ) {
		for(  int month=0;  month<MAX_WAY_STAT_MONTHS;  month++  ) {
			sint32 w = statistics[month][type];
			file->rdwr_long(w);
			statistics[month][type] = (sint16)w;
			// DBG_DEBUG("weg_t::rdwr()", "statistics[%d][%d]=%d", month, type, statistics[month][type]);
		}
	}

	if(file->get_extended_version() >= 1)
	{
		if (max_axle_load > 32768) {
			dbg->error("weg_t::rdwr()", "Max axle load out of range");
		}
		uint16 wdummy16 = max_axle_load;
		file->rdwr_short(wdummy16);
		max_axle_load = wdummy16;
	}

	if(file->get_extended_version() >= 12)
	{
		bool prow = public_right_of_way;
		file->rdwr_bool(prow);
		public_right_of_way = prow;
#ifdef SPECIAL_RESCUE_12_3
		if(file->is_saving())
		{
			uint32 rwc = remaining_wear_capacity;
			file->rdwr_long(rwc);
			remaining_wear_capacity = rwc;
			uint16 cmy = creation_month_year;
			file->rdwr_short(cmy);
			creation_month_year = cmy;
			uint16 lrmy = last_renewal_month_year;
			file->rdwr_short(lrmy);
			last_renewal_month_year = lrmy;
			bool deg = degraded;
			file->rdwr_bool(deg);
			degraded = deg;
		}
#else
		uint32 rwc = remaining_wear_capacity;
		file->rdwr_long(rwc);
		remaining_wear_capacity = rwc;
		uint16 cmy = creation_month_year;
		file->rdwr_short(cmy);
		creation_month_year = cmy;
		uint16 lrmy = last_renewal_month_year;
		file->rdwr_short(lrmy);
		last_renewal_month_year = lrmy;
		bool deg = degraded;
		file->rdwr_bool(deg);
		degraded = deg;
#endif
	}
}

bool weg_t::is_height_restricted() const
{
	const grund_t* gr_above = world()->lookup(get_pos() + koord3d(0, 0, 1));
	if (env_t::pak_height_conversion_factor == 2 && gr_above && gr_above->get_weg_nr(0))
	{
		return true;
	}
	else
	{
		return false;
	}
}


/**
 * Info-text for this way
 * @author Hj. Malthaner
 */
void weg_t::info(cbuffer_t & buf, bool is_bridge) const
{
	obj_t::info(buf);

	// There are some texts that is shown above here on some of the way info windows. Those should be rearranged to look more appropriate and fill less space.
	const double tiles_pr_km = (1000 / welt->get_settings().get_meters_per_tile());
	grund_t *gr = welt->lookup(get_pos());
	const wayobj_t *wayobj = gr ? gr->get_wayobj(get_waytype()) : NULL;
	const bruecke_t *bridge = gr ? gr->find<bruecke_t>() : NULL;
	const tunnel_t *tunnel = gr ? gr->find<tunnel_t>() : NULL;

	const sint32 city_road_topspeed = welt->get_city_road()->get_topspeed();
	const sint32 wayobj_topspeed = wayobj ? wayobj->get_desc()->get_topspeed() : UINT32_MAX_VALUE;
	const sint32 bridge_topspeed = bridge ?  bridge->get_desc()->get_topspeed() : UINT32_MAX_VALUE;
	const sint32 tunnel_topspeed = tunnel ? tunnel->get_desc()->get_topspeed() : UINT32_MAX_VALUE;
	const sint32 topspeed = desc->get_topspeed();

	if (public_right_of_way)
	{
		buf.append(translator::translate("Public right of way"));
		buf.append("\n");
	}
	buf.append(translator::translate(desc->get_name()));
	if (wayobj)
	{
		buf.printf(", %s %s", translator::translate("with_wayobj"), translator::translate(wayobj->get_desc()->get_name()));
	}
	buf.append("\n\n");

	if (degraded)
	{
		buf.append(translator::translate("Degraded"));
		buf.append("\n\n");
		buf.append(translator::translate("way_cannot_be_used_by_any_vehicle"));
		buf.append("\n\n");
	}



	if (!degraded)
	{
		buf.append(translator::translate("Max. speed:"));
		buf.append(" ");
		buf.append(max_speed);
		buf.append(translator::translate("km/h"));
		buf.append("\n");

		if (topspeed > max_speed)
		{
			if (tunnel)
			{
				if (max_speed == tunnel->get_desc()->get_topspeed() || tunnel->get_desc()->get_topspeed_gradient_1() || tunnel->get_desc()->get_topspeed_gradient_2())
				{
					buf.append(translator::translate("(speed_restricted_by_tunnel)"));
				}
			}
			else if (bridge)
			{
				if (max_speed == bridge->get_desc()->get_topspeed() || bridge->get_desc()->get_topspeed_gradient_1() || bridge->get_desc()->get_topspeed_gradient_2())
				{
					buf.append(translator::translate("(speed_restricted_by_bridge)"));
				}
			}
			else if (wayobj)
			{
				if (max_speed == wayobj->get_desc()->get_topspeed() || wayobj->get_desc()->get_topspeed_gradient_1() || wayobj->get_desc()->get_topspeed_gradient_2())

				{
					buf.append(translator::translate("(speed_restricted_by_wayobj)"));
				}
			}

			else
			{
				buf.append(translator::translate("(speed_restricted_by_city)"));
			}
			buf.append("\n");
		}
		
		if (desc->get_styp() == type_elevated || wtyp == air_wt || wtyp == water_wt)
		{
			buf.append(translator::translate("Max. weight:"));
		}
		else
		{
			buf.append(translator::translate("Max. axle load:"));
		}
		buf.append(" ");
		buf.append(max_axle_load);
		buf.append(translator::translate("tonnen"));
		buf.append("\n");
		if (is_bridge && bridge_weight_limit < UINT32_MAX_VALUE)
		{
			buf.append(translator::translate("Max. weight:"));
			buf.append(" ");
			buf.append(bridge_weight_limit);
			buf.append(translator::translate("tonnen"));
			buf.append("\n");
		}
	}

	if (wtyp == air_wt && desc->get_styp() == type_runway)
	{
		bool runway_36_18 = false;
		bool runway_09_27 = false;

		switch (get_ribi())
		{
		case 1: // north
		case 4: // south
		case 5: // north-south
		case 7: // north-south-east
		case 13: // north-south-west
			runway_36_18 = true;
			break;
		case 2: // east
		case 8: // west
		case 10: // east-west
		case 11: // east-west-north
		case 14: // east-west-south
			runway_09_27 = true;
			break;
		case 15: // all
			runway_36_18 = true;
			runway_09_27 = true;
			break;
		default:
			runway_36_18 = false;
			runway_09_27 = false;
			break;
		}


		uint32 runway_tiles = 0;
		koord3d pos = get_pos();
		bool more_runway_left = true;
		bool counting_runway_forward = true;
		const double km_per_tile = welt->get_settings().get_meters_per_tile();

		if (runway_36_18 == true)
		{
			for (uint32 i = 0; more_runway_left == true; i++)
			{
				grund_t *gr = welt->lookup_kartenboden(pos.x, pos.y);
				runway_t * sch1 = gr ? (runway_t *)gr->get_weg(air_wt) : NULL;
				if (counting_runway_forward == true)
				{
					if (sch1 != NULL)
					{
						pos.y += 1;
						if ((sch1->get_ribi_unmasked() == 1))
						{
							counting_runway_forward = false;
							pos = get_pos();
						}
					}
					else
					{
						counting_runway_forward = false;
						pos = get_pos();
					}
				}

				else
				{
					if (sch1 != NULL)
					{
						pos.y -= 1;
						if ((sch1->get_ribi_unmasked() == 4))
						{
							more_runway_left = false;
							runway_tiles = i;
						}
					}
					else
					{
						more_runway_left = false;
						runway_tiles = i;
					}
				}
			}
			const double runway_meters_36_18 = (double)runway_tiles * km_per_tile;

			buf.printf("%s: ", translator::translate("runway_36/18"));
			buf.append(runway_meters_36_18);
			buf.append(translator::translate("meter"));
			buf.append("\n");
		}

		runway_tiles = 0;
		pos = get_pos();
		more_runway_left = true;
		counting_runway_forward = true;

		if (runway_09_27 == true)
		{
			for (uint32 i = 0; more_runway_left == true; i++)
			{
				grund_t *gr = welt->lookup_kartenboden(pos.x, pos.y);
				runway_t * sch1 = gr ? (runway_t *)gr->get_weg(air_wt) : NULL;
				if (counting_runway_forward == true)
				{
					if (sch1 != NULL)
					{
						pos.x += 1;
						if ((sch1->get_ribi_unmasked() == 8))
						{
							counting_runway_forward = false;
							pos = get_pos();
						}
					}
					else
					{
						counting_runway_forward = false;
						pos = get_pos();
					}
				}

				else
				{
					if (sch1 != NULL)
					{
						pos.x -= 1;
						if ((sch1->get_ribi_unmasked() == 2))
						{
							more_runway_left = false;
							runway_tiles = i;
						}
					}
					else
					{
						more_runway_left = false;
						runway_tiles = i;
					}
				}
			}

			const double runway_meters_09_27 = (double)runway_tiles * km_per_tile;

			buf.printf("%s: ", translator::translate("runway_09/27"));
			buf.append(runway_meters_09_27);
			buf.append(translator::translate("meter"));
			buf.append("\n");
		}
	}

	if (desc->get_maintenance() > 0)
	{
		char maintenance_number[64];
		money_to_string(maintenance_number, (double)welt->calc_adjusted_monthly_figure(desc->get_maintenance()) / 100.0);
		buf.printf("%s:\n%s", translator::translate("monthly_maintenance_cost"), maintenance_number);



		char maintenance_km_number[64];
		money_to_string(maintenance_km_number, (double)welt->calc_adjusted_monthly_figure(desc->get_maintenance()) / 100 * tiles_pr_km);
		buf.printf(", (%s/%s)", maintenance_km_number, translator::translate("km"));
		buf.append("\n");
	}
	if (wayobj && wayobj->get_desc()->get_maintenance() > 0)
	{
			char maintenance_wayobj_number[64];
			money_to_string(maintenance_wayobj_number, ((double)welt->calc_adjusted_monthly_figure(desc->get_maintenance()) + (double)welt->calc_adjusted_monthly_figure(wayobj->get_desc()->get_maintenance())) / 100.0);
			buf.printf("%s:\n%s", translator::translate("maint_incl_wayobj"), maintenance_wayobj_number);

			char maintenance_wayobj_km_number[64];
			money_to_string(maintenance_wayobj_km_number, ((double)welt->calc_adjusted_monthly_figure(desc->get_maintenance()) + (double)welt->calc_adjusted_monthly_figure(wayobj->get_desc()->get_maintenance())) / 100.0 * tiles_pr_km);
			buf.printf(", (%s/%s)", maintenance_wayobj_km_number, translator::translate("km"));
			buf.append("\n");
	}
	if ((desc->get_maintenance() <= 0) && (!wayobj || (wayobj && wayobj->get_desc()->get_maintenance() <= 0)))
	{
		buf.append(translator::translate("no_maintenance_costs"));
		buf.append("\n");
	}

	

	buf.append("\n");
	buf.append(translator::translate("Condition"));
	buf.append(": ");
	char tmpbuf_cond[40];
	sprintf(tmpbuf_cond, "%u%%", get_condition_percent());
	buf.append(tmpbuf_cond);
	buf.append("\n");
	buf.append(translator::translate("Built"));
	buf.append(": ");
	char tmpbuf_built[40];
	sprintf(tmpbuf_built, "%s, %i", translator::get_month_name(creation_month_year % 12), creation_month_year / 12);
	buf.append(tmpbuf_built);
	buf.append("\n");
	if (!degraded)
	{
		buf.append(translator::translate("Last renewed"));
	}
	else
	{
		buf.append(translator::translate("Degraded"));
	}
	buf.append(": ");
	char tmpbuf_renewed[40];
	sprintf(tmpbuf_renewed, "%s, %i", translator::get_month_name(last_renewal_month_year % 12), last_renewal_month_year / 12);
	buf.append(tmpbuf_renewed);
	buf.append("\n");
	buf.append(translator::translate("To be renewed with"));
	buf.append(":\n- ");
	if (replacement_way)
	{
		const uint16 time = welt->get_timeline_year_month();
		bool is_current = !time || replacement_way->get_intro_year_month() <= time && time < replacement_way->get_retire_year_month();

		if (replacement_way->get_name() != get_desc()->get_name())
		{
			if (!is_current)
			{
				buf.append(translator::translate(way_builder_t::weg_search(replacement_way->get_waytype(), replacement_way->get_topspeed(), (const sint32)replacement_way->get_axle_load(), time, (systemtype_t)replacement_way->get_styp(), replacement_way->get_wear_capacity())->get_name()));
			}
			else
			{
				buf.append(translator::translate(replacement_way->get_name()));
			}
		}
		else
		{
			if (!degraded)
			{
				buf.append(translator::translate("same_as_current"));
			}
			else
			{
				buf.append(translator::translate("keine"));
			}
		}
		if (replacement_way->is_mothballed() == false)
		{
			buf.append("\n");
			char upgrade_cost_number[64];
			money_to_string(upgrade_cost_number, (double)welt->calc_adjusted_monthly_figure(desc->get_upgrade_group() == replacement_way->get_upgrade_group() ? replacement_way->get_way_only_cost() : replacement_way->get_value()) / 100 / 2);
			buf.printf("- %s: %s", translator::translate("renewal_costs"), upgrade_cost_number);

			char upgrade_cost_pr_km_number[64];
			money_to_string(upgrade_cost_pr_km_number, (double)welt->calc_adjusted_monthly_figure(desc->get_upgrade_group() == replacement_way->get_upgrade_group() ? replacement_way->get_way_only_cost() : replacement_way->get_value()) / 100 / 2 * tiles_pr_km); 
			buf.printf(", (%s/%s)", upgrade_cost_pr_km_number, translator::translate("km"));
			buf.append("\n");
			

			if (replacement_way->get_axle_load() != desc->get_axle_load())
			{
				const char* weight_incr_text = NULL;
				const char* weight_decr_text = NULL;

				if (desc->get_styp() == type_elevated || wtyp == air_wt || wtyp == water_wt)
				{
					weight_incr_text = translator::translate("increased_max_weight");
					weight_decr_text = translator::translate("decreased_max_weight");
				}
				else
				{
					weight_incr_text = translator::translate("increased_axle_load");
					weight_decr_text = translator::translate("decreased_axle_load");
				}

				if (replacement_way->get_axle_load() > desc->get_axle_load())
				{
					buf.printf("- %s: +", weight_incr_text);
					buf.append(replacement_way->get_axle_load() - desc->get_axle_load());
					buf.append(translator::translate("tonnen"));
					buf.append("\n");
				}
				if (replacement_way->get_axle_load() < desc->get_axle_load())
				{
					buf.printf("- %s: -", weight_decr_text);
					buf.append(desc->get_axle_load() - replacement_way->get_axle_load());
					buf.append(translator::translate("tonnen"));
					buf.append("\n");
				}
			}
			if (replacement_way->get_topspeed() > desc->get_topspeed())
			{
				buf.printf("- %s: +", translator::translate("increased_speed"));
				buf.append(replacement_way->get_topspeed() - desc->get_topspeed());
				buf.append(translator::translate("km/h"));
				buf.append("\n");
			}
			if (replacement_way->get_topspeed() < desc->get_topspeed())
			{
				buf.printf("- %s: -", translator::translate("decreased_speed"));
				buf.append(desc->get_topspeed() - replacement_way->get_topspeed());
				buf.append(translator::translate("km/h"));
				buf.append("\n");
			}
			if (replacement_way->get_maintenance() != desc->get_maintenance())
			{
				const char* maintenance_text = NULL;
				const char* maintenance_symbol_text = NULL;
				if (replacement_way->get_maintenance() > desc->get_maintenance())
				{
					maintenance_text = translator::translate("increased_maintenance");
					maintenance_symbol_text = "+";
				}
				else
				{
					maintenance_text = translator::translate("decreased_maintenance");
					maintenance_symbol_text = "";
				}
				char maintenance_replacement_difference_number[64];
				money_to_string(maintenance_replacement_difference_number, ((double)welt->calc_adjusted_monthly_figure(replacement_way->get_maintenance()) - (double)welt->calc_adjusted_monthly_figure(desc->get_maintenance())) / 100.0);
				buf.printf("- %s: %s%s", maintenance_text, maintenance_symbol_text, maintenance_replacement_difference_number);

				char maintenance_replacement_difference_km_number[64];
				money_to_string(maintenance_replacement_difference_km_number, ((double)welt->calc_adjusted_monthly_figure(replacement_way->get_maintenance()) - (double)welt->calc_adjusted_monthly_figure(desc->get_maintenance())) / 100.0 * tiles_pr_km);
				buf.printf(", (%s%s/%s)", maintenance_symbol_text, maintenance_replacement_difference_km_number, translator::translate("km"));
				buf.append("\n");
			}

			const long double wear_capacity_fractional_orig = (long double)desc->get_wear_capacity() / 10000.0;
			const long double wear_capacity_fractional_replac = (long double)replacement_way->get_wear_capacity() / 10000.0;

			if (wear_capacity_fractional_replac != wear_capacity_fractional_orig)
			{
				buf.printf("- %s ", translator::translate("new_way_is"));
				if (wear_capacity_fractional_replac > wear_capacity_fractional_orig)
				{
					
					const double way_wear_stronger = (wear_capacity_fractional_replac - wear_capacity_fractional_orig) / wear_capacity_fractional_orig * 100;
					buf.append(way_wear_stronger);
					buf.printf("%s", translator::translate("%_stronger"));
				}
				else // Same equation, but with a minus sign to avoid a percentage preceded with a "-" sign
				{
					const double way_wear_weaker = (-(wear_capacity_fractional_replac - wear_capacity_fractional_orig))/ wear_capacity_fractional_orig *100;
					buf.append(way_wear_weaker);
					buf.printf("%s", translator::translate("%_weaker"));
				}
				buf.append("\n");
			}
		}
	}
	else
	{
		buf.append(translator::translate("keine"));
		buf.append("\n");
	}
	if (!degraded)
	{
		// Find out approximately when the road needs to upgrade again	
		if (replacement_way)
		{
			buf.append(translator::translate("estimated_renewal"));
		}
		else
		{
			buf.append(translator::translate("expected_degration"));
		}
		buf.append(": ");
		if (welt->get_timeline_year_month() - last_renewal_month_year > 0 && get_condition_percent() < 100)
		{
			uint16 month_since_last_renewal = welt->get_timeline_year_month() - last_renewal_month_year + 1;
			uint32 used_condition_percents = 100 - get_condition_percent();
			const uint32 degridation_fraction = welt->get_settings().get_way_degradation_fraction();
			uint32 condition_left = replacement_way ? get_condition_percent() - (100 / degridation_fraction) : get_condition_percent();

			const long double months_pr_percent = (long double)month_since_last_renewal / used_condition_percents;
			const long double remaining_months = (long double)months_pr_percent *condition_left;
			uint16 remaining_years = remaining_months / 12;

			const long double ticks_pr_month = (long double)welt->ticks_per_world_month;
			const long double remaining_month_ticks = (long double)welt->get_next_month_ticks() - welt->get_ticks();
			const long double progress_towards_next_month_ticks = ticks_pr_month - remaining_month_ticks;
			const double month_progress_percent = (progress_towards_next_month_ticks / ticks_pr_month * 100) / 100;

			// Due to some very inacuracy in the calculations, the "(month_progress_percent*2) - 1" part in the lower line will subtract a month from the beginning,
			// and add a month to the ending of each month in the calculation. When testing, this seemed to give better predictions and help eliminating the yoyo effect at each ends of the months.
			// This should be subject to calibration if the values still jump too much back and fourth. // Ves
			const uint16 next_renewal_month_year = remaining_months + welt->get_current_month() + (month_progress_percent*2) - 1;

			char next_renew_date[40];
			if (remaining_years < 10)
			{
				// Only show date when it is within 10 years.
				sprintf(next_renew_date, "%s, %i", translator::get_month_name(next_renewal_month_year % 12), next_renewal_month_year / 12);
			}
			else if (remaining_years < 50)
			{
				// Just show how many years left.
				sprintf(next_renew_date, translator::translate("in_%i_years"), remaining_years);
			}
			else
			{
				// Dont even bother count the years...
				sprintf(next_renew_date, translator::translate(">_50_years"));
			}
			buf.append(next_renew_date);
			buf.append("\n");
		}
		else
		{
			buf.append(translator::translate("not_enough_data"));
		}
		buf.append("\n");
	}

	if (way_constraints.get_permissive() || way_constraints.get_prohibitive())
	{
	//	buf.append("\n");
		bool any_permissive = false;
		for (sint8 i = 0; i < way_constraints.get_count(); i++)
		{
			if (way_constraints.get_permissive(i))
			{
				if (!any_permissive)
				{
					buf.append("\n");
					buf.append(translator::translate("assets"));
					buf.append(":");
					buf.append("\n");
				}
				any_permissive = true;
				char tmpbuf[30];
				sprintf(tmpbuf, "Permissive %i", i);
				buf.append(translator::translate(tmpbuf));
				buf.append("\n");
			}
		}
		if (is_electrified() && !way_constraints.get_permissive())
		{
			if (!any_permissive)
			{
				buf.append("\n");
				buf.append(translator::translate("assets"));
				buf.append(":");
				buf.append("\n");
			}
			buf.append(translator::translate("elektrified"));
			buf.append("\n");
		}
		bool any_prohibitive = false;
		for (sint8 i = 0; i < way_constraints.get_count(); i++)
		{
			if (way_constraints.get_prohibitive(i))
			{
				if (!any_prohibitive)
				{
					buf.append("\n");
					buf.append(translator::translate("Restrictions:"));
					buf.append("\n");
				}
				any_prohibitive = true;
				char tmpbuf[30];
				sprintf(tmpbuf, "Prohibitive %i", i);
				buf.append(translator::translate(tmpbuf));
				buf.append("\n");
			}
		}
		if (is_height_restricted())
		{
			if (!any_prohibitive)
			{
				buf.append("\n");
				buf.append(translator::translate("Restrictions:"));
				buf.append("\n");
			}
			buf.append(translator::translate("Low bridge"));
			buf.append("\n\n");
		}
	}
	
	#ifdef DEBUG
	buf.append(translator::translate("\nRibi (unmasked)"));
	buf.append(get_ribi_unmasked());

	buf.append(translator::translate("\nRibi (masked)"));
	buf.append(get_ribi());
	buf.append("\n");
#endif
	
#if 1
	//buf.append("\n");
	buf.printf(translator::translate("convoi passed last\nmonth %i\n"), statistics[1][1]);
#else
	// Debug - output stats
	buf.append("\n");
	for (int type = 0; type < MAX_WAY_STATISTICS; type++) {
		for (int month = 0; month < MAX_WAY_STAT_MONTHS; month++) {
			buf.printf("%d ", statistics[month][type]);
		}
		buf.append("\n");
	}
#endif
	buf.append("\n");
}


/**
 * called during map rotation
 * @author prissi
 */
void weg_t::rotate90()
{
	obj_t::rotate90();
	ribi = ribi_t::rotate90( ribi );
	ribi_maske = ribi_t::rotate90( ribi_maske );
}


/**
 * counts signals on this tile;
 * It would be enough for the signals to register and unregister themselves, but this is more secure ...
 * @author prissi
 */
void weg_t::count_sign()
{
	// Either only sign or signal please ...
	flags &= ~(HAS_SIGN|HAS_SIGNAL|HAS_CROSSING);
	const grund_t *gr=welt->lookup(get_pos());
	if(gr) {
		uint8 i = 1;
		// if there is a crossing, the start index is at least three ...
		if(  gr->ist_uebergang()  ) {
			flags |= HAS_CROSSING;
			i = 3;
			const crossing_t* cr = gr->find<crossing_t>();
			sint32 top_speed = cr->get_desc()->get_maxspeed( cr->get_desc()->get_waytype(0)==get_waytype() ? 0 : 1);
			if(  top_speed < max_speed  ) {
				max_speed = top_speed;
			}
		}
		// since way 0 is at least present here ...
		for( ;  i<gr->get_top();  i++  ) {
			obj_t *obj=gr->obj_bei(i);
			// sign for us?
			if(  roadsign_t const* const sign = obj_cast<roadsign_t>(obj)  ) {
				if(  sign->get_desc()->get_wtyp() == get_desc()->get_wtyp()  ) {
					// here is a sign ...
					flags |= HAS_SIGN;
					return;
				}
			}
			if(  signal_t const* const signal = obj_cast<signal_t>(obj)  ) {
				if(  signal->get_desc()->get_wtyp() == get_desc()->get_wtyp()  ) {
					// here is a signal ...
					flags |= HAS_SIGNAL;
					return;
				}
			}
		}
	}
}


void weg_t::set_images(image_type typ, uint8 ribi, bool snow, bool switch_nw)
{
	switch(typ) {
		case image_flat:
		default:
			set_image( desc->get_image_id( ribi, snow ) );
			set_after_image( desc->get_image_id( ribi, snow, true ) );
			break;
		case image_slope:
			set_image( desc->get_slope_image_id( (slope_t::type)ribi, snow ) );
			set_after_image( desc->get_slope_image_id( (slope_t::type)ribi, snow, true ) );
			break;
		case image_switch:
			set_image( desc->get_image_nr_switch(ribi, snow, switch_nw) );
			set_after_image( desc->get_image_nr_switch(ribi, snow, switch_nw, true) );
			break;
		case image_diagonal:
			set_image( desc->get_diagonal_image_id(ribi, snow) );
			set_after_image( desc->get_diagonal_image_id(ribi, snow, true) );
			break;
	}
}


// much faster recalculation of season image
bool weg_t::check_season(const bool calc_only_season_change)
{
	if(  calc_only_season_change  ) { // nothing depends on season, only snowline
		return true;
	}

	// no way to calculate this or no image set (not visible, in tunnel mouth, etc)
	if(  desc == NULL  ||  image == IMG_EMPTY  ) {
		return true;
	}

	grund_t *from = welt->lookup( get_pos() );

	// use snow image if above snowline and above ground
	bool snow = (from->ist_karten_boden() || !from->ist_tunnel()) && (get_pos().z + from->get_weg_yoff() / TILE_HEIGHT_STEP >= welt->get_snowline() || welt->get_climate(get_pos().get_2d()) == arctic_climate);
	bool old_snow = (flags&IS_SNOW) != 0;
	if(  !(snow ^ old_snow)  ) {
		// season is not changing ...
		return true;
	}

	// set snow flake
	flags &= ~IS_SNOW;
	if(  snow  ) {
		flags |= IS_SNOW;
	}

	slope_t::type hang = from->get_weg_hang();
	if(  hang != slope_t::flat  ) {
		set_images( image_slope, hang, snow );
		return true;
	}

	if(  is_diagonal()  ) 
	{
		if( desc->get_diagonal_image_id(ribi, snow) != IMG_EMPTY  ||
			desc->get_diagonal_image_id(ribi, snow, true) != IMG_EMPTY) 
		{
			set_images(image_diagonal, ribi, snow);
		}
		else
		{
			set_images(image_flat, ribi, snow);
		}
	}
	else if(  ribi_t::is_threeway( ribi )  &&  desc->has_switch_image()  ) {
		// there might be two states of the switch; remember it when changing seasons
		if(  image == desc->get_image_nr_switch( ribi, old_snow, false )  ) {
			set_images( image_switch, ribi, snow, false );
		}
		else if(  image == desc->get_image_nr_switch( ribi, old_snow, true )  ) {
			set_images( image_switch, ribi, snow, true );
		}
		else {
			set_images( image_flat, ribi, snow );
		}
	}
	else {
		set_images( image_flat, ribi, snow );
	}

	return true;
}



#ifdef MULTI_THREAD
void weg_t::lock_mutex()
{
	pthread_mutex_lock( &weg_calc_image_mutex );
}


void weg_t::unlock_mutex()
{ 
	pthread_mutex_unlock( &weg_calc_image_mutex );
}
#endif


void weg_t::calc_image()
{
#ifdef MULTI_THREAD
	pthread_mutex_lock( &weg_calc_image_mutex );
#endif
	grund_t *from = welt->lookup(get_pos());
	grund_t *to;
	image_id old_image = image;

	if(  from==NULL  ||  desc==NULL  ||  !from->is_visible()  ) {
		// no ground, in tunnel
		set_image(IMG_EMPTY);
		set_after_image(IMG_EMPTY);
		if(  from==NULL  ) {
			dbg->error( "weg_t::calc_image()", "Own way at %s not found!", get_pos().get_str() );
		}
#ifdef MULTI_THREAD
		pthread_mutex_unlock( &weg_calc_image_mutex );
#endif
		return;	// otherwise crashing during enlargement
	}
	else if(  from->ist_tunnel() &&  from->ist_karten_boden()  &&  (grund_t::underground_mode==grund_t::ugm_none || (grund_t::underground_mode==grund_t::ugm_level && from->get_hoehe()<grund_t::underground_level))  ) {
		// in tunnel mouth, no underground mode
		// TODO: Consider special treatment of tunnel portal images here.
		set_image(IMG_EMPTY);
		set_after_image(IMG_EMPTY);
	}
	else {
		// use snow image if above snowline and above ground
		bool snow = (from->ist_karten_boden() || !from->ist_tunnel()) && (get_pos().z + from->get_weg_yoff() / TILE_HEIGHT_STEP >= welt->get_snowline() || welt->get_climate(get_pos().get_2d()) == arctic_climate);
		flags &= ~IS_SNOW;
		if(  snow  ) {
			flags |= IS_SNOW;
		}

		slope_t::type hang = from->get_weg_hang();
		if(hang != slope_t::flat) {
			// on slope
			set_images(image_slope, hang, snow);
		}
		else {
			static int recursion = 0; /* Communicate among different instances of this method */

			// flat way
			set_images(image_flat, ribi, snow);

			// recalc image of neighbors also when this changed to non-diagonal
			if(recursion == 0) {
				recursion++;
				for(int r = 0; r < 4; r++) {
					if(  from->get_neighbour(to, get_waytype(), ribi_t::nsew[r])  ) {
						// can fail on water tiles
						if(  weg_t *w=to->get_weg(get_waytype())  )  {
							// and will only change the outcome, if it has a diagonal image ...
							if(  w->get_desc()->has_diagonal_image()  ) {
								w->calc_image();
							}
						}
					}
				}
				recursion--;
			}

			// try diagonal image
			if(  desc->has_diagonal_image()  ) {
				check_diagonal();

				// now apply diagonal image
				if(is_diagonal()) {
					if( desc->get_diagonal_image_id(ribi, snow) != IMG_EMPTY  ||
					    desc->get_diagonal_image_id(ribi, snow, true) != IMG_EMPTY) {
						set_images(image_diagonal, ribi, snow);
					}
				}
			}
		}
	}
	if (image!=old_image) {
		sint8 yoff = from ? from->get_weg_yoff() : 0;
		mark_image_dirty(old_image, yoff);
		mark_image_dirty(image, from->get_weg_yoff());
	}
#ifdef MULTI_THREAD
	pthread_mutex_unlock( &weg_calc_image_mutex );
#endif
}

// checks, if this way qualifies as diagonal
void weg_t::check_diagonal()
{
	bool diagonal = false;
	flags &= ~IS_DIAGONAL;

	const ribi_t::ribi ribi = get_ribi_unmasked();
	if(  !ribi_t::is_bend(ribi)  ) {
		// This is not a curve, it can't be a diagonal
		return;
	}

	grund_t *from = welt->lookup(get_pos());
	grund_t *to;

	ribi_t::ribi r1 = ribi_t::none;
	ribi_t::ribi r2 = ribi_t::none;

	// get the ribis of the ways that connect to us
	// r1 will be 45 degree clockwise ribi (eg northeast->east), r2 will be anticlockwise ribi (eg northeast->north)
	if(  from->get_neighbour(to, get_waytype(), ribi_t::rotate45(ribi))  ) {
		r1 = to->get_weg_ribi_unmasked(get_waytype());
	}

	if(  from->get_neighbour(to, get_waytype(), ribi_t::rotate45l(ribi))  ) {
		r2 = to->get_weg_ribi_unmasked(get_waytype());
	}

	// diagonal if r1 or r2 are our reverse and neither one is 90 degree rotation of us
	diagonal = (r1 == ribi_t::backward(ribi) || r2 == ribi_t::backward(ribi)) && r1 != ribi_t::rotate90l(ribi) && r2 != ribi_t::rotate90(ribi);

	if(  diagonal  ) {
		flags |= IS_DIAGONAL;
	}
}


/**
 * new month
 * @author hsiegeln
 */
void weg_t::new_month()
{
	for (int type=0; type<MAX_WAY_STATISTICS; type++) {
		for (int month=MAX_WAY_STAT_MONTHS-1; month>0; month--) {
			statistics[month][type] = statistics[month-1][type];
		}
		statistics[0][type] = 0;
	}
	wear_way(desc->get_monthly_base_wear()); 
}


// correct speed and maintenance
void weg_t::finish_rd()
{
	player_t *player=get_owner();
	if(player && desc) 
	{
		sint32 maint = desc->get_maintenance();
		check_diagonal();
		if(is_diagonal())
		{
			maint *= 10;
			maint /= 14;
		}
		player_t::add_maintenance( player,  maint, desc->get_finance_waytype() );
	}
}


// returns NULL, if removal is allowed
// players can remove public owned ways (Depracated)
const char *weg_t:: is_deletable(const player_t *player, bool allow_public)
{
	if(allow_public && get_owner() && get_owner()->is_public_service()) 
	{
		return NULL;
	}
	return obj_t:: is_deletable(player);
}

/**
 *  Check whether the city should adopt the road.
 *  (Adopting the road sets a speed limit and builds a sidewalk.)
 */
bool weg_t::should_city_adopt_this(const player_t* player)
{
	if(!welt->get_settings().get_towns_adopt_player_roads())
	{
		return false;
	}
	if(get_waytype() != road_wt) 
	{
		// Cities only adopt roads
		return false;
	}
	if(get_desc()->get_styp() == type_elevated)
	{
		// It would be too profitable for players if cities adopted elevated roads
		return false;
	}
	const grund_t* gr = welt->lookup_kartenboden(get_pos().get_2d()); 
	if(gr && get_pos().z < gr->get_hoehe())
	{
		// It would be too profitable for players if cities adopted tunnels
		return false;
	}
	if(player && player->is_public_service())
	{
		// Do not adopt public service roads, so that players can't mess with them
		return false;
	}
	const koord & pos = this->get_pos().get_2d();
	if(!welt->get_city(pos))
	{
		// Don't adopt roads outside the city limits.
		// Note, this also returns false on invalid coordinates
		return false;
	}

	bool has_neighbouring_building = false;
	for(uint8 i = 0; i < 8; i ++)
	{
		// Look for neighbouring buildings at ground level
		const grund_t* const gr = welt->lookup_kartenboden(pos + koord::neighbours[i]);
		if(!gr)
		{
			continue;
		}
		const gebaeude_t* const neighbouring_building = gr->find<gebaeude_t>();
		if(!neighbouring_building) 
		{
			continue;
		}
		const building_desc_t* const desc = neighbouring_building->get_tile()->get_desc();
		// Most buildings count, including station extension buildings.
		// But some do *not*, namely platforms and depots.
		switch(desc->get_type())
		{
			case building_desc_t::city_res:
			case building_desc_t::city_com:
			case building_desc_t::city_ind:
				has_neighbouring_building = true;
				break;
			default:
				; // keep looking
		}
		switch(desc->get_type())
		{
			case building_desc_t::attraction_city:
			case building_desc_t::attraction_land:
			case building_desc_t::monument: // monument
			case building_desc_t::factory: // factory
			case building_desc_t::townhall: // town hall
			case building_desc_t::generic_extension:
			case building_desc_t::headquarters: // HQ
			case building_desc_t::dock: // dock
			case building_desc_t::flat_dock: 
				has_neighbouring_building = (bool)welt->get_city(pos);
				break;
			case building_desc_t::depot:
			case building_desc_t::signalbox:
			case building_desc_t::generic_stop:
			default:
				; // continue checking
		}
	}
	// If we found a neighbouring building, we will adopt the road.
	return has_neighbouring_building;
}

uint32 weg_t::get_condition_percent() const
{
	if(desc->get_wear_capacity() == 0)
	{
		// Necessary to avoid divisions by zero on mothballed ways
		return 0;
	}
	// Necessary to avoid overflow. Speed not important as this is for the UI.
	// Running calculations should use fractions (e.g., "if(remaining_wear_capacity < desc->get_wear_capacity() / 6)"). 
	const sint64 remaining_wear_capacity_percent = (sint64)remaining_wear_capacity  * 100ll;
	const sint64 intermediate_result = remaining_wear_capacity_percent / (sint64)desc->get_wear_capacity();
	return (uint32)intermediate_result; 
}

void weg_t::wear_way(uint32 wear)
{
	if(!wear || remaining_wear_capacity == UINT32_MAX_VALUE)
	{
		// If ways are defined with UINT32_MAX_VALUE,
		// this feature is intended to be disabled.
		return;
	}
	if(remaining_wear_capacity > wear)
	{
		const uint32 degridation_fraction = welt->get_settings().get_way_degradation_fraction();
		remaining_wear_capacity -= wear;
		if(remaining_wear_capacity < desc->get_wear_capacity() / degridation_fraction)
		{
			if(!renew())
			{
				degrade();
			}
		}
	}
	else
	{
		remaining_wear_capacity = 0;
		if(!renew())
		{
			degrade();
		}
	}
}

bool weg_t::renew()
{
	if(!replacement_way)
	{
		return false;
	}

	player_t* const owner = get_owner();
	bool success = false;
	const sint64 price = desc->get_upgrade_group() == replacement_way->get_upgrade_group() ? replacement_way->get_way_only_cost() : replacement_way->get_value();
	if(welt->get_city(get_pos().get_2d()) || (owner && (owner->can_afford(price) || owner->is_public_service())))
	{
		// Unowned ways in cities are assumed to be owned by the city and will be renewed by it.
		waytype_t wt = replacement_way->get_waytype();
		const uint16 time = welt->get_timeline_year_month();
		bool is_current = !time || (replacement_way->get_intro_year_month() <= time && time < replacement_way->get_retire_year_month());
		if(!is_current)
		{
			way_constraints_of_vehicle_t constraints;
			constraints.set_permissive(desc->get_way_constraints().get_permissive());
			constraints.set_prohibitive(desc->get_way_constraints().get_prohibitive());
			replacement_way = way_builder_t::weg_search(wt, replacement_way->get_topspeed(), (const sint32)replacement_way->get_axle_load(), time, (systemtype_t)replacement_way->get_styp(), replacement_way->get_wear_capacity(), constraints);
		}
		
		if(!replacement_way)
		{
			// If the way search cannot find a replacement way, use the current way as a fallback.
			replacement_way = desc;
		}
		
		set_desc(replacement_way);
		success = true;
		if(owner)
		{
			owner->book_way_renewal(price, wt);
		}
	}
	else if(owner && !owner->get_has_been_warned_about_no_money_for_renewals())
	{
		welt->get_message()->add_message(translator::translate("Not enough money to carry out essential way renewal work.\n"), get_pos().get_2d(), message_t::warnings, owner->get_player_nr());
		owner->set_has_been_warned_about_no_money_for_renewals(true); // Only warn once a month.
	}
	else if (!owner && public_right_of_way && wtyp == road_wt)
	{
		// Unowned roads that are public rights of way should be renewed with the latest type.
		const way_desc_t* wb = welt->get_timeline_year_month() ? welt->get_settings().get_intercity_road_type(welt->get_timeline_year_month()) : NULL; // This search only works properly when the timeline is enabled
		set_desc(wb ? wb : desc);
		success = true;
	}

	return success;
}

void weg_t::degrade()
{
	if(public_right_of_way)
	{
		// Do not degrade public rights of way, as these should remain passable.
		// Instead, take them out of private ownership and renew them with the default way.
		const bool initially_unowned = get_owner() == NULL;
		set_owner(NULL); 
		if(wtyp == road_wt)
		{
			const stadt_t* city = welt->get_city(get_pos().get_2d()); 
			if (!initially_unowned && welt->get_timeline_year_month())
			{
				const way_desc_t* wb = city ? welt->get_settings().get_city_road_type(welt->get_timeline_year_month()) : welt->get_settings().get_intercity_road_type(welt->get_timeline_year_month());
				set_desc(wb ? wb : desc);
			}
			else
			{
				// If the timeline is not enabled, renew with the same type, or else the default is just the first way that happens to be in the array and might be something silly like a bridleway
				set_desc(desc);
			}
		}
		else
		{
			set_desc(desc);
		}
	}
	else
	{
		if(remaining_wear_capacity)
		{
			// There is some wear left, but this way is in a degraded state. Reduce the speed limit.
			if(!degraded)
			{
				// Only do this once, or else this will carry on reducing for ever.
				max_speed /= 2;
				degraded = true;
			}
		}
		else
		{
			// Totally worn out: impassable. 
			max_speed = 0;
			degraded = true;
			const way_desc_t* mothballed_type = way_builder_t::way_search_mothballed(get_waytype(), (systemtype_t)desc->get_styp()); 
			if(mothballed_type)
			{
				set_desc(mothballed_type);
				calc_image();
			}
		}
	}
}

signal_t *weg_t::get_signal(ribi_t::ribi direction_of_travel) const
{
	signal_t* sig = welt->lookup(get_pos())->find<signal_t>();
	if (sig && sig->get_desc()->get_working_method() == one_train_staff)
	{
		// This allows a single one train staff cabinet to work for trains passing in both directions.
		return sig;
	}
	ribi_t::ribi way_direction = (ribi_t::ribi)ribi_maske;
	if((direction_of_travel & way_direction) == 0)
	{
		return sig;
	}
	else return NULL;
}