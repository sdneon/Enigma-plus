#include <pebble.h>

Window *my_window;
Layer *box_layer;
TextLayer *text_layer[4];
PropertyAnimation *animations[4] = {0};
GRect to_rect[4];
int center;

char digits[4][32];
int offsets[4][10];
int order[4][10];

void box_layer_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint center = grect_center_point(&bounds);
    GRect r = GRect(bounds.origin.x, center.y - 24, bounds.size.w, 48);
    
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_rect(ctx, r);
    graphics_draw_rect(ctx, grect_crop(r, 1));
}

void set_digit(int col, int num) {
    Layer *layer = text_layer_get_layer(text_layer[col]);
    
    to_rect[col] = layer_get_frame(layer);
    to_rect[col].origin.y = (offsets[col][num] * -42) + center - 28;
    
    if(animations[col])
        property_animation_destroy(animations[col]);
    
    animations[col] = property_animation_create_layer_frame(layer, NULL, &to_rect[col]);
    animation_set_duration((Animation*) animations[col], 1000);
    animation_schedule((Animation*) animations[col]);
}

void display_time(struct tm* tick_time) {
    int h = tick_time->tm_hour;
    int m = tick_time->tm_min;
    
    // If watch is in 12hour mode
    if(!clock_is_24h_style()) {
        if(h == 0) { //Midnight to 1am
            h = 12;
        } else if(h > 12) { //1pm to 11:59pm
            h -= 12;
        }
    }
    
    set_digit(0, h/10);
    set_digit(1, h%10);
    set_digit(2, m/10);
    set_digit(3, m%10);
}

void handle_minute_tick(struct tm* tick_time, TimeUnits units_changed) {
    display_time(tick_time);
}

void fill_digits(int i) {
    for(int n=0; n<10; ++n) {
        int p = (order[i][n] * 2) + 4;
        digits[i][p] = '0' + n;
        digits[i][p+1] = '\n';
        
        if(p < 8) {
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

void fill_order(int i) {
    for(int n=0; n<10; ++n) {
        order[i][n] = n;
    }
    
    for(int n=0; n<10; ++n) {
        int k = rand() % 10;
        if(n != k) {
            int tmp = order[i][k];
            order[i][k] = order[i][n];
            order[i][n] = tmp;
        }
    }
}

void handle_init(void) {
    my_window = window_create();
    window_stack_push(my_window, true);
    window_set_background_color(my_window, GColorBlack);

    Layer *root_layer = window_get_root_layer(my_window);
    GRect frame = layer_get_frame(root_layer);
    
    center = frame.size.h/2;
    
    srand(time(NULL));
    
    for(int i=0; i<4; ++i) {
        fill_order(i);
        fill_offsets(i);
        fill_digits(i);
        
        text_layer[i] = text_layer_create(GRect(i*frame.size.w/4, 0, frame.size.w/4, 800));
        text_layer_set_text_color(text_layer[i], GColorWhite);
        text_layer_set_background_color(text_layer[i], GColorClear);
        text_layer_set_font(text_layer[i], fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
        text_layer_set_text(text_layer[i], &digits[i][0]);
        text_layer_set_text_alignment(text_layer[i], GTextAlignmentCenter);
        layer_add_child(root_layer, text_layer_get_layer(text_layer[i]));
    }
    
    box_layer = layer_create(frame);
    layer_set_update_proc(box_layer, box_layer_update_proc);
    layer_add_child(root_layer, box_layer);
    
    time_t now = time(NULL);
    struct tm *tick_time = localtime(&now);
    display_time(tick_time);
    
    tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick);
}

void handle_deinit(void) {
    for(int i=0; i<4; ++i)
        text_layer_destroy(text_layer[i]);
    window_destroy(my_window);
}

int main(void) {
    handle_init();
    app_event_loop();
    handle_deinit();
}
