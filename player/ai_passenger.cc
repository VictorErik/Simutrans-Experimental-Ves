/*
 * Copyright (c) 2008 Markus Pristovsek
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

/* simple passenger AI (not using trains, not preoptimized network) */

#include "../simcity.h"
#include "../simfab.h"
#include "../simhalt.h"
#include "../simline.h"
#include "../simmenu.h"
#include "../simtools.h"
#include "../simmesg.h"
#include "../simworld.h"

#include "../bauer/brueckenbauer.h"
#include "../bauer/hausbauer.h"
#include "../bauer/tunnelbauer.h"
#include "../bauer/vehikelbauer.h"
#include "../bauer/wegbauer.h"

#include "../dataobj/fahrplan.h"
#include "../dataobj/loadsave.h"

#include "../utils/cbuffer_t.h"
#include "../utils/simstring.h"

#include "../vehicle/simvehikel.h"

#include "ai_passenger.h"
#include "finance.h"

typedef quickstone_hashtable_tpl<haltestelle_t, haltestelle_t::connexion*> connexions_map_single_remote;

ai_passenger_t::ai_passenger_t(karte_t *wl, uint8 nr) : ai_t( wl, nr )
{
	state = NR_INIT;

	road_vehicle = NULL;
	road_weg = NULL;

	next_construction_steps = welt->get_steps() + 50;

	road_transport = true;
	rail_transport = false;
	air_transport = true;
	ship_transport = true;

	start_stadt = end_stadt = NULL;
	ziel = NULL;
	end_ausflugsziel = NULL;
}


/* Activates/deactivates a player
 * @author prissi
 */
bool ai_passenger_t::set_active(bool new_state)
{
	// only activate, when there are buses available!
	if(  new_state  ) {
		new_state = NULL!=vehikel_search( road_wt, 50, 80, warenbauer_t::passagiere, false);
	}
	return spieler_t::set_active( new_state );
}


/* return the hub of a city (always the very first stop) or zero
 * @author prissi
 */
halthandle_t ai_passenger_t::get_our_hub( const stadt_t *s ) const
{
	FOR(slist_tpl<halthandle_t>, const halt, haltestelle_t::get_alle_haltestellen()) {
		if(  halt->get_besitzer()==(const spieler_t *)this  ) {
			if(  halt->get_pax_enabled()  &&  (halt->get_station_type()&haltestelle_t::busstop)!=0  ) {
				koord h=halt->get_basis_pos();
				if(h.x>=s->get_linksoben().x  &&  h.y>=s->get_linksoben().y  &&  h.x<=s->get_rechtsunten().x  &&  h.y<=s->get_rechtsunten().y  ) {
					return halt;
				}
			}
		}
	}
	return halthandle_t();
}


koord ai_passenger_t::find_area_for_hub( const koord lo, const koord ru, const koord basis ) const
{
	// no found found
	koord best_pos = koord::invalid;
	// "shortest" distance
	int dist = 999;

	for( sint16 x=lo.x;  x<=ru.x;  x++ ) {
		for( sint16 y=lo.y;  y<=ru.y;  y++ ) {
			const koord trypos(x,y);
			const grund_t * gr = welt->lookup_kartenboden(trypos);
			if(gr) {
				// flat, solid
				if(  gr->get_typ()==grund_t::boden  &&  gr->get_grund_hang()==hang_t::flach  ) {
					const ding_t* thing = gr->obj_bei(0);
					int test_dist = shortest_distance( trypos, basis );
					if (!thing || !thing->get_besitzer() || thing->get_besitzer() == sim::up_cast<spieler_t const*>(this)) {
						if(  gr->is_halt()  &&  check_owner( gr->get_halt()->get_besitzer(), this )  &&  gr->hat_weg(road_wt)  ) {
							// ok, one halt belongs already to us ... (should not really happen!) but might be a public stop
							return trypos;
						} else if(  test_dist<dist  &&  gr->hat_weg(road_wt)  &&  !gr->is_halt()  ) {
							ribi_t::ribi  ribi = gr->get_weg_ribi_unmasked(road_wt);
							if(  ribi_t::ist_gerade(ribi)  ||  ribi_t::ist_einfach(ribi)  ) {
								best_pos = trypos;
								dist = test_dist;
							}
						} else if(  test_dist+2<dist  &&  gr->ist_natur()  ) {
							// also ok for a stop, but second choice
							// so wie gave it a malus of 2
							best_pos = trypos;
							dist = test_dist+2;
						}
					}
				}
			}
		}
	}
DBG_MESSAGE("ai_passenger_t::find_area_for_hub()","suggest hub at (%i,%i)",best_pos.x,best_pos.y);
	return best_pos;
}


/* tries to built a hub near the koordinate
 * @author prissi
 */
koord ai_passenger_t::find_place_for_hub( const stadt_t *s ) const
{
	halthandle_t h = get_our_hub( s );
	if(h.is_bound()) {
		return h->get_basis_pos();
	}
	return find_area_for_hub( s->get_linksoben(), s->get_rechtsunten(), s->get_pos() );
}


koord ai_passenger_t::find_harbour_pos(karte_t* welt, const stadt_t *s )
{
	koord bestpos = koord::invalid, k;
	sint32 bestdist = 999999;

	// try to find an airport place as close to the city as possible
	for(  k.y=max(6,s->get_linksoben().y-10); k.y<=min(welt->get_size().y-3-6,s->get_rechtsunten().y+10); k.y++  ) {
		for(  k.x=max(6,s->get_linksoben().x-25); k.x<=min(welt->get_size().x-3-6,s->get_rechtsunten().x+10); k.x++  ) {
			sint32 testdist = shortest_distance( k, s->get_pos() );
			if(  testdist<bestdist  ) {
				if(  k.x+2<s->get_linksoben().x  ||  k.y+2<s->get_linksoben().y  ||  k.x>=s->get_rechtsunten().x  ||  k.y>=s->get_rechtsunten().y  ) {
					// malus for out of town
					testdist += 5;
				}
				if(  testdist<bestdist  ) {
					grund_t *gr = welt->lookup_kartenboden(k);
					hang_t::typ hang = gr->get_grund_hang();
					if(gr->ist_natur()  &&  gr->get_hoehe()==welt->get_grundwasser()  &&  hang_t::ist_wegbar(hang)  &&  welt->ist_wasser(k-koord(hang),koord(hang)*4+koord(1,1))  ) {
						// can built busstop here?
						koord bushalt = k+koord(hang);
						gr = welt->lookup_kartenboden(bushalt);
						if(gr  &&  gr->ist_natur()) {
							bestpos = k;
							bestdist = testdist;
						}
					}
				}
			}
		}
	}
	return bestpos;
}


bool ai_passenger_t::create_water_transport_vehikel(const stadt_t* start_stadt, const koord target_pos)
{
	const vehikel_besch_t *v_besch = vehikel_search(water_wt, 10, 40, warenbauer_t::passagiere, false);
	if(v_besch==NULL  ) {
		// no ship there
		return false;
	}

	if(  convoihandle_t::is_exhausted()  ) {
		// too many convois => cannot do anything about this ...
		return false;
	}

	stadt_t *end_stadt = NULL;
	grund_t *ziel = welt->lookup_kartenboden(target_pos);
	gebaeude_t *gb = ziel->find<gebaeude_t>();
	if(gb  &&  gb->ist_rathaus()) {
		end_stadt = gb->get_stadt();
	}
	else if(!ziel->is_halt()  ||  !ziel->ist_wasser()) {
		// not townhall, not factory => we will not built this line for attractions!
		return false;
	}

	halthandle_t start_hub = get_our_hub( start_stadt );
	halthandle_t start_connect_hub;
	koord start_harbour = koord::invalid;
	if(start_hub.is_bound()) {
		if(  (start_hub->get_station_type()&haltestelle_t::dock)==0  ) 
		{
			start_connect_hub = start_hub;
			start_hub = halthandle_t();

			// is there already one harbour next to this one?
			FOR(connexions_map_single_remote, & iter, *start_connect_hub->get_connexions(0) )
			{
				halthandle_t const h = iter.key;
				if( h.is_bound() && h->get_station_type()&haltestelle_t::dock  ) 
				{
					start_hub = h;
					break;
				}
			}
		}
		else {
			start_harbour = start_hub->get_basis_pos();
		}
	}
	// find an dock place
	if(!start_hub.is_bound()) {
		start_harbour = find_harbour_pos( welt, start_stadt );
	}
	if(start_harbour==koord::invalid) {
		// sorry, no suitable place
		return false;
	}
	// same for end
	halthandle_t end_hub = end_stadt ? get_our_hub( end_stadt ) : ziel->get_halt();
	halthandle_t end_connect_hub;
	koord end_harbour = koord::invalid;
	if(end_hub.is_bound()) {
		if(  (end_hub->get_station_type()&haltestelle_t::dock)==0  ) {
			end_connect_hub = end_hub;
			end_hub = halthandle_t();
			// is there already one harbour next to this one?
			FOR(connexions_map_single_remote, & iter, *end_connect_hub->get_connexions(0) ) 
			{
				halthandle_t const h = iter.key;
				if( h.is_bound() && h->get_station_type()&haltestelle_t::dock  ) 
				{
					start_hub = h;
					break;
				}
			}
		}
		else {
			end_harbour = end_hub->get_basis_pos();
		}
	}
	if(!end_hub.is_bound()  &&  end_stadt) {
		// find an dock place
		end_harbour = find_harbour_pos( welt, end_stadt );
	}
	if(end_harbour==koord::invalid) {
		// sorry, no suitable place
		return false;
	}

	const koord start_dx(welt->lookup_kartenboden(start_harbour)->get_grund_hang());
	const koord end_dx(welt->lookup_kartenboden(end_harbour)->get_grund_hang());

	// now we must check, if these two seas are connected
	{
		// we use the free own vehikel_besch_t
		vehikel_besch_t remover_besch( water_wt, 500, vehikel_besch_t::diesel );
		vehikel_t* test_driver = vehikelbauer_t::baue( koord3d(start_harbour-start_dx,welt->get_grundwasser()), this, NULL, &remover_besch );
		test_driver->set_flag( ding_t::not_on_map );
		route_t verbindung;
		bool connected = verbindung.calc_route(welt, koord3d(start_harbour-start_dx,welt->get_grundwasser()), koord3d(end_harbour-end_dx,welt->get_grundwasser()), test_driver, 0, 0, 0);
		delete test_driver;
		if(!connected) {
			return false;
		}
	}

	// built the harbour if neccessary
	if(!start_hub.is_bound()) {
		koord bushalt = start_harbour+start_dx;
		const koord town_road = find_place_for_hub( start_stadt );
		// first: built street to harbour
		if(town_road!=bushalt) {
			wegbauer_t bauigel(welt, this);
			// no bridges => otherwise first tile might be bridge start ...
			bauigel.route_fuer( wegbauer_t::strasse, wegbauer_t::weg_search( road_wt, road_vehicle->get_geschw(), road_vehicle->get_gewicht(), welt->get_timeline_year_month(), weg_t::type_flat ), tunnelbauer_t::find_tunnel(road_wt,road_vehicle->get_geschw(),welt->get_timeline_year_month()), NULL );
			bauigel.set_keep_existing_faster_ways(true);
			bauigel.set_keep_city_roads(true);
			bauigel.set_maximum(10000);
			bauigel.calc_route( welt->lookup_kartenboden(bushalt)->get_pos(), welt->lookup_kartenboden(town_road)->get_pos() );
			if(bauigel.get_count()-1 <= 1) {
				return false;
			}
			bauigel.baue();
		}
	}
	if(!end_hub.is_bound()) {
		koord bushalt = end_harbour+end_dx;
		const koord town_road = find_place_for_hub( end_stadt );
		// first: built street to harbour
		if(town_road!=bushalt) {
			wegbauer_t bauigel(welt, this);
			// no bridges => otherwise first tile might be bridge start ...
			bauigel.route_fuer( wegbauer_t::strasse, wegbauer_t::weg_search( road_wt, road_vehicle->get_geschw(), road_vehicle->get_gewicht(), welt->get_timeline_year_month(), weg_t::type_flat ), tunnelbauer_t::find_tunnel(road_wt,road_vehicle->get_geschw(),welt->get_timeline_year_month()), NULL );
			bauigel.set_keep_existing_faster_ways(true);
			bauigel.set_keep_city_roads(true);
			bauigel.set_maximum(10000);
			bauigel.calc_route( welt->lookup_kartenboden(bushalt)->get_pos(), welt->lookup_kartenboden(town_road)->get_pos() );
			if(bauigel.get_count()-1 <= 1) {
				return false;
			}
			bauigel.baue();
		}
	}
	// now built the stops ... (since the roads were ok!)
	if(!start_hub.is_bound()) {
		/* first we must built the bus stop, since this will be the default stop for all our buses
		 * we want to keep the name of a dock, thus wil will create it beforehand
		 */
		koord bushalt = start_harbour+start_dx;
		const haus_besch_t* busstop_besch = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX );
		// now built the bus stop
		if(!call_general_tool( WKZ_STATION, bushalt, busstop_besch->get_name() )) {
			return false;
		}
		// and change name to dock ...
		halthandle_t halt = welt->lookup(bushalt)->get_halt();
		char* const name = halt->create_name(bushalt, "Dock");
		halt->set_name( name );
		free(name);
		// finally built the dock
		const haus_besch_t* dock_besch = hausbauer_t::get_random_station(haus_besch_t::hafen, water_wt, welt->get_timeline_year_month(), 0);
		welt->lookup_kartenboden(start_harbour)->obj_loesche_alle(this);
		call_general_tool( WKZ_STATION, start_harbour, dock_besch->get_name() );
		start_hub = welt->lookup(start_harbour)->get_halt();
		// eventually we must built a hub in the next town
		start_connect_hub = get_our_hub( start_stadt );
		if(!start_connect_hub.is_bound()) {
			koord sch = find_place_for_hub( start_stadt );
			call_general_tool( WKZ_STATION, sch, busstop_besch->get_name() );
			start_connect_hub = get_our_hub( start_stadt );
			assert( start_connect_hub.is_bound() );
		}
	}
	if(!end_hub.is_bound()) {
		/* agian we must built the bus stop first, since this will be the default stop for all our buses
		 * we want to keep the name of a dock, thus wil will create it beforehand
		 */
		koord bushalt = end_harbour+end_dx;
		const haus_besch_t* busstop_besch = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX );
		// now built the bus stop
		if(!call_general_tool( WKZ_STATION, bushalt, busstop_besch->get_name() )) {
			return false;
		}
		// and change name to dock ...
		halthandle_t halt = welt->lookup(bushalt)->get_halt();
		char* const name = halt->create_name(bushalt, "Dock");
		halt->set_name( name );
		free(name);
		// finally built the dock
		const haus_besch_t* dock_besch = hausbauer_t::get_random_station(haus_besch_t::hafen, water_wt, welt->get_timeline_year_month(), 0 );
		welt->lookup_kartenboden(end_harbour)->obj_loesche_alle(this);
		call_general_tool( WKZ_STATION, end_harbour, dock_besch->get_name() );
		end_hub = welt->lookup(end_harbour)->get_halt();
		// eventually we must built a hub in the next town
		end_connect_hub = get_our_hub( end_stadt );
		if(!end_connect_hub.is_bound()) {
			koord ech = find_place_for_hub( end_stadt );
			call_general_tool( WKZ_STATION, ech, busstop_besch->get_name() );
			end_connect_hub = get_our_hub( end_stadt );
			assert( end_connect_hub.is_bound() );
		}
	}

	uint16 const cov = welt->get_settings().get_station_coverage();
	// now we have harbour => find start position for ships
	koord pos1 = start_harbour-start_dx;
	koord start_pos = pos1;
	for (int y = pos1.y - cov; y <= pos1.y + cov; ++y) {
		for (int x = pos1.x - cov; x <= pos1.x + cov; ++x) {
			koord p(x,y);
			const planquadrat_t *plan = welt->lookup(p);
			if(plan) {
				grund_t *gr = plan->get_kartenboden();
				if(  gr->ist_wasser()  &&  !gr->get_halt().is_bound()  ) {
					if(plan->get_haltlist_count()>=1  &&  plan->get_haltlist()[0].halt == start_hub  &&  shortest_distance(start_pos,end_harbour)>shortest_distance(p,end_harbour)) {
						start_pos = p;
					}
				}
			}
		}
	}
	// now we have harbour => find start position for ships
	pos1 = end_harbour-end_dx;
	koord end_pos = pos1;
	for (int y = pos1.y - cov; y <= pos1.y + cov; ++y) {
		for (int x = pos1.x - cov; x <= pos1.x + cov; ++x) {
			koord p(x,y);
			const planquadrat_t *plan = welt->lookup(p);
			if(plan) {
				grund_t *gr = plan->get_kartenboden();
				if(  gr->ist_wasser()  &&  !gr->get_halt().is_bound()  ) {
					if(plan->get_haltlist_count()>=1  &&  plan->get_haltlist()[0].halt == end_hub  &&  shortest_distance(end_pos,start_harbour)>shortest_distance(p,start_harbour)) {
						end_pos = p;
					}
				}
			}
		}
	}

	// since 86.01 we use lines for vehicles ...
	schedule_t *fpl=new schifffahrplan_t();
	fpl->append( welt->lookup_kartenboden(start_pos), 0, 0 );
	fpl->append( welt->lookup_kartenboden(end_pos), 90, 0 );
	fpl->set_aktuell( 1 );
	fpl->eingabe_abschliessen();
	linehandle_t line=simlinemgmt.create_line(simline_t::shipline,this,fpl);
	delete fpl;

	// now create one ship
	vehikel_t* v = vehikelbauer_t::baue( koord3d(start_pos,welt->get_grundwasser()), this, NULL, v_besch);
	convoi_t* cnv = new convoi_t(this);
	cnv->set_name(v->get_besch()->get_name());
	cnv->add_vehikel( v );
	welt->sync_add( cnv );
	cnv->set_line(line);
	cnv->start();

	// eventually build a shuttle bus ...
	if(start_connect_hub.is_bound()  &&  start_connect_hub!=start_hub) {
		koord stops[2] = { start_harbour+start_dx, start_connect_hub->get_basis_pos() };
		create_bus_transport_vehikel( stops[1], 1, stops, 2, false );
	}

	// eventually build a airport shuttle bus ...
	if(end_connect_hub.is_bound()  &&  end_connect_hub!=end_hub) {
		koord stops[2] = { end_harbour+end_dx, end_connect_hub->get_basis_pos() };
		create_bus_transport_vehikel( stops[1], 1, stops, 2, false );
	}

	return true;
}


halthandle_t ai_passenger_t::build_airport(const stadt_t* city, koord pos, int rotation)
{
	// not too close to border?
	if(pos.x<6  ||  pos.y<6  ||  pos.x+3+6>=welt->get_size().x  ||  pos.y+3+6>=welt->get_size().y  ) {
		return halthandle_t();
	}
	// ok, not prematurely doomed
	// can we built airports at all?
	const weg_besch_t *taxi_besch = wegbauer_t::weg_search( air_wt, 25, welt->get_timeline_year_month(), weg_t::type_flat );
	const weg_besch_t *runway_besch = wegbauer_t::weg_search( air_wt, 250, welt->get_timeline_year_month(), weg_t::type_elevated );
	if(taxi_besch==NULL  ||  runway_besch==NULL) {
		return halthandle_t();
	}
	// first, check if at least one tile is within city limits
	const koord lo = city->get_linksoben();
	koord size(3-1,3-1);
	// make sure pos is within city limits!
	if(pos.x<lo.x) {
		pos.x += size.x;
		size.x = -size.x;
	}
	if(pos.y<lo.y) {
		pos.y += size.y;
		size.y = -size.y;
	}
	// find coord to connect in the town
	halthandle_t hub = get_our_hub( city );
	const koord town_road = hub.is_bound() ? hub->get_basis_pos() : find_place_for_hub( city );
	if(  town_road==koord::invalid) {
		return halthandle_t();
	}
	// ok, now we could built it => flatten the land
	sint8 h = max( welt->get_grundwasser()+1, welt->lookup_kartenboden(pos)->get_hoehe() );
	const koord dx( size.x/2, size.y/2 );
	for(  sint16 i=0;  i!=size.y+dx.y;  i+=dx.y  ) {
		for( sint16 j=0;  j!=size.x+dx.x;  j+=dx.x  ) {
			if(!welt->ebne_planquadrat(this,pos+koord(j,i),h)) {
				return halthandle_t();
			}
		}
	}
	// now taxiways
	wegbauer_t bauigel(welt, this);
	// 3x3 layout, first we make the taxiway cross
	koord center=pos+dx;
	bauigel.route_fuer( wegbauer_t::luft, taxi_besch, NULL, NULL );
	bauigel.calc_straight_route( welt->lookup_kartenboden(center+koord::nord)->get_pos(), welt->lookup_kartenboden(center+koord::sued)->get_pos() );
	assert(bauigel.get_count()-1 > 1);
	bauigel.baue();
	bauigel.route_fuer( wegbauer_t::luft, taxi_besch, NULL, NULL );
	bauigel.calc_straight_route( welt->lookup_kartenboden(center+koord::west)->get_pos(), welt->lookup_kartenboden(center+koord::ost)->get_pos() );
	assert(bauigel.get_count()-1 > 1);
	bauigel.baue();
	// now try to connect one of the corners with a road
	koord bushalt = koord::invalid, runwaystart, runwayend;
	koord trypos[4] = { koord(0,0), koord(size.x,0), koord(0,size.y), koord(size.x,size.y) };
	uint32 length=9999;
	rotation=-1;

	bauigel.route_fuer( wegbauer_t::strasse, wegbauer_t::weg_search( road_wt, road_vehicle->get_geschw(), road_vehicle->get_gewicht(), welt->get_timeline_year_month(), weg_t::type_flat ), tunnelbauer_t::find_tunnel(road_wt,road_vehicle->get_geschw(),welt->get_timeline_year_month()), brueckenbauer_t::find_bridge(road_wt,road_vehicle->get_geschw(),welt->get_timeline_year_month()) );
	bauigel.set_keep_existing_faster_ways(true);
	bauigel.set_keep_city_roads(true);
	bauigel.set_maximum(10000);

	// find the closest one to town ...
	for(  int i=0;  i<4;  i++  ) {
		bushalt = pos+trypos[i];
		bauigel.calc_route(welt->lookup_kartenboden(bushalt)->get_pos(),welt->lookup_kartenboden(town_road)->get_pos());
		// no road => try next
		if(  bauigel.get_count() >= 2  &&   bauigel.get_count() < length+1  ) {
			rotation = i;
			length = bauigel.get_count()-1;
		}
	}

	if(rotation==-1) {
		// if we every get here that means no connection road => remove airport
		welt->lookup_kartenboden(center+koord::nord)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::sued)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::west)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::ost)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center)->remove_everything_from_way( this, air_wt, ribi_t::alle );
		return halthandle_t();
	}

	bushalt = pos+trypos[rotation];
	bauigel.calc_route(welt->lookup_kartenboden(bushalt)->get_pos(),welt->lookup_kartenboden(town_road)->get_pos());
	bauigel.baue();
	// now the busstop (our next hub ... )
	const haus_besch_t* busstop_besch = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX );
	// get an airport name (even though the hub is the bus stop ... )
	// now built the bus stop
	if(!call_general_tool( WKZ_STATION, bushalt, busstop_besch->get_name() )) {
		welt->lookup_kartenboden(center+koord::nord)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::sued)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::west)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center+koord::ost)->remove_everything_from_way( this, air_wt, ribi_t::keine );
		welt->lookup_kartenboden(center)->remove_everything_from_way( this, air_wt, ribi_t::alle );
		return halthandle_t();
	}
	// and change name to airport ...
	halthandle_t halt = welt->lookup(bushalt)->get_halt();
	char* const name = halt->create_name(bushalt, "Airport");
	halt->set_name( name );
	free(name);
	// built also runway now ...
	bauigel.route_fuer( wegbauer_t::luft, runway_besch, NULL, NULL );
	bauigel.calc_straight_route( welt->lookup_kartenboden(pos+trypos[rotation==0?3:0])->get_pos(), welt->lookup_kartenboden(pos+trypos[1+(rotation&1)])->get_pos() );
	assert(bauigel.get_count()-1 > 1);
	bauigel.baue();
	// now the airstops (only on single tiles, this will always work
	const haus_besch_t* airstop_besch = hausbauer_t::get_random_station(haus_besch_t::generic_stop, air_wt, welt->get_timeline_year_month(), 0 );
	for(  int i=0;  i<4;  i++  ) {
		if(  shortest_distance(center+koord::nsow[i],bushalt)==1  &&  ribi_t::ist_einfach( welt->lookup_kartenboden(center+koord::nsow[i])->get_weg_ribi_unmasked(air_wt) )  ) {
			call_general_tool( WKZ_STATION, center+koord::nsow[i], airstop_besch->get_name() );
		}
	}
	// and now the one far away ...
	for(  int i=0;  i<4;  i++  ) {
		if(  shortest_distance(center+koord::nsow[i],bushalt)>1  &&  ribi_t::ist_einfach( welt->lookup_kartenboden(center+koord::nsow[i])->get_weg_ribi_unmasked(air_wt) )  ) {
			call_general_tool( WKZ_STATION, center+koord::nsow[i], airstop_besch->get_name() );
		}
	}
	// sucess
	return halt;
}


static koord find_airport_pos(karte_t* welt, const stadt_t *s )
{
	koord bestpos = koord::invalid, k;
	sint32 bestdist = 999999;

	// try to find an airport place as close to the city as possible
	for(  k.y=max(6,s->get_linksoben().y-10); k.y<=min(welt->get_size().y-3-6,s->get_rechtsunten().y+10); k.y++  ) {
		for(  k.x=max(6,s->get_linksoben().x-25); k.x<=min(welt->get_size().x-3-6,s->get_rechtsunten().x+10); k.x++  ) {
			sint32 testdist = shortest_distance( k, s->get_pos() );
			if(  testdist<bestdist  ) {
				if(  k.x+2<s->get_linksoben().x  ||  k.y+2<s->get_linksoben().y  ||  k.x>=s->get_rechtsunten().x  ||  k.y>=s->get_rechtsunten().y  ) {
					// malus for out of town
					testdist += 5;
				}
				if(  testdist<bestdist  &&  welt->square_is_free( k, 3, 3, NULL, ALL_CLIMATES )  ) {
					bestpos = k;
					bestdist = testdist;
				}
			}
		}
	}
	return bestpos;
}


/* builts airports and planes
 * @author prissi
 */
bool ai_passenger_t::create_air_transport_vehikel(const stadt_t *start_stadt, const stadt_t *end_stadt)
{
	const vehikel_besch_t *v_besch = vehikel_search(air_wt, 10, 900, warenbauer_t::passagiere, false);
	if(v_besch==NULL) {
		// no aircraft there
		return false;
	}

	if(  convoihandle_t::is_exhausted()  ) {
		// too many convois => cannot do anything about this ...
		return false;
	}

	halthandle_t start_hub = get_our_hub( start_stadt );
	halthandle_t start_connect_hub;
	koord start_airport;
	if(start_hub.is_bound()) {
		if(  (start_hub->get_station_type()&haltestelle_t::airstop)==0  ) {
			start_connect_hub = start_hub;
			start_hub = halthandle_t();
			// is there already one airport next to this town?
			FOR(connexions_map_single_remote, & iter, *start_connect_hub->get_connexions(0) ) 
			{
				halthandle_t const h = iter.key;
				if( h.is_bound() && h->get_station_type()&haltestelle_t::airstop  )
				{
					start_hub = h;
					break;
				}
			}
		}
		else {
			start_airport = start_hub->get_basis_pos();
		}
	}
	// find an airport place
	if(!start_hub.is_bound()) {
		start_airport = find_airport_pos( welt, start_stadt );
		if(start_airport==koord::invalid) {
			// sorry, no suitable place
			return false;
		}
	}
	// same for end
	halthandle_t end_hub = get_our_hub( end_stadt );
	halthandle_t end_connect_hub;
	koord end_airport;
	if(end_hub.is_bound()) {
		if(  (end_hub->get_station_type()&haltestelle_t::airstop)==0  ) {
			end_connect_hub = end_hub;
			end_hub = halthandle_t();
			// is there already one airport next to this town?
			FOR(connexions_map_single_remote, & iter, *end_connect_hub->get_connexions(0) ) 
			{
				halthandle_t const h = iter.key;
				if( h.is_bound() && h->get_station_type()&haltestelle_t::airstop  ) 
				{
					start_hub = h;
					break;
				}
			}
		}
		else {
			end_airport = end_hub->get_basis_pos();
		}
	}
	if(!end_hub.is_bound()) {
		// find an airport place
		end_airport = find_airport_pos( welt, end_stadt );
		if(end_airport==koord::invalid) {
			// sorry, no suitable place
			return false;
		}
	}
	// eventually construct them
	if(start_airport!=koord::invalid  &&  end_airport!=koord::invalid) {
		// built the airport if neccessary
		if(!start_hub.is_bound()) {
			start_hub = build_airport(start_stadt, start_airport, true);
			if(!start_hub.is_bound()) {
				return false;
			}
			start_connect_hub = get_our_hub( start_stadt );
			if(!start_connect_hub.is_bound()) {
				koord sch = find_place_for_hub( start_stadt );
				// probably we must construct a city hub, since the airport is outside of the city limits
				const haus_besch_t* busstop_besch = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX );
				call_general_tool( WKZ_STATION, sch, busstop_besch->get_name() );
				start_connect_hub = get_our_hub( start_stadt );
				assert( start_connect_hub.is_bound() );
			}
		}
		if(!end_hub.is_bound()) {
			end_hub = build_airport(end_stadt, end_airport, true);
			if(!end_hub.is_bound()) 
			{
				if(start_hub->get_connexions(0)->empty())
				{
					// remove airport busstop
					welt->lookup_kartenboden(start_hub->get_basis_pos())->remove_everything_from_way( this, road_wt, ribi_t::keine );
					koord center = start_hub->get_basis_pos() + koord( welt->lookup_kartenboden(start_hub->get_basis_pos())->get_weg_ribi_unmasked( air_wt ) );
					// now the remaining taxi-/runways
					for( sint16 y=center.y-1;  y<=center.y+1;  y++  ) 
					{
						for( sint16 x=center.x-1;  x<=center.x+1;  x++  ) 
						{
							welt->lookup_kartenboden(koord(x,y))->remove_everything_from_way( this, air_wt, ribi_t::keine );
						}
					}
				}
				return false;
			}
			end_connect_hub = get_our_hub( end_stadt );
			if(!end_connect_hub.is_bound()) {
				koord ech = find_place_for_hub( end_stadt );
				// probably we must construct a city hub, since the airport is outside of the city limits
				const haus_besch_t* busstop_besch = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX );
				call_general_tool( WKZ_STATION, ech, busstop_besch->get_name() );
				end_connect_hub = get_our_hub( end_stadt );
				assert( end_connect_hub.is_bound() );
			}
		}
	}
	// now we have aiports (albeit first tile is bus stop)
	const grund_t *start = start_hub->find_matching_position(air_wt);
	const grund_t *end = end_hub->find_matching_position(air_wt);

	// since 86.01 we use lines for vehicles ...
	schedule_t *fpl=new airfahrplan_t();
	fpl->append( start, 0, 0 );
	fpl->append( end, 90, 0 );
	fpl->set_aktuell( 1 );
	fpl->eingabe_abschliessen();
	linehandle_t line=simlinemgmt.create_line(simline_t::airline,this,fpl);
	delete fpl;

	// now create one plane
	vehikel_t* v = vehikelbauer_t::baue( start->get_pos(), this, NULL, v_besch);
	convoi_t* cnv = new convoi_t(this);
	cnv->set_name(v->get_besch()->get_name());
	cnv->add_vehikel( v );
	welt->sync_add( cnv );
	cnv->set_line(line);
	cnv->start();

	// eventually build a airport shuttle bus ...
	if(start_connect_hub.is_bound()  &&  start_connect_hub!=start_hub) {
		koord stops[2] = { start_hub->get_basis_pos(), start_connect_hub->get_basis_pos() };
		create_bus_transport_vehikel( stops[1], 1, stops, 2, false );
	}

	// eventually build a airport shuttle bus ...
	if(end_connect_hub.is_bound()  &&  end_connect_hub!=end_hub) {
		koord stops[2] = { end_hub->get_basis_pos(), end_connect_hub->get_basis_pos() };
		create_bus_transport_vehikel( stops[1], 1, stops, 2, false );
	}

	return true;
}


/* creates a more general road transport
 * @author prissi
 */
void ai_passenger_t::create_bus_transport_vehikel(koord startpos2d,int anz_vehikel,koord *stops,int anzahl,bool do_wait)
{
DBG_MESSAGE("ai_passenger_t::create_bus_transport_vehikel()","bus at (%i,%i)",startpos2d.x,startpos2d.y);
	// now start all vehicle one field before, so they load immediately
	koord3d startpos = welt->lookup_kartenboden(startpos2d)->get_pos();

	// since 86.01 we use lines for road vehicles ...
	schedule_t *fpl=new autofahrplan_t();
	// do not start at current stop => wont work ...
	for(int j=0;  j<anzahl;  j++) {
		fpl->append(welt->lookup_kartenboden(stops[j]), j == 0 || !do_wait ? 0 : 10);
	}
	fpl->set_aktuell( stops[0]==startpos2d );
	fpl->eingabe_abschliessen();
	linehandle_t line=simlinemgmt.create_line(simline_t::truckline,this,fpl);
	delete fpl;

	// now create all vehicles as convois
	for(int i=0;  i<anz_vehikel;  i++) {
		vehikel_t* v = vehikelbauer_t::baue(startpos, this, NULL, road_vehicle);
		if(  convoihandle_t::is_exhausted()  ) {
			// too many convois => cannot do anything about this ...
			return;
		}
		convoi_t* cnv = new convoi_t(this);
		// V.Meyer: give the new convoi name from first vehicle
		cnv->set_name(v->get_besch()->get_name());
		cnv->add_vehikel( v );

		welt->sync_add( cnv );
		cnv->set_line(line);
		cnv->start();
	}
}


/* now we follow all adjacent streets recursively and mark them
 * if they below to this stop, then we continue
 */
void ai_passenger_t::walk_city(linehandle_t const line, grund_t* const start, int const limit)
{
	//maximum number of stops reached?
	if(line->get_schedule()->get_count()>=limit)  {
		return;
	}

	ribi_t::ribi ribi = start->get_weg_ribi(road_wt);

	for(int r=0; r<4; r++) {

		// a way in our direction?
		if(  (ribi & ribi_t::nsow[r])==0  ) {
			continue;
		}

		// ok, if connected, not marked, and not owner by somebody else
		grund_t *to;
		if(  start->get_neighbour(to, road_wt, ribi_t::nsow[r] )  &&  !welt->ist_markiert(to)  &&  check_owner(to->obj_bei(0)->get_besitzer(),this)  ) {

			// ok, here is a valid street tile
			welt->markiere(to);

			// can built a station here
			if(  ribi_t::ist_gerade(to->get_weg_ribi(road_wt))  ) {

				// find out how many tiles we have covered already
				int covered_tiles=0;
				int house_tiles=0;
				uint16 const cov = welt->get_settings().get_station_coverage();
				for (sint16 y = to->get_pos().y - cov; y <= to->get_pos().y + cov + 1; ++y) {
					for (sint16 x = to->get_pos().x - cov; x <= to->get_pos().x + cov + 1; ++x) {
						const planquadrat_t *pl = welt->lookup(koord(x,y));
						// check, if we have a passenger stop already here
						if(pl  &&  pl->get_haltlist_count()>0) {
							const nearby_halt_t *hl = pl->get_haltlist();
							for( uint8 own=0;  own<pl->get_haltlist_count();  own++  ) {
								if(  hl[own].halt->is_enabled(warenbauer_t::INDEX_PAS)  ) {
									// our stop => nothing to do
#if AUTOJOIN_PUBLIC
									// we leave also public stops alone
									if(  hl[own].halt->get_besitzer()==this  ||  hl[own]->get_besitzer()==welt->get_spieler(1)  ) {
#else
									if(  hl[own].halt->get_besitzer()==this  ) {
#endif
										covered_tiles ++;
										break;
									}
								}
							}
						}
						// check for houses
						if(pl  &&  pl->get_kartenboden()->get_typ()==grund_t::fundament) {
							house_tiles++;
						}
					}
				}
				// now decide, if we build here
				// just using the ration of covered tiles versus house tiles
				int const max_tiles = cov * 2 + 1;
				if(  covered_tiles<(max_tiles*max_tiles)/3  &&  house_tiles>=3  ) {
					// ok, lets do it
					const haus_besch_t* bs = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX);
					if(  call_general_tool( WKZ_STATION, to->get_pos().get_2d(), bs->get_name() )  ) {
						//add to line
						line->get_schedule()->append(to,0); // no need to register it yet; done automatically, when convois will be assinged
					}
				}
				// start road, but no houses anywhere => stop searching
				if(house_tiles==0) {
					return;
				}
			}
			// now do recursion
			walk_city( line, to, limit );
		}
	}
}


/* tries to cover a city with bus stops that does not overlap much and cover as much as possible
 * returns the line created, if sucessful
 */
void ai_passenger_t::cover_city_with_bus_route(koord start_pos, int number_of_stops)
{
	if(  convoihandle_t::is_exhausted()  ) {
		// too many convois => cannot do anything about this ...
		return;
	}

	// nothing in lists
	welt->unmarkiere_alle();

	// and init all stuff for recursion
	grund_t *start = welt->lookup_kartenboden(start_pos);
	linehandle_t line = simlinemgmt.create_line( simline_t::truckline,this, new autofahrplan_t() );
	line->get_schedule()->append(start,0);

	// now create a line
	walk_city( line, start, number_of_stops );
	line->get_schedule()->eingabe_abschliessen();

	road_vehicle = vehikel_search( road_wt, 1, 50, warenbauer_t::passagiere, false);
	if( line->get_schedule()->get_count()>1  ) {
		// success: add a bus to the line
		vehikel_t* v = vehikelbauer_t::baue(start->get_pos(), this, NULL, road_vehicle);
		convoi_t* cnv = new convoi_t(this);

		cnv->set_name(v->get_besch()->get_name());
		cnv->add_vehikel( v );

		welt->sync_add( cnv );
		cnv->set_line(line);
		cnv->start();
	}
	else {
		simlinemgmt.delete_line( line );
	}
}


void ai_passenger_t::step()
{
	// needed for schedule of stops ...
	spieler_t::step();

	if(!automat) {
		// I am off ...
		return;
	}

	// one route per month ...
	if(  welt->get_steps() < next_construction_steps  ) {
		return;
	}

	switch(state) {
		case NR_INIT:
		{
			// time to update hq?
			built_update_headquarter();

			// assume fail
			state = CHECK_CONVOI;

			/* if we have this little money, we do nothing
			 * The second condition may happen due to extensive replacement operations;
			 * in such a case it is save enough to expand anyway.
			 */
			if(!(finance->get_account_balance()>0  ||  finance->has_money_or_assets())  ) {
				return;
			}

			const weighted_vector_tpl<stadt_t*>& staedte = welt->get_staedte();
			int anzahl = staedte.get_count();
			int offset = (anzahl>1) ? simrand(anzahl-1, "ai_passenger_t::step()") : 0;
			// start with previous target
			const stadt_t* last_start_stadt=start_stadt;
			start_stadt = end_stadt;
			end_stadt = NULL;
			end_ausflugsziel = NULL;
			ziel = NULL;
			platz2 = koord::invalid;
			// if no previous town => find one
			if(start_stadt==NULL) {
				// larger start town preferred
				start_stadt = pick_any_weighted(staedte);
				offset = staedte.index_of(start_stadt);
			}
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","using city %s for start",start_stadt->get_name());
			const halthandle_t start_halt = get_our_hub(start_stadt);
			// find starting place

if(!start_halt.is_bound()) {
	DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","new_hub");
}
			platz1 = start_halt.is_bound() ? start_halt->get_basis_pos() : find_place_for_hub( start_stadt );
			if(platz1==koord::invalid) {
				return;
			}
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","using place (%i,%i) for start",platz1.x,platz1.y);

			if(anzahl==1  ||  simrand(3, "ai_passenger_t::step()")==0) {
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","searching attraction");
				// 25 % of all connections are tourist attractions
				const weighted_vector_tpl<gebaeude_t*> &ausflugsziele = welt->get_ausflugsziele();
				// this way, we are sure, our factory is connected to this town ...
				const vector_tpl<stadt_t::factory_entry_t> &fabriken = start_stadt->get_target_factories_for_pax().get_entries();
				unsigned	last_dist = 0xFFFFFFFF;
				bool ausflug=simrand(2, "ai_passenger_t::step()")!=0;	// holidays first ...
				int ziel_count=ausflug?ausflugsziele.get_count():fabriken.get_count();
				for( int i=0;  i<ziel_count;  i++  ) {
					unsigned	dist;
					koord pos, size;
					if(ausflug) {
						const gebaeude_t* a = ausflugsziele[i];
						if (a->get_post_level() <= 25) {
							// not a good object to go to ...
							continue;
						}
						pos  = a->get_pos().get_2d();
						size = a->get_tile()->get_besch()->get_groesse(a->get_tile()->get_layout());
					}
					else {
						const fabrik_t* f = fabriken[i].factory;
						const fabrik_besch_t *const besch = f->get_besch();
						if (( besch->get_pax_demand()==65535 ? besch->get_pax_level() : besch->get_pax_demand() ) <= 10) {
							// not a good object to go to ... we want more action ...
							continue;
						}
						pos  = f->get_pos().get_2d();
						size = f->get_besch()->get_haus()->get_groesse(f->get_rotate());
					}
					const stadt_t *next_town = welt->suche_naechste_stadt(pos);
					if(next_town==NULL  ||  start_stadt==next_town) {
						// this is either a town already served (so we do not create a new hub)
						// or a lonely point somewhere
						// in any case we do not want to serve this location already
						uint16 const c   = welt->get_settings().get_station_coverage();
						koord  const cov = koord(c, c);
						koord test_platz=find_area_for_hub(pos-cov,pos+size+cov,pos);
						if(  !get_halt(test_platz).is_bound()  ) {
							// not served
							dist = shortest_distance(platz1,test_platz);
							if(dist+simrand(50, "ai_passenger_t::step()")<last_dist  &&   dist>3) {
								// but closer than the others
								if(ausflug) {
									end_ausflugsziel = ausflugsziele[i];
								}
								else {
									ziel = fabriken[i].factory;
								}
								last_dist = dist;
								platz2 = test_platz;
							}
						}
					}
				}
				// test for success
				if(platz2!=koord::invalid) {
					// found something
					state = NR_SAMMLE_ROUTEN;
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","decision: %s wants to built network between %s and %s",get_name(),start_stadt->get_name(),ausflug?end_ausflugsziel->get_tile()->get_besch()->get_name():ziel->get_name());
				}
			}
			else {
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","searching town");
				int last_dist = 9999999;
				// find a good route
				for( int i=0;  i<anzahl;  i++  ) {
					const int nr = (i+offset)%anzahl;
					const stadt_t* cur = staedte[nr];
					if(cur!=last_start_stadt  &&  cur!=start_stadt) {
						halthandle_t end_halt = get_our_hub(cur);
						int dist = shortest_distance(platz1,cur->get_pos());
						if(  end_halt.is_bound()  &&  is_connected(platz1,end_halt->get_basis_pos(),warenbauer_t::passagiere) ) {
							// already connected
							continue;
						}
						// check if more close
						if(  dist<last_dist  ) {
							end_stadt = cur;
							last_dist = dist;
						}
					}
				}
				// ok, found two cities
				if(start_stadt!=NULL  &&  end_stadt!=NULL) {
					state = NR_SAMMLE_ROUTEN;
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","%s wants to built network between %s and %s",get_name(),start_stadt->get_name(),end_stadt->get_name());
				}
			}
		}
		break;

		// so far only busses
		case NR_SAMMLE_ROUTEN:
		{
			//
			koord end_hub_pos = koord::invalid;
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","Find hub");
			// also for target (if not tourist attraction!)
			if(end_stadt!=NULL) {
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","try to built connect to city %p", end_stadt );
				// target is town
				halthandle_t h = get_our_hub(end_stadt);
				if(h.is_bound()) {
					end_hub_pos = h->get_basis_pos();
				}
				else {
					end_hub_pos = find_place_for_hub( end_stadt );
				}
			}
			else {
				// already found place
				end_hub_pos = platz2;
			}
			// was successful?
			if(end_hub_pos!=koord::invalid) {
				// ok, now we check the vehicle
				platz2 = end_hub_pos;
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","hub found -> NR_BAUE_ROUTE1");
				state = NR_BAUE_ROUTE1;
			}
			else {
				// no success
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","no suitable hub found");
				end_stadt = NULL;
				state = CHECK_CONVOI;
			}
		}
		break;

		// get a suitable vehicle
		case NR_BAUE_ROUTE1:
		// wait for construction semaphore
		{
			// we want the fastest we can get!
			road_vehicle = vehikel_search( road_wt, 50, 80, warenbauer_t::passagiere, false);
			if(road_vehicle!=NULL) {
				// find the best => AI will never survive
//				road_weg = wegbauer_t::weg_search( road_wt, road_vehicle->get_geschw(), welt->get_timeline_year_month(),weg_t::type_flat );
				// find the really cheapest road
				road_weg = wegbauer_t::weg_search( road_wt, road_vehicle->get_geschw(), road_vehicle->get_gewicht(), welt->get_timeline_year_month(), weg_t::type_flat );
				state = NR_BAUE_STRASSEN_ROUTE;
DBG_MESSAGE("ai_passenger_t::do_passenger_ki()","using %s on %s",road_vehicle->get_name(),road_weg->get_name());
			}
			else {
				// no success
				end_stadt = NULL;
				state = CHECK_CONVOI;
			}
		}
		break;

		// built a simple road (no bridges, no tunnels)
		case NR_BAUE_STRASSEN_ROUTE:
		{
			state = NR_BAUE_WATER_ROUTE;	// assume failure
			if(  !ai_t::road_transport  ) {
				// no overland bus lines
				break;
			}
			const haus_besch_t* bs = hausbauer_t::get_random_station(haus_besch_t::generic_stop, road_wt, welt->get_timeline_year_month(), haltestelle_t::PAX);
			if(bs  &&  create_simple_road_transport(platz1, koord(1,1),platz2,koord(1,1),road_weg)  ) {
				// since the road my have led to a crossing at the indended stop position ...
				bool ok = true;
				if(  !get_halt(platz1).is_bound()  ) {
					if(  !call_general_tool( WKZ_STATION, platz1, bs->get_name() )  ) {
						platz1 = find_area_for_hub( platz1-koord(2,2), platz1+koord(2,2), platz1 );
						ok = call_general_tool( WKZ_STATION, platz1, bs->get_name() );
					}
				}
				if(  ok  ) {
					if(  !get_halt(platz2).is_bound()  ) {
						if(  !call_general_tool( WKZ_STATION, platz2, bs->get_name() )  ) {
							platz2 = find_area_for_hub( platz2-koord(2,2), platz2+koord(2,2), platz2 );
							ok = call_general_tool( WKZ_STATION, platz2, bs->get_name() );
						}
					}
				}
				// still continue?
				if(ok) {
					koord list[2]={ platz1, platz2 };
					// wait only, if target is not a hub but an attraction/factory
					create_bus_transport_vehikel(platz1,1,list,2,end_stadt==NULL);
					state = NR_SUCCESS;
					// tell the player
					cbuffer_t buf;
					if(end_ausflugsziel!=NULL) {
						platz1 = end_ausflugsziel->get_pos().get_2d();
						buf.printf(translator::translate("%s now\noffers bus services\nbetween %s\nand attraction\n%s\nat (%i,%i).\n"), get_name(), start_stadt->get_name(), make_single_line_string(translator::translate(end_ausflugsziel->get_tile()->get_besch()->get_name()),2), platz1.x, platz1.y );
						end_stadt = start_stadt;
					}
					else if(ziel!=NULL) {
						platz1 = ziel->get_pos().get_2d();
						buf.printf( translator::translate("%s now\noffers bus services\nbetween %s\nand factory\n%s\nat (%i,%i).\n"), get_name(), start_stadt->get_name(), make_single_line_string(translator::translate(ziel->get_name()),2), platz1.x, platz1.y );
						end_stadt = start_stadt;
					}
					else {
						buf.printf( translator::translate("Travellers now\nuse %s's\nbusses between\n%s \nand %s.\n"), get_name(), start_stadt->get_name(), end_stadt->get_name() );
						// add two intown routes
						cover_city_with_bus_route(platz1, 6);
						cover_city_with_bus_route(platz2, 6);
					}
					welt->get_message()->add_message(buf, platz1, message_t::ai, PLAYER_FLAG|player_nr, road_vehicle->get_basis_bild());
				}
			}
		}
		break;

		case NR_BAUE_WATER_ROUTE:
			if(  !ai_t::ship_transport  ) {
				// no overland bus lines
				state = NR_BAUE_AIRPORT_ROUTE;
				break;
			}
			if(  end_ausflugsziel == NULL  &&  ship_transport  &&
					create_water_transport_vehikel(start_stadt, end_stadt ? end_stadt->get_pos() : ziel->get_pos().get_2d())) {
				// add two intown routes
				cover_city_with_bus_route( get_our_hub(start_stadt)->get_basis_pos(), 6);
				if(end_stadt!=NULL) {
					cover_city_with_bus_route( get_our_hub(end_stadt)->get_basis_pos(), 6);
				}
				else {
					// start again with same town
					end_stadt = start_stadt;
				}
				cbuffer_t buf;
				buf.printf( translator::translate("Ferry service by\n%s\nnow between\n%s \nand %s.\n"), get_name(), start_stadt->get_name(), end_stadt->get_name() );
				welt->get_message()->add_message(buf, end_stadt->get_pos(), message_t::ai, PLAYER_FLAG | player_nr, road_vehicle->get_basis_bild());
				state = NR_SUCCESS;
			}
			else {
				if(  end_ausflugsziel==NULL  &&  ziel==NULL  ) {
					state = NR_BAUE_AIRPORT_ROUTE;
				}
				else {
					state = NR_BAUE_CLEAN_UP;
				}
			}
		break;

		// despite its name: try airplane
		case NR_BAUE_AIRPORT_ROUTE:
			// try airline (if we are wealthy enough) ...
			if(  !air_transport  ||  finance->get_history_com_month(1, ATC_CASH) < finance->get_starting_money()  ||
			     !end_stadt  ||  !create_air_transport_vehikel( start_stadt, end_stadt )  ) {
				state = NR_BAUE_CLEAN_UP;
			}
			else 
			{
				// add two intown routes
				cover_city_with_bus_route( get_our_hub(start_stadt)->get_basis_pos(), 6);
				cover_city_with_bus_route( get_our_hub(end_stadt)->get_basis_pos(), 6);
				cbuffer_t buf;
				buf.printf( translator::translate("Airline service by\n%s\nnow between\n%s \nand %s.\n"), get_name(), start_stadt->get_name(), end_stadt->get_name() );
				welt->get_message()->add_message(buf, end_stadt->get_pos(), message_t::ai, PLAYER_FLAG | player_nr, road_vehicle->get_basis_bild());
				state = NR_SUCCESS;
			}
		break;

		// remove marker etc.
		case NR_BAUE_CLEAN_UP:
			state = CHECK_CONVOI;
			end_stadt = NULL; // otherwise it may always try to built the same route!
		break;

		// successful construction
		case NR_SUCCESS:
		{
			state = CHECK_CONVOI;
			next_construction_steps = welt->get_steps() + simrand( construction_speed/16, "ai_passenger_t::step()" );
		}
		break;


		// add vehicles to crowded lines
		case CHECK_CONVOI:
		{
			// next time: do something different
			state = NR_INIT;
			next_construction_steps = welt->get_steps() + simrand( ai_t::construction_speed, "ai_passenger_t::step()" ) + 25;

			vector_tpl<linehandle_t> lines(0);
			simlinemgmt.get_lines( simline_t::line, &lines);
			const uint32 offset = simrand(lines.get_count(), "ai_passenger_t::step()");
			for (uint32 i = 0;  i<lines.get_count();  i++  ) {
				linehandle_t line = lines[(i+offset)%lines.get_count()];
				if(line->get_linetype()!=simline_t::airline  &&  line->get_linetype()!=simline_t::truckline) {
					continue;
				}

				// remove empty lines
				if(line->count_convoys()==0) {
					simlinemgmt.delete_line(line);
					break;
				}

				// avoid empty schedule ?!?
				assert(!line->get_schedule()->empty());

				// made loss with this line
				if(line->get_finance_history(0,LINE_PROFIT)<0) {
					// try to update the vehicles
					if(welt->use_timeline()  &&  line->count_convoys()>1) {
						// do not update unimportant lines with single vehicles
						slist_tpl <convoihandle_t> obsolete;
						uint32 capacity = 0;
						for(  uint i=0;  i<line->count_convoys();  i++  ) {
							convoihandle_t cnv = line->get_convoy(i);
							if(cnv->has_obsolete_vehicles()) {
								obsolete.append(cnv);
								capacity += cnv->front()->get_besch()->get_zuladung();
							}
						}
						if(capacity>0) {
							// now try to finde new vehicle
							vehikel_t              const& v       = *line->get_convoy(0)->front();
							waytype_t              const  wt      = v.get_waytype();
							vehikel_besch_t const* const  v_besch = vehikelbauer_t::vehikel_search(wt, welt->get_current_month(), 50, welt->get_average_speed(wt), warenbauer_t::passagiere, false, true);
							if (!v_besch->is_retired(welt->get_current_month()) && v_besch != v.get_besch()) {
								// there is a newer one ...
								for(  uint32 new_capacity=0;  capacity>new_capacity;  new_capacity+=v_besch->get_zuladung()) {
									if(  convoihandle_t::is_exhausted()  ) {
										// too many convois => cannot do anything about this ...
										break;
									}
									vehikel_t* v = vehikelbauer_t::baue( line->get_schedule()->eintrag[0].pos, this, NULL, v_besch  );
									convoi_t* new_cnv = new convoi_t(this);
									new_cnv->set_name( v->get_besch()->get_name() );
									new_cnv->add_vehikel( v );
									welt->sync_add( new_cnv );
									new_cnv->set_line(line);
									new_cnv->start();
								}
								// delete all old convois
								while(!obsolete.empty()) {
									obsolete.remove_first()->self_destruct();
								}
								return;
							}
						}
					}
				}
				// next: check for stucked convois ...

				sint64	free_cap = line->get_finance_history(0,LINE_CAPACITY);
				sint64	used_cap = line->get_finance_history(0,LINE_TRANSPORTED_GOODS);

				if(free_cap+used_cap==0) {
					continue;
				}

				sint32 ratio = (sint32)((free_cap*100l)/(free_cap+used_cap));

				// next: check for overflowing lines, i.e. running with 3/4 of the capacity
				if(  ratio<10  &&  !convoihandle_t::is_exhausted()  ) {
					// else add the first convoi again
					vehikel_t* const v = vehikelbauer_t::baue(line->get_schedule()->eintrag[0].pos, this, NULL, line->get_convoy(0)->front()->get_besch());
					convoi_t* new_cnv = new convoi_t(this);
					new_cnv->set_name( v->get_besch()->get_name() );
					new_cnv->add_vehikel( v );
					welt->sync_add( new_cnv );
					new_cnv->set_line( line );
					// on waiting line, wait at alternating stations for load balancing
					if(  line->get_schedule()->eintrag[1].ladegrad==90  &&  line->get_linetype()!=simline_t::truckline  &&  (line->count_convoys()&1)==0  ) {
						new_cnv->get_schedule()->eintrag[0].ladegrad = 90;
						new_cnv->get_schedule()->eintrag[1].ladegrad = 0;
					}
					new_cnv->start();
					return;
				}

				// next: check for too many cars, i.e. running with too many cars
				if(  ratio>40  &&  line->count_convoys()>1) {
					// remove one convoi
					line->get_convoy(0)->self_destruct();
					return;
				}
			}
		}
		break;

		default:
			DBG_MESSAGE("ai_passenger_t::do_passenger_ki()",	"Illegal state %d!", state );
			end_stadt = NULL;
			state = NR_INIT;
	}
}



void ai_passenger_t::rdwr(loadsave_t *file)
{
	if(  file->get_version()<102002  ) {
		// due to an error the player was never saved correctly
		spieler_t::rdwr(file);
		return;
	}

	xml_tag_t t( file, "ai_passenger_t" );

	ai_t::rdwr(file);

	// then check, if we have to do something or the game is too old ...
	if(file->get_version()<101000) {
		// ignore saving, reinit on loading
		if(  file->is_loading()  ) {
			next_construction_steps = welt->get_steps()+simrand(ai_t::construction_speed, "ai_passenger_t::rdwr()");
		}
		return;
	}

	// now save current state ...
	file->rdwr_enum(state);
	if(  file->get_version()<111001  ) {
		file->rdwr_long(ai_t::construction_speed);
		file->rdwr_bool(air_transport);
		file->rdwr_bool(ship_transport);
		road_transport = true;
		rail_transport = false;
	}
	platz1.rdwr( file );
	platz2.rdwr( file );

	if(file->is_saving()) {
		// save current pointers
		sint32 delta_steps = next_construction_steps-welt->get_steps();
		file->rdwr_long(delta_steps);
		koord k = start_stadt ? start_stadt->get_pos() : koord::invalid;
		k.rdwr(file);
		k = end_stadt ? end_stadt->get_pos() : koord::invalid;
		k.rdwr(file);
		koord3d k3d = end_ausflugsziel ? end_ausflugsziel->get_pos() : koord3d::invalid;
		k3d.rdwr(file);
		k3d = ziel ? ziel->get_pos() : koord3d::invalid;
		k3d.rdwr(file);
	}
	else {
		// since steps in loaded game == 0
		file->rdwr_long(next_construction_steps);
		next_construction_steps += welt->get_steps();
		// reinit current pointers
		koord k;
		k.rdwr(file);
		start_stadt = welt->suche_naechste_stadt(k);
		k.rdwr(file);
		end_stadt = welt->suche_naechste_stadt(k);
		koord3d k3d;
		k3d.rdwr(file);
		end_ausflugsziel = welt->lookup(k3d) ? welt->lookup(k3d)->find<gebaeude_t>() : NULL;
		k3d.rdwr(file);
		ziel = fabrik_t::get_fab( welt, k3d.get_2d() );
	}
}



/**
 * Dealing with stucked  or lost vehicles:
 * - delete lost ones
 * - ignore stucked ones
 * @author prissi
 * @date 30-Dec-2008
 */
void ai_passenger_t::bescheid_vehikel_problem(convoihandle_t cnv,const koord3d ziel)
{
	if(  cnv->get_state() == convoi_t::NO_ROUTE  &&  this!=welt->get_active_player()  ) {
			DBG_MESSAGE("ai_passenger_t::bescheid_vehikel_problem","Vehicle %s can't find a route to (%i,%i)!", cnv->get_name(),ziel.x,ziel.y);
			cnv->self_destruct();
			return;
	}
	spieler_t::bescheid_vehikel_problem( cnv, ziel );
}


void ai_passenger_t::laden_abschliessen()
{
	road_vehicle = vehikel_search( road_wt, 50, 80, warenbauer_t::passagiere, false);
	if (road_vehicle == NULL) {
		// reset state
		end_stadt = NULL;
		state = CHECK_CONVOI;
	}
}
