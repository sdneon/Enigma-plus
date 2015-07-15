/**
 * Enigma watch face variant.
 * Modified by @neon from @chadheim's old black & white example:
 *   https://github.com/chadheim/pebble-watchface-slider
 *
 * Changes:
 * > Coloured time bar:
 *   > red for AM, blue for PM.
 * > Added rows for: year, DDMM, 'day of week'.
 * > 'day of week' mini-bar (top-left) doubles as 'bluetooth status' indicator.
 *   > Coloured: connected; dark: disconnected.
 * > Added battery-level indicator: it's the 3rd column.
 *   > Top down orange bar: charging; bottom-up bar: charge remaining.
 **/
#include <pebble.h>

//DEBUG flags
//#define DISABLE_CONFIG


//masks for vibes:
#define MASKV_BTDC   0x20000
#define MASKV_HOURLY 0x10000
#define MASKV_FROM   0xFF00
#define MASKV_TO     0x00FF
//def: disabled, 10am to 8pm
#define DEF_VIBES    0x0A14
#define TXT_LAYER_DAY_OF_WEEK 4
#define BATTERY_LVL_COL 2

Window *my_window;
Layer *time_box_layer, *battery_level_layer;
TextLayer *text_layer[5];
PropertyAnimation *animations[5] = {0};
GRect to_rect[6];
int center;

char digits[4][32], digitsBkup[4][32];
int offsets[4][10];
int order[4][10];
bool m_bIsAm = false;
static char s_dayOfWeek_buffer[4];

static int m_nVibes = DEF_VIBES;
// Vibe pattern for loss of BT connection: ON for 400ms, OFF for 100ms, ON for 300ms, OFF 100ms, 100ms:
static const uint32_t const VIBE_SEG_BT_LOSS[] = { 400, 200, 200, 400, 100 };
static const VibePattern VIBE_PAT_BT_LOSS = {
  .durations = VIBE_SEG_BT_LOSS,
  .num_segments = ARRAY_LENGTH(VIBE_SEG_BT_LOSS),
};

//
//Configuration stuff via AppMessage API
//
#define KEY_VIBES 0

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    // Get the first pair
    Tuple *t = dict_read_first(iterator);
    int nNewValue, i;

    // Process all pairs present
    while(t != NULL) {
        // Process this pair's key
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "Key:%d received with value:%d", (int)t->key, (int)t->value->int32);
        switch (t->key) {
            case KEY_VIBES:
                nNewValue = t->value->int32;
                if (m_nVibes != nNewValue)
                {
                    m_nVibes = nNewValue;
                }
                break;
        }

        // Get next pair, if any
        t = dict_read_next(iterator);
    }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}


//
//Bluetooth stuff
//
bool m_bBtConnected = false;
static void bt_handler(bool connected) {
    if (!connected && m_bBtConnected //vibrate once upon BT connection lost
        && (m_nVibes & MASKV_BTDC)) //only if option enabled
    {
        vibes_enqueue_custom_pattern(VIBE_PAT_BT_LOSS);
    }
    m_bBtConnected = connected;
    TextLayer *layer = text_layer[TXT_LAYER_DAY_OF_WEEK];
    if (layer)
    {
        text_layer_set_background_color(layer, m_bBtConnected? (m_bIsAm? GColorRed: GColorBlue): GColorBlack);
    }
}

//
// Battery stuff
//
BatteryChargeState m_sBattState = {
    0,      //charge_percent
    false,  //is_charging
    false   //is_plugged
};
void update_battery_level_display()
{
    if (!battery_level_layer) return;
    GRect bounds = layer_get_frame(battery_level_layer);
    int third = (bounds.size.h - 4) / 3; //height of screen
    //int y = third * 33 / 100; //DEBUG
    int y = third * m_sBattState.charge_percent / 100;
    to_rect[5] = layer_get_frame(battery_level_layer);
    to_rect[4] = layer_get_frame(battery_level_layer);
    if (m_sBattState.is_charging)
    {
        to_rect[5].origin.y = -(2 + 3*third); //from
        to_rect[4].origin.y = -(2 + 3*third) + y;
        //Note: charge_percent seems incorrect when charging! (e.g. shows 10% instead of 60% actual capacity).
    }
    else
    {
        to_rect[5].origin.y = -2; //from
        to_rect[4].origin.y = -2 - y;
    }

    if (animations[4])
    {
        property_animation_destroy(animations[4]);
        animations[4] = NULL;
    }

    animations[4] = property_animation_create_layer_frame(battery_level_layer,
        &to_rect[5], &to_rect[4]);
    animation_set_duration((Animation*) animations[4], 1500);
    animation_schedule((Animation*) animations[4]);
}

static void battery_handler(BatteryChargeState new_state) {
    m_sBattState = new_state;
    if (battery_level_layer)
    {
        //layer_mark_dirty(battery_level_layer);
        update_battery_level_display();
    }
}

void battery_level_layer_update_proc(Layer *layer, GContext *ctx) {
    if (!my_window) return;
    GRect bounds = layer_get_bounds(layer);
    bounds = layer_get_frame(layer);

    GRect r = GRect(0, 0, bounds.size.w, bounds.size.h);
    graphics_context_set_stroke_color(ctx, GColorOrange);
    graphics_draw_rect(ctx, r);
    graphics_draw_rect(ctx, grect_crop(r, 1));

    //battery level bar is coloured in the middle third (with 2 pixels top & bottom): ||   ===   ||
    int third = (bounds.size.h - 4) / 3; //height of screen
//    r = GRect(0, 0.5 * third + 2, bounds.size.w, 1.5 * third + 2);
    r = GRect(0, third + 2, bounds.size.w, 2 * third + 2);
    graphics_context_set_fill_color(ctx, GColorOrange);
    graphics_fill_rect(ctx, r, 0, GCornerNone);
}

//
// Enigma display stuff
//
void time_box_layer_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    GRect r = GRect(bounds.origin.x, center.y - 24, bounds.size.w, 48);

    graphics_context_set_stroke_color(ctx, m_bIsAm? GColorRed: GColorBlue);
    graphics_context_set_fill_color(ctx, m_bIsAm? GColorRed: GColorBlue);
//    graphics_draw_rect(ctx, r);
    graphics_fill_rect(ctx, r, 0, GCornerNone);
//    graphics_draw_rect(ctx, grect_crop(r, 1));
}

//overwrites 'digits' with our desired 'ddmm' or 'year' digit (num)
void change_digit(int col, int ref, int num, int off)
{
    if (num <= 10)
    {
        num += '0';
    }
    int p = (order[col][ref] * 2) + 4 + off;
    digits[col][p] = num;
    if (p < 8) {
    p += 20;
    digits[col][p] = num;
    } else if(p >= 20) {
        p -= 20;
        digits[col][p] = num;
    }
}

/**
 * Change the digit in given column, and animate the column by rolling
 * the newly selected digit into position (3rd row).
 **/
void set_digit(int col, int num) {
    Layer *layer = text_layer_get_layer(text_layer[col]);

    to_rect[col] = layer_get_frame(layer);
    to_rect[col].origin.y = (offsets[col][num] * -42) + center - 28;

    if (animations[col])
    {
        property_animation_destroy(animations[col]);
        animations[col] = NULL;
    }

    animations[col] = property_animation_create_layer_frame(layer, NULL, &to_rect[col]);
    animation_set_duration((Animation*) animations[col], 1000);
    animation_schedule((Animation*) animations[col]);
}

/**
 * Change the digits in given column.
 * @param col column to change.
 * @param numTime one of the hhmm (time) digits.
 * @param numTop one of the ddmm (date) digits.
 * @param numBtm one of the year digits.
 **/
void change_digits(int col, int numTime, int numTop, int numBtm)
{
    change_digit(col, numTime, numTop, -2);
    change_digit(col, numTime, numBtm, 2);
    set_digit(col, numTime);
}

/**
 * Convert string to uppercase.
 * @param a_pchStr string to be converted.
 * @param a_nMaxLen size of string.
 **/
void toUpperCase(char *a_pchStr, int a_nMaxLen)
{
    for (int i = 0; (i < a_nMaxLen) && (a_pchStr[i] != 0); ++i)
    {
        if ((a_pchStr[i] >= 'a') && (a_pchStr[i] <= 'z'))
        {
            a_pchStr[i] -= 32;
        }
    }
}

/**
 * Changes the 3-letters representation of 'day of week' (e.g. SUN).
 * Its background colour acts as 'bluetooth connectivity' indication:
 *   Coloured (AM/PM colour): connected; dark (black): disconnected.
 * TODO: Maybe change to 200% height half-coloured bar for animation.
 *
 * @param daysSinceSun day of week (0: Sun, etc).
 **/
void changeDayOfWeek(struct tm* tick_time)
{
    //write abbreviated 'day of week' (weekday name) according to the current locale:
    strftime(s_dayOfWeek_buffer, sizeof(s_dayOfWeek_buffer), "%a", tick_time);
    toUpperCase(s_dayOfWeek_buffer, sizeof(s_dayOfWeek_buffer));

    TextLayer *layer = text_layer[TXT_LAYER_DAY_OF_WEEK];
    text_layer_set_text(layer, s_dayOfWeek_buffer);
    //text_layer_set_background_color(layer, m_bIsAm? GColorRed: GColorBlue);
    //text_layer_set_background_color(layer, GColorBlack);
    text_layer_set_background_color(layer, m_bBtConnected? (m_bIsAm? GColorRed: GColorBlue): GColorBlack);

//    to_rect[TXT_LAYER_DAY_OF_WEEK] = layer_get_frame((Layer*)layer);
//    to_rect[TXT_LAYER_DAY_OF_WEEK].origin.y = 0;
//
//    if (animations[TXT_LAYER_DAY_OF_WEEK])
//        property_animation_destroy(animations[TXT_LAYER_DAY_OF_WEEK]);
//
//    animations[TXT_LAYER_DAY_OF_WEEK] = property_animation_create_layer_frame((Layer*)layer, NULL, &to_rect[TXT_LAYER_DAY_OF_WEEK]);
//    animation_set_duration((Animation*) animations[TXT_LAYER_DAY_OF_WEEK], 1000);
//    animation_schedule((Animation*) animations[TXT_LAYER_DAY_OF_WEEK]);
}

void display_time(struct tm* tick_time) {
    int h = tick_time->tm_hour;
    int m = tick_time->tm_min;

    if ((m_nVibes & MASKV_HOURLY) //option enabled to vibrate hourly
        && (m == 0)) //hourly mark reached
    {
        int from = (m_nVibes & MASKV_FROM) >> 8,
            to = m_nVibes & MASKV_TO;
        bool bShake = false;
        if (from <= to)
        {
            bShake = (h >= from) && (h <= to);
        }
        else
        {
            bShake = (h >= from) || (h <= to);
        }
        if (bShake)
        {
            vibes_double_pulse();
        }
    }

    m_bIsAm = (h < 12);

    // If watch is in 12hour mode
    if(!clock_is_24h_style()) {
        if(h == 0) { //Midnight to 1am
            h = 12;
        } else if(h > 12) { //1pm to 11:59pm
            h -= 12;
        }
    }

    //restore digits:
    memcpy(digits, digitsBkup, (4*32));
    /**
     * Change 3 rows as follows:
     *   2nd row: ddmm (date less year)
     *   3rd row: hhmm (time)
     *   4th row: year
     **/
    int year = tick_time->tm_year + 1900;
    int mthsSinceJan = tick_time->tm_mon + 1; //base 0
    int dayOfMth = tick_time->tm_mday; //base 1
    change_digits(0, h/10, dayOfMth / 10, year / 1000);
    change_digits(1, h%10, dayOfMth % 10, (year % 1000) / 100);
    change_digits(2, m/10, mthsSinceJan / 10, (year % 100) / 10);
    change_digits(3, m%10, mthsSinceJan % 10, year % 10);

    //show day of week in row above ddmm (i.e. 1st row):
    changeDayOfWeek(tick_time);
}

void handle_minute_tick(struct tm* tick_time, TimeUnits units_changed) {
    display_time(tick_time);
}

/**
 * digits comprises 4 cols of 14 digits each.
 * In each column, digits in 1st 4 rows are the same as those in last 4 rows.
 **/
void fill_digits(int i) {
    for (int n=0; n<10; ++n) {
        int p = (order[i][n] * 2) + 4;
        digits[i][p] = '0' + n;
        digits[i][p+1] = '\n';

        //make digits in 1st 4 rows same as those in last 4 rows
        if (p < 8) {
            p += 20;
            digits[i][p] = '0' + n;
            digits[i][p+1] = '\n';
        } else if(p >= 20) {
            p -= 20;
            digits[i][p] = '0' + n;
            digits[i][p+1] = '\n';
        }
    }

    digits[i][28] = '\0';
}

void fill_offsets(int i) {
    for(int n=0; n<10; ++n) {
        offsets[i][n] = order[i][n] + 2;
    }
}

//fill given column i with digits 0 - 9 at random locations
void fill_order(int i) {
    //fill col with digits 0-9
    for(int n=0; n<10; ++n) {
        order[i][n] = n;
    }

    //randomly swap digits' locations
    for(int n=0; n<10; ++n) {
        int k = rand() % 10;
        if(n != k) {
            int tmp = order[i][k];
            order[i][k] = order[i][n];
            order[i][n] = tmp;
        }
    }
}

void readConfig()
{
    if (persist_exists(KEY_VIBES))
    {
        m_nVibes = persist_read_int(KEY_VIBES);
    }
}

void saveConfig()
{
    persist_write_int(KEY_VIBES, m_nVibes);
}

//
//Window setup sutff
//
void handle_init(void)
{
#ifndef DISABLE_CONFIG
    readConfig();
    // Register callbacks
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    // Open AppMessage
    app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
#endif

    my_window = window_create();
    window_stack_push(my_window, true);
    window_set_background_color(my_window, GColorBlack);

    s_dayOfWeek_buffer[0] = 0;

    Layer *root_layer = window_get_root_layer(my_window);
    GRect frame = layer_get_frame(root_layer);

    //Add coloured time row layer 1st so that it is below digits (text) layers:
    time_box_layer = layer_create(frame);
    layer_set_update_proc(time_box_layer, time_box_layer_update_proc);
    layer_add_child(root_layer, time_box_layer);

    //Add coloured battery-level column layer 1st so that it is below digits (text) layers:
    battery_level_layer = layer_create(GRect(BATTERY_LVL_COL*frame.size.w/4, -2, frame.size.w/4, frame.size.h*3 + 4));
    layer_set_update_proc(battery_level_layer, battery_level_layer_update_proc);
    layer_add_child(root_layer, battery_level_layer);

    center = frame.size.h/2;

    srand(time(NULL));

    for (int i = 0; i < 4; ++i)
    {
        fill_order(i);
        fill_offsets(i);
        fill_digits(i);
        //backup digits:
        memcpy(digitsBkup, digits, (4*32));

        text_layer[i] = text_layer_create(GRect(i*frame.size.w/4, 0, frame.size.w/4, 800));
        text_layer_set_text_color(text_layer[i], GColorWhite);
        text_layer_set_background_color(text_layer[i], GColorClear);
        //text_layer_set_background_color(text_layer[i],
        //  (i != 2)? GColorClear:
        //  m_bIsAm? GColorRed: GColorBlue);
        text_layer_set_font(text_layer[i],
            //fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
            fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS)); //Can't use, as numbers only
        text_layer_set_text(text_layer[i], &digits[i][0]);
        text_layer_set_text_alignment(text_layer[i], GTextAlignmentCenter);
        layer_add_child(root_layer, text_layer_get_layer(text_layer[i]));
    }
    //Example: view live log using "pebble logs --phone=192.168.1.X" command
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "digits0 %s", digits[0]);

    //Add 'day of week' top row layer above 'digits' layers:
    text_layer[TXT_LAYER_DAY_OF_WEEK] = text_layer_create(GRect(0, -8, frame.size.w / 2 + 6, 32));
    //text_layer[TXT_LAYER_DAY_OF_WEEK] = text_layer_create(GRect(6, -8, frame.size.w / 2 - 2, /*32*/64));
    //layer_set_bounds((Layer*)text_layer[TXT_LAYER_DAY_OF_WEEK], GRect(6, -8, frame.size.w / 2 - 2, 10));
    text_layer_set_text_color(text_layer[TXT_LAYER_DAY_OF_WEEK], GColorWhite);
    text_layer_set_background_color(text_layer[TXT_LAYER_DAY_OF_WEEK], GColorClear);
    text_layer_set_font(text_layer[TXT_LAYER_DAY_OF_WEEK],
        fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
    text_layer_set_text(text_layer[TXT_LAYER_DAY_OF_WEEK], s_dayOfWeek_buffer);
    text_layer_set_text_alignment(text_layer[TXT_LAYER_DAY_OF_WEEK], GTextAlignmentLeft);
    //text_layer_set_overflow_mode(text_layer[TXT_LAYER_DAY_OF_WEEK], GTextOverflowModeWordWrap);
    layer_add_child(root_layer, text_layer_get_layer(text_layer[TXT_LAYER_DAY_OF_WEEK]));

    time_t now = time(NULL);
    struct tm *tick_time = localtime(&now);
    display_time(tick_time);

    tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick);

    // Subscribe to Bluetooth updates
    bluetooth_connection_service_subscribe(bt_handler);
    // Show current connection state
    bt_handler(bluetooth_connection_service_peek());

    // Subscribe to the Battery State Service
    battery_state_service_subscribe(battery_handler);
    // Get the current battery level
    battery_handler(battery_state_service_peek());
}

void handle_deinit(void)
{
    bluetooth_connection_service_unsubscribe();
    battery_state_service_unsubscribe();
    tick_timer_service_unsubscribe();

#ifndef DISABLE_CONFIG
    app_message_deregister_callbacks();
    saveConfig();
#endif

    for (int i = 0; i < 5; ++i)
    {
        text_layer_destroy(text_layer[i]);
        if (animations[i])
            property_animation_destroy(animations[i]);
    }
    layer_destroy(time_box_layer);
    layer_destroy(battery_level_layer);
    window_destroy(my_window);
}

int main(void) {
    handle_init();
    app_event_loop();
    handle_deinit();
}

