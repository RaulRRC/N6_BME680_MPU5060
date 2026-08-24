#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    _SCREEN_ID_LAST = 1
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *chart_ax;
    lv_obj_t *chart_ay;
    lv_obj_t *chart_az;
    lv_obj_t *chart_gx;
    lv_obj_t *chart_gy;
    lv_obj_t *chart_gz;
    lv_obj_t *preasure_bar;
    lv_obj_t *humid_bar;
    lv_obj_t *humidity_label;
    lv_obj_t *pressure_label;
    lv_obj_t *temperature_label;
    lv_obj_t *temp_gauge;
    lv_obj_t *indicator_line;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/