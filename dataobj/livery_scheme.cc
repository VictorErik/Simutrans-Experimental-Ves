/*
  * @author: jamespetts, April 2011
  * This file is part of the Simutrans project under the artistic licence.
  * (see licence.txt)
  */

#include "livery_scheme.h"
#include "loadsave.h"
#include "../descriptor/vehicle_desc.h"

livery_scheme_t::livery_scheme_t(const char* n, const uint16 date)
{
	scheme_name = n;
	retire_date = date;
}


const char* livery_scheme_t::get_latest_available_livery(uint16 date, const vehicle_desc_t* desc) const
{
	if(liveries.empty())
	{
		// No liveries available at all
		return NULL;
	}
	const char* livery_name = NULL;
	uint16 latest_valid_intro_date = 0;
	for(uint32 i = 0; i < liveries.get_count(); i ++)
	{
		if(date >= liveries[i].intro_date && desc->check_livery(liveries[i].name.c_str()) && liveries[i].intro_date > latest_valid_intro_date)
		{
			// This returns the most recent livery available for this vehicle that is not in the future.
			latest_valid_intro_date = liveries[i].intro_date;
			livery_name = liveries[i].name.c_str();
		}
	}
	return livery_name;
}

void livery_scheme_t::rdwr(loadsave_t *file)
{
	file->rdwr_string(scheme_name);
	file->rdwr_short(retire_date);
	uint16 count = 0;
	if(file->is_saving())
	{
		count = liveries.get_count();
	}

	file->rdwr_short(count);

	std::string n; 
	uint16 date;

	for(int i = 0; i < count; i ++)
	{
		if(file->is_saving())
		{
			n = liveries.get_element(i).name;
			date = liveries.get_element(i).intro_date;
		}
		else
		{
			n = '\0';
			date = 0;
		}
		
		file->rdwr_string(n);
		file->rdwr_short(date);

		if(file->is_loading())
		{
			livery_t liv;
			liv.name = n;
			liv.intro_date = date;
			liveries.append_unique(liv);
		}	
	}
}

bool livery_scheme_t::operator == (livery_scheme_t comparator) const
{
	if(comparator.scheme_name != scheme_name)
	{
		return false;
	}

	return true;

	// The below allows duplication, which is not desirable.
	/*
	if(comparator.retire_date != retire_date)
	{
		return false;
	}

	if(liveries.get_count() != comparator.liveries.get_count())
	{
		return false;
	}

	for(uint32 i = 0; i < liveries.get_count(); i ++)
	{
		if((const livery_t)liveries[i] != comparator.liveries[i])
		{
			return false;
		}
	}

	return true;*/
}