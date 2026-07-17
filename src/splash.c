#include "splash.h"
#include <lvgl_zephyr.h>

extern const lv_image_dsc_t img_image1;
extern const lv_image_dsc_t img_image2;

static lv_obj_t *splash1_screen;
static lv_obj_t *splash2_screen;

void splash_show_image1(void)
{
    lvgl_lock();

    splash1_screen = lv_obj_create(NULL);

    lv_obj_t *img = lv_image_create(splash1_screen);
    lv_image_set_src(img, &img_image1);
    lv_obj_center(img);

    lv_screen_load(splash1_screen);

    lvgl_unlock();
}

void splash_show_image2(void)
{
    lvgl_lock();

    splash2_screen = lv_obj_create(NULL);

    lv_obj_t *img = lv_image_create(splash2_screen);
    lv_image_set_src(img, &img_image2);
    lv_obj_center(img);

    lv_screen_load(splash2_screen);

    lv_obj_delete(splash1_screen);
    splash1_screen = NULL;

    lvgl_unlock();
}

void splash_cleanup(void)
{
    lvgl_lock();

    lv_obj_delete(splash2_screen);
    splash2_screen = NULL;

    lvgl_unlock();
}
