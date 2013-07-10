#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "config.h"
#include "my_math.h"
#include "suncalc.h"

#define MY_UUID {0xB9,0x51,0xEB,0x8C,0x14,0x58,0x4F,0x52,0x94,0x59,0x44,0x17,0xE0,0x5C,0xDE,0xD9}
PBL_APP_INFO(MY_UUID,
             "KP Sun-Moon-Clock", "KarbonPebbler,Boldo,Chad Harp",
             2, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);



Window window;

#if HOUR_VIBRATION
const VibePattern hour_pattern = {
        .durations = (uint32_t []) {200, 100, 200, 100, 200},
        .num_segments = 5
};
#endif
	
TextLayer text_time_layer;
TextLayer text_sunrise_layer;
TextLayer text_sunset_layer;
TextLayer dow_layer;
TextLayer dom_layer;
TextLayer yon_layer;
TextLayer mon_layer;
TextLayer moonLayer; // moon phase
Layer graphics_sun_layer;
//Make fonts global so we can deinit later
GFont font_roboto;
GFont font_moon;

RotBmpPairContainer bitmap_container;
RotBmpPairContainer watchface_container;

GPathInfo sun_path_info = {
  5,
  (GPoint []) {
    {0, 0},
    {-73, +84}, //replaced by sunrise angle
    {-73, +84}, //bottom left
    {+73, +84}, //bottom right
    {+73, +84}, //replaced by sunset angle
  }
};

GPath sun_path;

short currentData = -1;

void graphics_sun_layer_update_callback(Layer *me, GContext* ctx) 
{
  (void)me;

  gpath_init(&sun_path, &sun_path_info);
  gpath_move_to(&sun_path, grect_center_point(&graphics_sun_layer.frame));

  graphics_context_set_fill_color(ctx, GColorBlack);
  gpath_draw_filled(ctx, &sun_path);  
}

float get24HourAngle(int hours, int minutes) 
{
  return (12.0f + hours + (minutes/60.0f)) / 24.0f;
}

void adjustTimezone(float* time) 
{
  *time += TIMEZONE;
  if (*time > 24) *time -= 24;
  if (*time < 0) *time += 24;
}

//return julian day number for time
int tm2jd(PblTm *time)
{
    int y,m;
    y = time->tm_year + 1900;
    m = time->tm_mon + 1;
    return time->tm_mday-32075+1461*(y+4800+(m-14)/12)/4+367*(m-2-(m-14)/12*12)/12-3*((y+4900+(m-14)/12)/100)/4;
}

int moon_phase(int jdn)
{
    double jd;
    jd = jdn-2451550.1;
    jd /= 29.530588853;
    jd -= (int)jd;
    return (int)(jd*27 + 0.5); /* scale fraction from 0-27 and round by adding 0.5 */
}   

// Called once per day
void handle_day(AppContextRef ctx, PebbleTickEvent *t) {

    (void)t;
    (void)ctx;


    static char moon[] = "m";
    int moonphase_number = 0;

    PblTm *time = t->tick_time;
    if(!t)
        get_time(time);

    // date
    //string_format_time(date, sizeof(date), "%m/%d/%Y", time);
    //text_layer_set_text(&dateLayer, date);

    // moon
    moonphase_number = moon_phase(tm2jd(time));
    // correct for southern hemisphere
    if ((moonphase_number > 0) && (LATITUDE < 0))
        moonphase_number = 28 - moonphase_number;
    // select correct font char
    if (moonphase_number == 14)
    {
        moon[0] = (unsigned char)(48);
    } else if (moonphase_number == 0) {
        moon[0] = (unsigned char)(49);
    } else if (moonphase_number < 14) {
        moon[0] = (unsigned char)(moonphase_number+96);
    } else {
        moon[0] = (unsigned char)(moonphase_number+95);
    }
    text_layer_set_text(&moonLayer, moon);


}



void updateDayAndNightInfo()
{
  static char sunrise_text[] = "00:00";
  static char sunset_text[] = "00:00";

  PblTm pblTime;
  get_time(&pblTime);

  if(currentData != pblTime.tm_hour) 
  {
    char *time_format;

    if (clock_is_24h_style()) 
    {
      time_format = "%R";
    } 
    else 
    {
      time_format = "%I:%M";
    }

    float sunriseTime = calcSunRise(pblTime.tm_year, pblTime.tm_mon+1, pblTime.tm_mday, LATITUDE, LONGITUDE, 91.0f);
    float sunsetTime = calcSunSet(pblTime.tm_year, pblTime.tm_mon+1, pblTime.tm_mday, LATITUDE, LONGITUDE, 91.0f);
    adjustTimezone(&sunriseTime);
    adjustTimezone(&sunsetTime);
    
    if (!pblTime.tm_isdst) 
    {
      sunriseTime+=1;
      sunsetTime+=1;
    } 
    
    pblTime.tm_min = (int)(60*(sunriseTime-((int)(sunriseTime))));
    pblTime.tm_hour = (int)sunriseTime;
    string_format_time(sunrise_text, sizeof(sunrise_text), time_format, &pblTime);
    text_layer_set_text(&text_sunrise_layer, sunrise_text);
    
    pblTime.tm_min = (int)(60*(sunsetTime-((int)(sunsetTime))));
    pblTime.tm_hour = (int)sunsetTime;
    string_format_time(sunset_text, sizeof(sunset_text), time_format, &pblTime);
    text_layer_set_text(&text_sunset_layer, sunset_text);
    text_layer_set_text_alignment(&text_sunset_layer, GTextAlignmentRight);

    sunriseTime+=12.0f;
    sun_path_info.points[1].x = (int16_t)(my_sin(sunriseTime/24 * M_PI * 2) * 120);
    sun_path_info.points[1].y = -(int16_t)(my_cos(sunriseTime/24 * M_PI * 2) * 120);

    sunsetTime+=12.0f;
    sun_path_info.points[4].x = (int16_t)(my_sin(sunsetTime/24 * M_PI * 2) * 120);
    sun_path_info.points[4].y = -(int16_t)(my_cos(sunsetTime/24 * M_PI * 2) * 120);
  
    currentData = pblTime.tm_hour;
  }
}

void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) 
{
  (void)t;
  (void)ctx;

  // Need to be static because they're used by the system later.
  static char time_text[] = "00:00";
  static char dom_text[] = "00";
	string_format_time(dom_text, sizeof(dom_text), "%e", t->tick_time);	
  static char dow_text[] = "xxx";
	string_format_time(dow_text, sizeof(dow_text), "%a", t->tick_time);	
  static char yon_text[] = "00";
	string_format_time(yon_text, sizeof(yon_text), "%y", t->tick_time);	
  static char mon_text[] = "xxx";
	string_format_time(mon_text, sizeof(mon_text), "%b", t->tick_time);	

  char *time_format;

  if (clock_is_24h_style()) 
  {
    time_format = "%R";
  } 
  else 
  {
    time_format = "%I:%M";
  }

  string_format_time(time_text, sizeof(time_text), time_format, t->tick_time);

  if (!clock_is_24h_style() && (time_text[0] == '0')) 
  {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(&dow_layer, dow_text);
  text_layer_set_text(&dom_layer, dom_text);
  text_layer_set_text(&yon_layer, yon_text);
  text_layer_set_text(&mon_layer, mon_text);
  
  text_layer_set_text(&text_time_layer, time_text);
  text_layer_set_text_alignment(&text_time_layer, GTextAlignmentCenter);

  rotbmp_pair_layer_set_angle(&bitmap_container.layer, TRIG_MAX_ANGLE * get24HourAngle(t->tick_time->tm_hour, t->tick_time->tm_min));
  bitmap_container.layer.layer.frame.origin.x = (144/2) - (bitmap_container.layer.layer.frame.size.w/2);
  bitmap_container.layer.layer.frame.origin.y = (168/2) - (bitmap_container.layer.layer.frame.size.h/2);

// Vibrate Every Hour
  #if HOUR_VIBRATION
	 
    if ((t->tick_time->tm_min==0) && (t->tick_time->tm_sec==0))
	{
	vibes_enqueue_custom_pattern(hour_pattern);
    }
  #endif
	
  updateDayAndNightInfo();
}


void handle_init(AppContextRef ctx) {
  (void)ctx;
	
  window_init(&window, "KP Sun-Moon-Clock");
  window_stack_push(&window, true /* Animated */);
  window_set_background_color(&window, GColorWhite);

  resource_init_current_app(&APP_RESOURCES);
  font_moon = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_MOON_PHASES_SUBSET_30));
  layer_init(&graphics_sun_layer, window.layer.frame);
  graphics_sun_layer.update_proc = &graphics_sun_layer_update_callback;
  layer_add_child(&window.layer, &graphics_sun_layer);

  rotbmp_pair_init_container(RESOURCE_ID_IMAGE_WATCHFACE_WHITE, RESOURCE_ID_IMAGE_WATCHFACE_BLACK, &watchface_container);
  layer_add_child(&graphics_sun_layer, &watchface_container.layer.layer);
  rotbmp_pair_layer_set_angle(&watchface_container.layer, 1);
  watchface_container.layer.layer.frame.origin.x = (144/2) - (watchface_container.layer.layer.frame.size.w/2);
  watchface_container.layer.layer.frame.origin.y = (168/2) - (watchface_container.layer.layer.frame.size.h/2);

  text_layer_init(&text_time_layer, window.layer.frame);
  text_layer_set_text_color(&text_time_layer, GColorBlack);
  text_layer_set_background_color(&text_time_layer, GColorClear);
  layer_set_frame(&text_time_layer.layer, GRect(0, 35, 144, 30));
  text_layer_set_font(&text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_30)));
  layer_add_child(&window.layer, &text_time_layer.layer);

  rotbmp_pair_init_container(RESOURCE_ID_IMAGE_HOUR_WHITE, RESOURCE_ID_IMAGE_HOUR_BLACK, &bitmap_container);
  rotbmp_pair_layer_set_src_ic(&bitmap_container.layer, GPoint(9,56));
  layer_add_child(&window.layer, &bitmap_container.layer.layer);

  text_layer_init(&moonLayer, GRect(0, 100, 144 /* width */, 168-115 /* height */));
  text_layer_set_text_color(&moonLayer, GColorWhite);
  text_layer_set_background_color(&moonLayer, GColorClear);
  text_layer_set_font(&moonLayer, font_moon);
  text_layer_set_text_alignment(&moonLayer, GTextAlignmentCenter);

  handle_day(ctx, NULL);
	
  layer_add_child(&window.layer, &moonLayer.layer);
	
PblTm t;
  get_time(&t);
  rotbmp_pair_layer_set_angle(&bitmap_container.layer, TRIG_MAX_ANGLE * get24HourAngle(t.tm_hour, t.tm_min));
  bitmap_container.layer.layer.frame.origin.x = (144/2) - (bitmap_container.layer.layer.frame.size.w/2);
  bitmap_container.layer.layer.frame.origin.y = (168/2) - (bitmap_container.layer.layer.frame.size.h/2);

  //Day of Week text
  text_layer_init(&dow_layer, GRect(0, 0, 144, 127+26));
  text_layer_set_text_color(&dow_layer, GColorWhite);
  text_layer_set_background_color(&dow_layer, GColorClear);
  text_layer_set_font(&dow_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(&dow_layer, GTextAlignmentLeft);
  text_layer_set_text(&dow_layer, "xxx");
  layer_add_child(&window.layer, &dow_layer.layer);

  //Day of the Month text
  text_layer_init(&dom_layer, GRect(0, 10, 144, 137+26));
  text_layer_set_text_color(&dom_layer, GColorWhite);
  text_layer_set_background_color(&dom_layer, GColorClear);
  text_layer_set_font(&dom_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(&dom_layer, GTextAlignmentLeft);
  text_layer_set_text(&dom_layer, "00");
  layer_add_child(&window.layer, &dom_layer.layer);
	
  //Month Text
  text_layer_init(&mon_layer, GRect(0, 0, 144, 127+26));
  text_layer_set_text_color(&mon_layer, GColorWhite);
  text_layer_set_background_color(&mon_layer, GColorClear);
  text_layer_set_font(&mon_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(&mon_layer, GTextAlignmentRight);
  text_layer_set_text(&mon_layer, "xxx");
  layer_add_child(&window.layer, &mon_layer.layer);

  //Year Number text
  text_layer_init(&yon_layer, GRect(0, 10, 144, 137+26));
  text_layer_set_text_color(&yon_layer, GColorWhite);
  text_layer_set_background_color(&yon_layer, GColorClear);
  text_layer_set_font(&yon_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(&yon_layer, GTextAlignmentRight);
  text_layer_set_text(&yon_layer, "00");
  layer_add_child(&window.layer, &yon_layer.layer); 	

  //Sunrise Text 
  text_layer_init(&text_sunrise_layer, window.layer.frame);
  text_layer_set_text_color(&text_sunrise_layer, GColorWhite);
  text_layer_set_background_color(&text_sunrise_layer, GColorClear);
  layer_set_frame(&text_sunrise_layer.layer, GRect(0, 145, 144, 30));
  text_layer_set_font(&text_sunrise_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(&window.layer, &text_sunrise_layer.layer);

 //Sunset Text
  text_layer_init(&text_sunset_layer, window.layer.frame);
  text_layer_set_text_color(&text_sunset_layer, GColorWhite);
  text_layer_set_background_color(&text_sunset_layer, GColorClear);
  layer_set_frame(&text_sunset_layer.layer, GRect(0, 145, 144, 30));
  text_layer_set_font(&text_sunset_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(&window.layer, &text_sunset_layer.layer); 

  updateDayAndNightInfo();
}

void handle_deinit(AppContextRef ctx) {
  (void)ctx;

  rotbmp_pair_deinit_container(&watchface_container);
  rotbmp_pair_deinit_container(&bitmap_container);
  fonts_unload_custom_font(font_moon);
}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,

    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    }
  };
  app_event_loop(params, &handlers);
}
