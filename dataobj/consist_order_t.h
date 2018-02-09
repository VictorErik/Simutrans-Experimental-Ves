/*
* This file is part of the Simutrans project under the artistic licence.
* (see licence.txt)
*
* @author: jamespetts, February 2018
*/

#ifndef consist_order_h
#define consist_order_h

#include "../bauer/goods_manager.h"

class vehicle_desc_t;

struct vehicle_description_element
{
	/*
	* If a specific vehicle is required in this slot,
	* this is specified. If not, this is a nullptr, and
	* the rules for vehicle selection are used instead.
	* If this is != nullptr, the rules below here are 
	* ignored.
	*/
	vehicle_desc_t* specific_vehicle = nullptr;

	/*
	* If this is set to true, the vehicle slot is empty.
	* This means that it is permissible for the convoy as
	* assembled by this consist order to have no vehicle
	* in this slot.
	*/
	bool empty = true;

	/* Rules hereinafter */
	
	uint8 engine_type = 0;

	uint8 min_catering = 0;

	uint16 min_range = 0;

	uint16 min_brake_force = 0;
	uint32 min_power = 0;
	uint32 min_tractive_effort = 0;
	uint32 min_topspeed = 0;
	
	uint32 max_weight = 0;
	uint32 max_axle_load = 0;

	uint16 min_capacity = 0;

	/*
	* These rules define which of available
	* vehicles are to be preferred, and do
	* not affect what vehicles will be selected
	* if <=1 vehicles matching the above rules
	* are available
	*
	* For reference, the default order of preference is:
	* Capacity(+) > Power > Speed > Tractive effort > Running cost
	* 
	* + Where the vehicle is a goods carrying vehicle: otherwise, this is ignored
	*/

	enum rule_flag
	{
		prefer_cost_to_power				= (1u << 0),
		prefer_tractive_effort_to_power		= (1u << 1),
		prefer_speed_to_power				= (1u << 2),
		prefer_cost_to_capacity				= (1u << 3),
		prefer_cost_to_speed				= (1u << 4),
		prefer_speed_to_capacity			= (1u << 5)
	};
	
	uint8 rule_flags = 0;
};

class consist_order_element_t
{
	friend class consist_order_t;
protected:
	/* 
	* The goods category of the vehicle that must occupy this slot.
	*/
	uint8 catg_index = goods_manager_t::INDEX_NONE;

	/*
	* All vehicle tags are cleared on the vehicle joining at this slot
	* if this is set to true here.
	*/
	bool clear_all_tags = false;
	
	/*
	* A bitfield of the tags necessary for a loose vehicle to be
	* allowed to couple to this convoy.
	*/
	uint16 tags_required = 0;

	/*
	* A bitfield of the vehicle tags to be set for the vehicle that
	* occupies this slot when it is attached to this convoy by this order.
	*/
	uint16 tags_to_set = 0;

	vector_tpl<vehicle_description_element> vehicle_description;
};

class consist_order_t
{
	friend class schedule_t;
protected:
	/*
	* The unique ID of the schedule entry to which this consist order refers
	*/
	uint16 schedule_entry_id = 0;

	/* The tags that are to be cleared on _all_ vehicles
	* (whether loose or not) in this convoy after the execution
	* of this consist order.
	*/
	uint16 tags_to_clear = 0;

	vector_tpl<consist_order_element_t> orders;

public:

	uint16 get_schedule_entry_id() const { return schedule_entry_id; }
	
	consist_order_element_t& get_order(uint32 element_number)
	{
		return orders[element_number];
	}
};

#endif