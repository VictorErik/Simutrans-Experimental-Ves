#ifndef freight_list_sorter_h
#define freight_list_sorter_h

// same sorting for stations and vehicle/convoi freight ...

#include "simtypes.h"

template<class T> class slist_tpl;
template<class T> class vector_tpl;
class ware_t;
class cbuffer_t;
class karte_ptr_t;


class freight_list_sorter_t
{
public:
	enum sort_mode_t {
		by_name = 0,
		by_via = 1,
		by_via_sum = 2,
		by_amount = 3, 
		by_origin = 4, 
		by_origin_amount = 5,
		by_destination_detail = 6, 
		by_wealth_detail = 7, 
		by_wealth_via = 8,
		by_accommodation_detail = 9,
		by_accommodation_via = 10/*, 
		by_transfer_time =  */
	};

	static void sort_freight(vector_tpl<ware_t> const& warray, cbuffer_t& buf, sort_mode_t sort_mode, const slist_tpl<ware_t>* full_list, const char* what_doing);

private:
	static karte_ptr_t welt;

	static sort_mode_t sortby;

	static bool compare_ware(ware_t const& w1, ware_t const& w2);

	static void add_ware_heading( cbuffer_t &buf, uint32 sum, uint32 max, const ware_t *ware, const char *what_doing, uint8 g_class );



//	vector_tpl<transferring_cargo_t> *transferring_cargoes;
};


#endif
