#include "api.h"

/** @file api_map_objects.cc exports all map-objects. */

#include "api_obj_desc_base.h"
#include "api_simple.h"
#include "../api_class.h"
#include "../api_function.h"

#include "../../simobj.h"
#include "../../simtool.h"
#include "../../simworld.h"
#include "../../dataobj/scenario.h"
#include "../../obj/baum.h"
#include "../../obj/gebaeude.h"
#include "../../obj/label.h"

using namespace script_api;

// use pointers to obj_t_tag[ <type> ] to tag the obj_t classes
static uint8 obj_t_tag[256];

/*
 * template struct to bind obj_t::typ to obj_t classes
 */
template<class D> struct bind_code;

/*
 * Class to templatify access of obj_t objects
 */
template<class D> struct access_objs {

	/*
	 * Access object: check whether object is still on this tile.
	 */
	static D* get_by_pos(HSQUIRRELVM vm, SQInteger index)
	{
		SQUserPointer tag = obj_t_tag + bind_code<D>::objtype;
		SQUserPointer p = NULL;
		if (SQ_SUCCEEDED(sq_getinstanceup(vm, index, &p, tag))  &&  p) {
			D *obj = static_cast<D*>(p);
			koord3d pos = param<koord3d>::get(vm, index);
			grund_t *gr = welt->lookup(pos);
			if (gr  &&  gr->obj_ist_da(obj)) {
				return obj;
			}
			else {
				// object or tile disappeared: clear userpointer
				sq_setinstanceup(vm, index, NULL);
			}
		}
		return NULL;
	}

	/*
	 * Create instance: call constructor with coordinates.
	 */
	static SQInteger push_with_pos(HSQUIRRELVM vm, D* const& obj)
	{
		if (obj == NULL) {
			sq_pushnull(vm);
			return 1;
		}
		koord pos = obj->get_pos().get_2d();
		welt->get_scenario()->koord_w2sq(pos);
		sint16 x = pos.x;
		sint16 y = pos.y;
		sint8  z = obj->get_pos().z;
		assert(bind_code<D>::objtype == obj->get_typ()  ||  bind_code<D>::objtype == obj_t::obj);
		if (!SQ_SUCCEEDED(push_instance(vm, script_api::param<D*>::squirrel_type(), x, y, z, obj->get_typ()))) {
			return SQ_ERROR;
		}
		sq_setinstanceup(vm, -1, obj);
		return 1;
	}

};


SQInteger exp_obj_pos_constructor(HSQUIRRELVM vm) // parameters: sint16 x, sint16 y, sint8 z, obj_t::typ type
{
	// get coordinates
	sint16 x = param<sint16>::get(vm, 2);
	sint16 y = param<sint16>::get(vm, 3);
	sint8  z = param<sint16>::get(vm, 4);
	// set coordinates
	set_slot(vm, "x", x, 1);
	set_slot(vm, "y", y, 1);
	set_slot(vm, "z", z, 1);
	koord pos(x,y);
	welt->get_scenario()->koord_sq2w(pos);
	// find object and set instance up
	if (grund_t *gr = welt->lookup(koord3d(pos, z))) {
		if (obj_t *obj = gr->suche_obj( (obj_t::typ)param<uint8>::get(vm, 5) )) {
			sq_setinstanceup(vm, -1, obj);
			return SQ_OK;
		}
	}
	sq_raise_error(vm, "No object of requested type on tile (or no tile at this position)");
	return SQ_ERROR;
}

// we have to resolve instances of derived classes here...
SQInteger script_api::param<obj_t*>::push(HSQUIRRELVM vm, obj_t* const& obj)
{
	if (obj == NULL) {
		sq_pushnull(vm);
		return 1;
	}
	obj_t::typ type = obj->get_typ();
	switch(type) {
		case obj_t::baum:
			return script_api::param<baum_t*>::push(vm, (baum_t*)obj);

		case obj_t::gebaeude:
			return script_api::param<gebaeude_t*>::push(vm, (gebaeude_t*)obj);

		case obj_t::way:
		{
			waytype_t wt = obj->get_waytype();
			switch(wt) {
				default:
					return script_api::param<weg_t*>::push(vm, (weg_t*)obj);
			}
		}
		default:
			return access_objs<obj_t>::push_with_pos(vm, obj);
	}
}

obj_t* script_api::param<obj_t*>::get(HSQUIRRELVM vm, SQInteger index)
{
	return access_objs<obj_t>::get_by_pos(vm, index);
}

template<> struct bind_code<obj_t> { static const uint8 objtype = obj_t::obj; };

// macro to implement get and push for obj_t's with position
#define getpush_obj_pos(D, type) \
	D* script_api::param<D*>::get(HSQUIRRELVM vm, SQInteger index) \
	{ \
		return access_objs<D>::get_by_pos(vm, index); \
	} \
	SQInteger script_api::param<D*>::push(HSQUIRRELVM vm, D* const& obj) \
	{ \
		return access_objs<D>::push_with_pos(vm, obj); \
	} \
	template<> struct bind_code<D> { static const uint8 objtype = type; };

// implementation of get and push by macros
getpush_obj_pos(baum_t, obj_t::baum);
getpush_obj_pos(gebaeude_t, obj_t::gebaeude);
getpush_obj_pos(label_t, obj_t::label);
getpush_obj_pos(weg_t, obj_t::way);

// return way ribis, have to implement a wrapper, to correctly rotate ribi
static SQInteger get_way_ribi(HSQUIRRELVM vm)
{
	weg_t *w = param<weg_t*>::get(vm, 1);
	bool masked = param<bool>::get(vm, 2);

	ribi_t::ribi ribi = w ? (masked ? w->get_ribi() : w->get_ribi_unmasked() ) : 0;

	return push_ribi(vm, ribi);
}

// create class
template<class D>
void begin_obj_class(HSQUIRRELVM vm, const char* name, const char* base = NULL)
{
	SQInteger res = create_class(vm, name, base);
	if(  !SQ_SUCCEEDED(res)  ) {
		// base class not found: maybe scenario_base.nut is not up-to-date
		dbg->error( "begin_obj_class()", "Create class failed for %s. Base class %s missing. Please update simutrans (or just script/scenario_base.nut)!", name, base );
		sq_raise_error(vm, "Create class failed for %s. Base class %s missing. Please update simutrans (or just script/scenario_base.nut)!", name, base);
	}
	uint8 objtype = bind_code<D>::objtype;
	// store typetag to identify pointers
	sq_settypetag(vm, -1, obj_t_tag + objtype);
	// export constructor
	register_function(vm, exp_obj_pos_constructor, "constructor", 5, "xiiii");
	// now functions can be registered
}

// markers / labels
label_t* create_marker(koord pos, player_t* player, const char* text)
{
	if (player == NULL  ||  text == NULL) {
		return NULL;
	}
	tool_marker_t w;
	w.flags = 0;
	const char* err = w.work(player, koord3d(pos,0));
	if (err) {
		return NULL;
	}
	grund_t *gr = welt->lookup_kartenboden(pos);
	gr->set_text(text);
	return gr->find<label_t>();
}

/* This fails to compile in Visual Studio citing an "ambiguous symbol" error (C2872)
 * "return void_t();" also throws error C2955
 */
/*
void_ label_set_text(label_t* l, const char* text)
{
	if (l == NULL  ||  text == NULL) {
		return void_t();
	}
	koord3d pos = l->get_pos();
	tool_rename_t w;
	cbuffer_t buf;
	buf.printf("m%hi,%hi,%hi,%s", pos.x, pos.y, pos.z, text);
	w.set_default_param(buf);
	w.init(l->get_owner());

	return void_t();
} */


void export_map_objects(HSQUIRRELVM vm)
{
	/**
	 * Class to access objects on the map
	 * These classes cannot modify anything.
	 */
	begin_class(vm, "map_object_x", "extend_get,coord3d");
	uint8 objtype = bind_code<obj_t>::objtype;
	sq_settypetag(vm, -1, obj_t_tag + objtype);
	/**
	 * Constructor. Implemented by derived classes.
	 * Fails if no object of precisely the requested type is on the tile.
	 * Fails for map_object_x itself.
	 * @param x
	 * @param y
	 * @param z
	 * @typemask void(integer,integer,integer)
	 */
	register_function(vm, exp_obj_pos_constructor, "constructor", 5, "xiiii");
	/**
	 * @returns owner of the object.
	 */
	register_method(vm, &obj_t::get_owner, "get_owner");
	/**
	 * @returns raw name.
	 */
	register_method(vm, &obj_t::get_name, "get_name");
	/**
	 * @returns way type, can be @ref wt_invalid.
	 */
	register_method(vm, &obj_t::get_waytype, "get_waytype");
	/**
	 * @returns position.
	 */
	register_method(vm, &obj_t::get_pos, "get_pos");
	/**
	 * Checks whether player can remove this object.
	 * @returns error message or null if object can be removed.
	 */
	register_method(vm, &obj_t:: is_deletable, "is_removable");
	/**
	 * @returns type of object.
	 */
	register_method(vm, &obj_t::get_typ, "get_type");
	end_class(vm);


	/**
	 * Trees on the map.
	 */
	begin_obj_class<baum_t>(vm, "tree_x", "map_object_x");
	/**
	 * @returns age of tree in months.
	 */
	register_method(vm, &baum_t::get_age, "get_age");
	/**
	 * @returns object descriptor.
	 */
	register_method(vm, &baum_t::get_desc, "get_desc");

	end_class(vm);

	/**
	 * Buildings.
	 */
	begin_obj_class<gebaeude_t>(vm, "building_x", "map_object_x");
	/**
	 * @returns factory if building belongs to one, otherwise null
	 */
	register_method(vm, &gebaeude_t::get_fabrik, "get_factory");
	/**
	 * @returns city if building belongs to one, otherwise null
	 */
	register_method(vm, &gebaeude_t::get_stadt, "get_city");
	/**
	 * @returns whether building is townhall
	 */
	register_method(vm, &gebaeude_t::is_townhall, "is_townhall");
	/**
	 * @returns whether building is headquarters
	 */
	register_method(vm, &gebaeude_t::is_headquarter, "is_headquarter");
	/**
	 * @returns whether building is a monument
	 */
	register_method(vm, &gebaeude_t::is_monument, "is_monument");

	register_method(vm, &gebaeude_t::get_adjusted_visitor_demand, "get_adjusted_visitor_demand");
	register_method(vm, &gebaeude_t::get_adjusted_mail_demand, "get_adjusted_mail_demand");
	register_method(vm, &gebaeude_t::get_adjusted_jobs, "get_adjusted_jobs");

//	/**
//	 * Passenger level controls how many passengers will be generated by this building.
//	 * @returns passenger level
//	 */
//	register_method(vm, &gebaeude_t::get_passagier_level, "get_passenger_level");
//	/**
//	 * Mail level controls how many mail will be generated by this building.
//	 * @returns mail level
//	 */
//	register_method(vm, &gebaeude_t::get_mail_level, "get_mail_level");
	/**
	 * @returns object descriptor.
	 */
	register_method(vm, &gebaeude_t::get_tile, "get_desc");
	/**
	 * @returns true if both building tiles are part of one (multi-tile) building
	 */
	register_method(vm, &gebaeude_t::is_same_building, "is_same_building");

	end_class(vm);

	/**
	 * Ways.
	 */
	begin_obj_class<weg_t>(vm, "way_x", "map_object_x");
	/**
	 * @return if this way has sidewalk - only meaningful for roads
	 */
	register_method(vm, &weg_t::hat_gehweg, "has_sidewalk");
	/**
	 * @return whether way is electrified by some way-object
	 */
	register_method(vm, &weg_t::is_electrified, "is_electrified");
	/**
	 * @return whether there is a road-sign associated to the way on the tile
	 */
	register_method(vm, &weg_t::has_sign, "has_sign");
	/**
	 * @return whether there is a signal associated to the way on the tile
	 */
	register_method(vm, &weg_t::has_signal, "has_signal");
	/**
	 * @return whether there is a way-object associated to the way on the tile
	 */
	register_method(vm, &weg_t::has_wayobj, "has_wayobj");
	/**
	 * @return whether there is a crossing associated to the way on the tile
	 */
	register_method(vm, &weg_t::is_crossing, "is_crossing");
	/**
	 * Return directions of this way. One-way signs are ignored here.
	 * @returns direction
	 * @typemask dir()
	 */
	register_function_fv(vm, &get_way_ribi, "get_dirs", 1, "x", freevariable<bool>(false) );
	/**
	 * Return directions of this way. Some signs restrict available directions.
	 * @returns direction
	 * @typemask dir()
	 */
	register_function_fv(vm, &get_way_ribi, "get_dirs_masked", 1, "x", freevariable<bool>(true) );
	/**
	 * @returns object descriptor.
	 */
	register_method(vm, &weg_t::get_desc, "get_desc");
	end_class(vm);


	/**
	 * Labels.
	 */
	begin_obj_class<label_t>(vm, "label_x", "map_object_x");
	/**
	 * Creates a new marker.
	 * @param pos  position
	 * @param pl   owner
	 * @param text text
	 * @returns label_x instance or null if creation failed
	 * @warning cannot be used in network games.
	 */
	STATIC register_method(vm, &create_marker, "create", false, true);
	/**
	 * Set text of marker.
	 * @param text text
	 * @warning cannot be used in network games.
	 */
	// Must comment this out as this fails to compile in Visual Studio
	// FIXME
	 //register_method(vm, &label_set_text, "set_text", true);

	end_class(vm);
}
