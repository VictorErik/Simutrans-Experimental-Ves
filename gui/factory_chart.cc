/*
 * Copyright (c) 2011 Knightly
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

/*
 * Where the current factory chart statistics are calculated
 */

#include "../obj/leitung2.h"
#include "../dataobj/translator.h"

#include "factory_chart.h"
#include "../dataobj/environment.h"

#define CHART_WIDTH (D_DEFAULT_WIDTH-104)
#define CHART_HEIGHT (70)

#define MAX_GOODS_COLOR (24)

static const int goods_color[MAX_GOODS_COLOR] =
{
	/* greyish blue  */ 0,
	/* bright orange */ 33,
	/* cyan          */ 48,
	/* lemon yellow  */ 24,
	/* purple        */ 57,
	/* greyish green */ 80,
	/* lilac         */ 105,
	/* pale brown    */ 89,
	/* blue          */ 144,
	/* dark green    */ 161,
	/* dark brown    */ 177,
	/* dark blue     */ 97,
	/* green         */ 41,
	/* reddish brown */ 113,
	/* magenta       */ 73,
	/* turquoise     */ 121,
	/* red           */ 129,
	/* muddy yellow  */ 192,
	/* bright green  */ 136,
	/* dull orange   */ 65,
	/* pale yellow   */ 167,
	/* pale green    */ 201,
	/* orange        */ 152,
	/* pale purple   */ 217
};

static const char *const input_type[MAX_FAB_GOODS_STAT] =
{
	"Storage", "Arrived", "Consumed", "In Transit"
};

static const char *const output_type[MAX_FAB_GOODS_STAT] =
{
	"Storage", "Delivered", "Produced", "In Transit"
};

static const gui_chart_t::convert_proc goods_convert[MAX_FAB_GOODS_STAT] =
{
	convert_goods, NULL, convert_goods, NULL
};

static const char *const prod_type[MAX_FAB_STAT+1] =
{
	"Produktion", "Power usage",
	"Electricity", "Jobs", "Post",
	"", "", "Commuters", "", "Post",
	"Post", "Consumers",
	"Power output" // put this at the end
};

static const int prod_color[MAX_FAB_STAT] =
{
	COL_BROWN, COL_ELECTRICITY - 1,
	COL_LIGHT_RED, COL_LIGHT_TURQUOISE, COL_ORANGE,
	0, 0, COL_LIGHT_PURPLE, 0, COL_LIGHT_YELLOW,
	COL_YELLOW, COL_GREY3
};

static const gui_chart_t::convert_proc prod_convert[MAX_FAB_STAT] =
{
	NULL, convert_power, convert_boost, convert_boost, convert_boost, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static const gui_chart_t::convert_proc ref_convert[MAX_FAB_REF_LINE] =
{
	convert_boost, convert_boost, convert_boost, convert_power, NULL, NULL
};

static const koord button_pos[MAX_FAB_STAT] =
{
	/* Production */  koord(0, 0), koord(1, 0),
	/* Boost      */  koord(1, 1), koord(2, 1), koord(3, 1),
	/* Max Boost  */
	/* Demand     */
	/* Commuter   */  koord(2, 5), koord(2, 5), koord(2, 4), // koord(2, 5) = unused
	/* Mail       */  koord(2, 5), koord(3, 5), koord(3, 4),
	/* Consumer   */  koord(1, 4)
};

static const int ref_color[MAX_FAB_REF_LINE] =
{
	COL_RED+2, COL_TURQUOISE, COL_ORANGE_RED,
	COL_RED, COL_DODGER_BLUE, COL_LEMON_YELLOW-2
};

static const char *const label_text[MAX_PROD_LABEL+1] =
{
	"(MW)", "Boost (%)", "Max Boost (%)", "Demand", "Arrived", "sended",
	"(KW)" // put this at the end
};

// Max Kielland
// we can't initialise global statics with #defines
// of variables because these variables aren't initialized yet.
// This table is referring to grid positions instead of pixel positions.
static const koord label_pos[MAX_PROD_LABEL] =
{
	koord(2, 0),
	koord(0, 1),
	koord(0, 2),
	koord(0, 3),
	koord(0, 4),
	koord(0, 5)
};


factory_chart_t::factory_chart_t(const fabrik_t *_factory) :
	factory(NULL),
	goods_buttons(NULL),
	goods_labels(NULL),
	goods_button_count(0),
	goods_label_count(0)
{
	if(_factory) {
		set_factory( _factory );
	}
}


void factory_chart_t::set_factory(const fabrik_t *_factory)
{
	if(  factory  ) {
		delete [] goods_buttons;
		delete [] goods_labels;
		goods_button_count = 0;
		goods_label_count = 0;
	}
	factory = _factory;

	const scr_coord_val offset_below_chart = 10 + CHART_HEIGHT + 20;
	const scr_coord_val label_offset = D_GET_CENTER_ALIGN_OFFSET(LINESPACE,D_BUTTON_HEIGHT);
	tab_panel.set_pos( scr_coord(0, 0) );

	// GUI components for goods input/output statistics
	goods_chart.set_pos( scr_coord(10 + 80, D_TAB_HEADER_HEIGHT ) );
	goods_chart.set_size( scr_size( CHART_WIDTH, CHART_HEIGHT ) );
	goods_chart.set_dimension(12, 10000);
	goods_chart.set_background(SYSCOL_CHART_BACKGROUND);
	goods_chart.set_ltr(env_t::left_to_right_graphs);
	const uint32 input_count = factory->get_input().get_count();
	const uint32 output_count = factory->get_output().get_count();
	if(  input_count>0  ||  output_count>0  ) {
		goods_buttons = new button_t[ (input_count + output_count) * MAX_FAB_GOODS_STAT ];
		goods_labels = new gui_label_t[ (input_count>0 ? input_count + 1 : 0) + (output_count>0 ? output_count + 1 : 0) ];
	}
	sint16 goods_label_row = 0;
	if(  input_count>0  ) {
		goods_labels[goods_label_count].set_text( "Verbrauch" );
		goods_labels[goods_label_count].set_pos( scr_coord( D_MARGIN_LEFT, offset_below_chart + label_offset + (D_H_SPACE+D_BUTTON_HEIGHT)*goods_label_row) );
		goods_labels[goods_label_count].set_width(D_BUTTON_WIDTH);
		goods_cont.add_component( goods_labels + goods_label_count );
		goods_label_count ++;
		goods_label_row ++;
		const array_tpl<ware_production_t> &input = factory->get_input();
		for(  uint32 g=0;  g<input_count;  ++g  ) {
			goods_labels[goods_label_count].set_text( input[g].get_typ()->get_name() );
			goods_labels[goods_label_count].set_pos( scr_coord( D_MARGIN_LEFT+(D_H_SPACE<<1), offset_below_chart + label_offset + (D_H_SPACE+D_BUTTON_HEIGHT)*goods_label_row ) );
			goods_labels[goods_label_count].set_width(D_BUTTON_WIDTH-(D_H_SPACE<<1));
			goods_cont.add_component( goods_labels + goods_label_count );
			for(  int s=0;  s<MAX_FAB_GOODS_STAT;  ++s  ) {
				goods_chart.add_curve( goods_color[goods_label_count%MAX_GOODS_COLOR]+2+(s*3)/2, input[g].get_stats(), MAX_FAB_GOODS_STAT, s, MAX_MONTH, false, false, true, 0, goods_convert[s] );
				goods_buttons[goods_button_count].init(button_t::box_state, input_type[s], scr_coord( D_MARGIN_LEFT+(D_H_SPACE+D_BUTTON_WIDTH)*(s%2+1), offset_below_chart+(D_H_SPACE+D_BUTTON_HEIGHT)*(goods_label_row+s/2) ));
				goods_buttons[goods_button_count].background_color = goods_color[goods_label_count%MAX_GOODS_COLOR]+2+(s*3)/2;
				goods_buttons[goods_button_count].pressed = false;
				goods_buttons[goods_button_count].add_listener(this);
				goods_cont.add_component( goods_buttons + goods_button_count );
				goods_button_count ++;
			}
			goods_label_row += 2;
			goods_label_count ++;
		}
	}
	if(  output_count>0  ) {
		goods_labels[goods_label_count].set_text( "Produktion" );
		goods_labels[goods_label_count].set_pos( scr_coord( D_MARGIN_LEFT, offset_below_chart + label_offset + (D_H_SPACE+D_BUTTON_HEIGHT)*goods_label_row ) );
		goods_labels[goods_label_count].set_width( D_BUTTON_WIDTH );
		goods_cont.add_component( goods_labels + goods_label_count );
		goods_label_count ++;
		goods_label_row ++;
		const array_tpl<ware_production_t> &output = factory->get_output();
		for(  uint32 g=0;  g<output_count;  ++g  ) {
			goods_labels[goods_label_count].set_text( output[g].get_typ()->get_name() );
			goods_labels[goods_label_count].set_pos( scr_coord( D_MARGIN_LEFT+(D_H_SPACE<<1), offset_below_chart + label_offset + (D_H_SPACE+D_BUTTON_HEIGHT)*goods_label_row ) );
			goods_labels[goods_label_count].set_width( D_BUTTON_WIDTH );
			goods_cont.add_component( goods_labels + goods_label_count );
			for(  int s=0;  s<3;  ++s  ) {
				goods_chart.add_curve( goods_color[goods_label_count%MAX_GOODS_COLOR]+2+s*2, output[g].get_stats(), MAX_FAB_GOODS_STAT, s, MAX_MONTH, false, false, true, 0, goods_convert[s] );
				goods_buttons[goods_button_count].init(button_t::box_state, output_type[s], scr_coord( D_MARGIN_LEFT+(D_H_SPACE+D_BUTTON_WIDTH)*(s+1), offset_below_chart+(D_H_SPACE+D_BUTTON_HEIGHT)*goods_label_row ));
				goods_buttons[goods_button_count].background_color = goods_color[goods_label_count%MAX_GOODS_COLOR]+2+s*2;
				goods_buttons[goods_button_count].pressed = false;
				goods_buttons[goods_button_count].add_listener(this);
				goods_cont.add_component( goods_buttons + goods_button_count );
				goods_button_count ++;
			}
			goods_label_count ++;
			goods_label_row ++;
		}
	}
	goods_cont.add_component( &goods_chart );
	tab_panel.add_tab( &goods_cont, translator::translate("Goods") );

	// GUI components for other production-related statistics
	prod_chart.set_pos( scr_coord(10 + 80, 10) );
	prod_chart.set_size( scr_size( CHART_WIDTH, CHART_HEIGHT ) );
	prod_chart.set_dimension(12, 10000);
	prod_chart.set_background(SYSCOL_CHART_BACKGROUND);
	prod_chart.set_ltr(env_t::left_to_right_graphs);
	for(  int s=0;  s<MAX_FAB_STAT;  ++s  ) {
		prod_chart.add_curve( prod_color[s], factory->get_stats(), MAX_FAB_STAT, s, MAX_MONTH, false, false, true, 0, prod_convert[s] );
		if (s==1 && factory->get_desc()->is_electricity_producer()) {
			// if power plant, switch label to output
			prod_buttons[s].init(button_t::box_state, prod_type[MAX_FAB_STAT], scr_coord(D_MARGIN_LEFT + (D_H_SPACE + D_BUTTON_WIDTH)*button_pos[s].x, offset_below_chart + (D_H_SPACE + D_BUTTON_HEIGHT)*button_pos[s].y));
		}
		else {
			prod_buttons[s].init(button_t::box_state, prod_type[s], scr_coord(D_MARGIN_LEFT + (D_H_SPACE + D_BUTTON_WIDTH)*button_pos[s].x, offset_below_chart + (D_H_SPACE + D_BUTTON_HEIGHT)*button_pos[s].y));
		}
		prod_buttons[s].background_color = prod_color[s];
		prod_buttons[s].pressed = false;
		// only show buttons, if the is something to do ...
		if(
			(s==FAB_BOOST_ELECTRIC  &&  (factory->get_desc()->is_electricity_producer()  ||  factory->get_desc()->get_electric_boost()==0))  ||
			(s==FAB_BOOST_PAX  &&  factory->get_desc()->get_pax_boost()==0)  ||
			(s==FAB_BOOST_MAIL  &&  factory->get_desc()->get_mail_boost()==0) ||
			(s==FAB_CONSUMER_ARRIVED && factory->get_sector() != fabrik_t::end_consumer) ||
			s == FAB_PAX_GENERATED || s == FAB_PAX_DEPARTED || s == FAB_MAIL_GENERATED
			) {
			prod_buttons[s].disable();
		}
		else {
			prod_buttons[s].add_listener(this);
			prod_cont.add_component( prod_buttons + s );
		}
		if(  s==FAB_BOOST_MAIL  ) {
			// insert the reference line buttons here to ensure correct tab order
			for(  int r=0;  r<MAX_FAB_REF_LINE;  ++r  ) {
				prod_chart.add_line( ref_color[r], prod_ref_line_data + r, MAX_MONTH, false, true, 0, ref_convert[r] );
				prod_ref_line_buttons[r].init(button_t::box_state, prod_type[2+(r%3)], scr_coord( D_MARGIN_LEFT+(D_H_SPACE+D_BUTTON_WIDTH)*(1+r%3), offset_below_chart+(D_H_SPACE+D_BUTTON_HEIGHT)*(2+(r/3))));
				prod_ref_line_buttons[r].background_color = ref_color[r];
				prod_ref_line_buttons[r].pressed = false;
				if(
					(r==FAB_REF_MAX_BOOST_ELECTRIC  &&  (factory->get_desc()->is_electricity_producer()  ||  factory->get_desc()->get_electric_boost()==0))  ||
					(r==FAB_REF_MAX_BOOST_PAX  &&  factory->get_desc()->get_pax_boost()==0)  ||
					(r==FAB_REF_MAX_BOOST_MAIL  &&  factory->get_desc()->get_mail_boost()==0)  ||
					(r==FAB_REF_DEMAND_ELECTRIC  &&  (factory->get_desc()->is_electricity_producer()  ||  factory->get_desc()->get_electric_amount()==0))  ||
					(r==FAB_REF_DEMAND_PAX  &&  factory->get_desc()->get_pax_demand()==0)  ||
					(r==FAB_REF_DEMAND_MAIL  &&  factory->get_desc()->get_mail_demand()==0)
					) {
					prod_ref_line_buttons[r].disable();
				}
				else {
					prod_ref_line_buttons[r].add_listener(this);
					prod_cont.add_component( prod_ref_line_buttons + r );
				}
			}
		}
	}
	for(  int i=0;  i<MAX_PROD_LABEL;  ++i  ) {
		if(!i && !factory->get_desc()->is_electricity_producer()){
			prod_labels[i].set_text(label_text[MAX_PROD_LABEL]); // switch MW to KW
		}
		else {
			prod_labels[i].set_text( label_text[i] );
		}
		prod_labels[i].set_pos( scr_coord( D_MARGIN_LEFT+label_pos[i].x*(D_BUTTON_WIDTH+D_H_SPACE), offset_below_chart + label_offset + (D_H_SPACE+D_BUTTON_HEIGHT) * label_pos[i].y ) );
		prod_labels[i].set_width( D_BUTTON_WIDTH );
		prod_cont.add_component( prod_labels + i );
	}
	prod_cont.add_component( &prod_chart );
	tab_panel.add_tab( &prod_cont, translator::translate("Production/Boost") );

	add_component( &tab_panel );
	const int max_rows = max( goods_label_row, label_pos[MAX_PROD_LABEL-1].y+1 );
	const scr_size size( 20+80+CHART_WIDTH+(input_count > 0 ? D_H_SPACE+D_BUTTON_WIDTH : 0 ), D_TAB_HEADER_HEIGHT+CHART_HEIGHT+20+max_rows*D_BUTTON_HEIGHT+(max_rows-1)*D_H_SPACE+16 );
	set_size( size );
	tab_panel.set_size( size );

	// initialize reference lines' data (these do not change over time)
	prod_ref_line_data[FAB_REF_MAX_BOOST_ELECTRIC] = factory->get_desc()->get_electric_boost();
	prod_ref_line_data[FAB_REF_MAX_BOOST_PAX] = factory->get_desc()->get_pax_boost();
	prod_ref_line_data[FAB_REF_MAX_BOOST_MAIL] = factory->get_desc()->get_mail_boost();
}


factory_chart_t::~factory_chart_t()
{
	delete [] goods_buttons;
	delete [] goods_labels;
}


bool factory_chart_t::action_triggered(gui_action_creator_t *comp, value_t)
{
	if(  tab_panel.get_active_tab_index()==0  ) {
		// case : goods statistics' buttons
		for(  int b=0;  b<goods_button_count;  ++b  ) {
			if(  comp==&goods_buttons[b]  ) {
				goods_buttons[b].pressed = !goods_buttons[b].pressed;
				if(  goods_buttons[b].pressed  ) {
					goods_chart.show_curve(b);
				}
				else {
					goods_chart.hide_curve(b);
				}
				return true;
			}
		}
	}
	else {
		// first, check for buttons of other production-related statistics
		for(  int s=0;  s<MAX_FAB_STAT;  ++s  ) {
			if(  comp==&prod_buttons[s]  ) {
				prod_buttons[s].pressed = !prod_buttons[s].pressed;
				if(  prod_buttons[s].pressed  ) {
					prod_chart.show_curve(s);
				}
				else {
					prod_chart.hide_curve(s);
				}
				return true;
			}
		}
		// second, check for buttons of reference lines
		for(  int r=0;  r<MAX_FAB_REF_LINE;  ++r  ) {
			if(  comp==&prod_ref_line_buttons[r]  ) {
				prod_ref_line_buttons[r].pressed = !prod_ref_line_buttons[r].pressed;
				if(  prod_ref_line_buttons[r].pressed  ) {
					prod_chart.show_line(r);
				}
				else {
					prod_chart.hide_line(r);
				}
				return true;
			}
		}
	}
	return false;
}


void factory_chart_t::draw(scr_coord pos)
{
	// update reference lines' data (these might change over time)
	prod_ref_line_data[FAB_REF_DEMAND_ELECTRIC] = ( factory->get_desc()->is_electricity_producer() ? 0 : factory->get_scaled_electric_demand()*1000 );
	prod_ref_line_data[FAB_REF_DEMAND_PAX] = factory->get_monthly_pax_demand();
	prod_ref_line_data[FAB_REF_DEMAND_MAIL] = factory->get_scaled_mail_demand();

	gui_container_t::draw( pos );
}


void factory_chart_t::rdwr( loadsave_t *file )
{
	sint16 tabstate;
	uint32 goods_flag = 0;
	uint32 prod_flag = 0;
	uint32 ref_flag = 0;
	if(  file->is_saving()  ) {
		tabstate = tab_panel.get_active_tab_index();
		for(  int b=0;  b<goods_button_count;  b++  ) {
			goods_flag |= (goods_buttons[b].pressed << b);
		}
		for(  int s=0;  s<MAX_FAB_STAT;  ++s  ) {
			prod_flag |= (prod_buttons[s].pressed << s);
		}
		for(  int r=0;  r<MAX_FAB_REF_LINE;  ++r  ) {
			ref_flag |= (prod_ref_line_buttons[r].pressed << r);
		}
	}

	file->rdwr_short( tabstate );
	file->rdwr_long( goods_flag );
	file->rdwr_long( prod_flag );
	file->rdwr_long( ref_flag );

	if(  file->is_loading()  ) {
		tab_panel.set_active_tab_index( tabstate );
		for(  int b=0;  b<goods_button_count;  b++  ) {
			goods_buttons[b].pressed = (goods_flag >> b)&1;
			if(  goods_buttons[b].pressed  ) {
				goods_chart.show_curve(b);
			}
		}
		for(  int s=0;  s<MAX_FAB_STAT;  ++s  ) {
			prod_buttons[s].pressed = (prod_flag >> s)&1;
			if(  prod_buttons[s].pressed  ) {
				prod_chart.show_curve(s);
			}
		}
		for(  int r=0;  r<MAX_FAB_REF_LINE;  ++r  ) {
			prod_ref_line_buttons[r].pressed = (ref_flag >> r)&1;
			if(  prod_ref_line_buttons[r].pressed  ) {
				prod_chart.show_line(r);
			}
		}
	}
}
