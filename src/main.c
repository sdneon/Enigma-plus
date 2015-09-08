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
//#define DEBUG_MODE 1

//masks for vibes:
#define MASKV_BTDC   0x20000
#define MASKV_HOURLY 0x10000
#define MASKV_FROM   0xFF00
#define MASKV_TO     0x00FF
//def: disabled, 10am to 8pm
#define DEF_VIBES    0x0A14
#define TXT_LAYER_DAY_OF_WEEK 4
#define BATTERY_LVL_COL 2
#define DEF_USE_MMDD false
#define DEF_DUAL_TIME false
#define DEF_TIMEZONE2 65

#define A_DAY_IN_MINS (24 * 60)

const float TIMEZONE_OFFSETS[] = {
 0, -12, -11, -10,  -9,  -8,  -8,  -7,  -7,  -7,
-6,  -6,  -6,  -6,  -5,  -5,  -5,  -4,  -4,  -4,
-4,-3.5,  -3,  -3,  -3,  -3,  -2,  -1,  -1,   0,
0,    1,   1,   1,   1,   1,   2,   2,   2,   2,
2,    2,   2,   2,   2,   3,   3,   3,   3, 3.5,
4,    4,   4, 4.5,   5,   5, 5.5, 5.5,5.75,   6,
6,  6.5,   7,   7,   8,   8,   8,   8,   8,   9,
9,    9, 9.5, 9.5,  10,  10,  10,  10,  10,  11,
12,  12,  13};

Window *my_window;
Layer *time_box_layer, *time_box_layer2, *battery_level_layer;
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
static bool m_bUseMMDD = DEF_USE_MMDD;
static bool m_bDualTime = DEF_DUAL_TIME;
static int m_nTimezone2 = DEF_TIMEZONE2;
//time info for dual time:
int m_nHours2nd = 0, m_nMins2nd = 0;
bool m_bisAm2nd = false;
// Vibe pattern for loss of BT connection: ON for 400ms, OFF for 100ms, ON for 300ms, OFF 100ms, 100ms:
static const uint32_t const VIBE_SEG_BT_LOSS[] = { 400, 200, 200, 400, 100 };
static const VibePattern VIBE_PAT_BT_LOSS = {
  .durations = VIBE_SEG_BT_LOSS,
  .num_segments = ARRAY_LENGTH(VIBE_SEG_BT_LOSS),
};

//forward declaration
void display_time(struct tm* tick_time);
void fill_digits(int i);

//
//Configuration stuff via AppMessage API
//
#define KEY_VIBES 0
#define KEY_ROW2  1
#define KEY_ROW4  2
#define KEY_TIMEZONE2 3

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    // Get the first pair
    Tuple *t = dict_read_first(iterator);
    int nNewValue;
    bool bNewValue, bNeedUpdate = false;

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
            case KEY_ROW2:
                bNewValue = !!t->value->int32;
                if (m_bUseMMDD != bNewValue)
                {
                    m_bUseMMDD = bNewValue;
                    bNeedUpdate = true;
                }
                break;
            case KEY_ROW4:
                bNewValue = !!t->value->int32;
                if (m_bDualTime != bNewValue)
                {
                    m_bDualTime = bNewValue;
                    bNeedUpdate = true;
                }
                break;
            case KEY_TIMEZONE2:
                nNewValue = t->value->int32;
                if (m_nTimezone2 != nNewValue)
                {
                    m_nTimezone2 = nNewValue;
                    bNeedUpdate = true;
                }
                break;
        }

        // Get next pair, if any
        t = dict_read_next(iterator);
    }
    if (bNeedUpdate)
    {
        time_t now = time(NULL);
        struct tm *tick_time = localtime(&now);
        display_time(tick_time);
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

void time_box_layer_update_proc2(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    GRect r = GRect(bounds.origin.x, center.y + 24, bounds.size.w, 48);

    graphics_context_set_stroke_color(ctx, !m_bDualTime?
        GColorBlack: (m_bisAm2nd? GColorRed: GColorBlue));
    graphics_context_set_fill_color(ctx, !m_bDualTime?
        GColorBlack: (m_bisAm2nd? GColorRed: GColorBlue));
    graphics_fill_rect(ctx, r, 0, GCornerNone);
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
#ifdef DEBUG_MODE
    h = m;
    m = tick_time->tm_sec;
    if ((m%5) != 0) return; //update every 5secs to allow time for animation
#endif

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
#ifndef DEBUG_MODE
    if (m == 0)
#else
    if ((m%15) == 0) //every 15 secs
#endif
    {
        //reset digits hourly
        for (int i = 0; i < 4; ++i)
        {
            //fill_order(i);
            //fill_offsets(i);
            fill_digits(i);
        }
    }

    m_bIsAm = (h < 12);

    // If watch is in 12hour mode
    if (!clock_is_24h_style()) {
        if (h == 0) { //Midnight to 1am
            h = 12;
        } else if (h > 12) { //1pm to 11:59pm
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
    int ddmm1, ddmm2, ddmm3, ddmm4,
        year1, year2, year3, year4;
    if (!m_bUseMMDD)
    {   //DDMM format:
        ddmm1 = dayOfMth / 10;
        ddmm2 = dayOfMth % 10;
        ddmm3 = mthsSinceJan / 10;
        ddmm4 = mthsSinceJan % 10;
    }
    else
    {   //MMDD format:
        ddmm3 = dayOfMth / 10;
        ddmm4 = dayOfMth % 10;
        ddmm1 = mthsSinceJan / 10;
        ddmm2 = mthsSinceJan % 10;
    }
    if (!m_bDualTime)
    {
        year1 = year / 1000;
        year2 = (year % 1000) / 100;
        year3 = (year % 100) / 10;
        year4 = year % 10;
    }
    else
    {
        time_t timeUtc = time(NULL);
        struct tm *time2nd = gmtime(&timeUtc);
        int minsTotal2nd = time2nd->tm_hour * 60 + time2nd->tm_min;
#ifdef DEBUG_MODE
        minsTotal2nd = time2nd->tm_min * 60 + time2nd->tm_sec;
#endif
        minsTotal2nd += TIMEZONE_OFFSETS[m_nTimezone2] * 60;
        //ensure minsTotal2nd remains within valid range:
        if (minsTotal2nd < 0)
        {
            minsTotal2nd += A_DAY_IN_MINS;
        }
        else if (minsTotal2nd >= A_DAY_IN_MINS)
        {
            minsTotal2nd -= A_DAY_IN_MINS;
        }
        m_nHours2nd = minsTotal2nd / 60;
        m_nMins2nd = minsTotal2nd % 60;
        m_bisAm2nd = m_nHours2nd < 12;
        if (!clock_is_24h_style())
        {
            if (m_nHours2nd == 0) { //Midnight to 1am
                m_nHours2nd = 12;
            } else if (m_nHours2nd > 12) { //1pm to 11:59pm
                m_nHours2nd -= 12;
            }
        }
        year1 = m_nHours2nd/10;
        year2 = m_nHours2nd%10;
        year3 = m_nMins2nd/10;
        year4 = m_nMins2nd%10;
    }
    change_digits(0, h/10, ddmm1, year1);
    change_digits(1, h%10, ddmm2, year2);
    change_digits(2, m/10, ddmm3, year3);
    change_digits(3, m%10, ddmm4, year4);

    //show day of week in row above ddmm (i.e. 1st row):
    changeDayOfWeek(tick_time);

/*
//TRY gmtime():
    #define MST (-7)
    #define UTC (0)
    #define CCT (+8)
    time_t rawtime;
    struct tm * ptm;
    rawtime = time(NULL);
    ptm = gmtime(&rawtime);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Phoenix, AZ (U.S.) :  %2d:%02d\n", (ptm->tm_hour+MST)%24, ptm->tm_min);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Reykjavik (Iceland) : %2d:%02d\n", (ptm->tm_hour+UTC)%24, ptm->tm_min);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Beijing (China) :     %2d:%02d\n", (ptm->tm_hour+CCT)%24, ptm->tm_min);
*/
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
    if (persist_exists(KEY_ROW2))
    {
        m_bUseMMDD = persist_read_bool(KEY_ROW2);
    }
    if (persist_exists(KEY_ROW4))
    {
        m_bDualTime = persist_read_bool(KEY_ROW4);
    }
    if (persist_exists(KEY_TIMEZONE2))
    {
        m_nTimezone2 = persist_read_int(KEY_TIMEZONE2);
    }
}

void saveConfig()
{
    persist_write_int(KEY_VIBES, m_nVibes);
    persist_write_bool(KEY_ROW2, m_bUseMMDD);
    persist_write_bool(KEY_ROW4, m_bDualTime);
    persist_write_int(KEY_TIMEZONE2, m_nTimezone2);
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
    //Add coloured time row layer for dual time
    time_box_layer2 = layer_create(frame);
    layer_set_update_proc(time_box_layer2, time_box_layer_update_proc2);
    layer_add_child(root_layer, time_box_layer2);

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

#ifdef DEBUG_MODE
    tick_timer_service_subscribe(SECOND_UNIT, &handle_minute_tick);
#else
    tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick);
#endif

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
    layer_destroy(time_box_layer2);
    layer_destroy(battery_level_layer);
    window_destroy(my_window);
}

int main(void) {
    handle_init();
    app_event_loop();
    handle_deinit();
}

