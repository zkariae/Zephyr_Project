/**
 * @file
 * @brief Tilt-controlled bouncing ball screen, driven by the MPU6050.
 */

#include "ball_game.h"
#include "launcher.h"
#include "events_logs.h"
#include "mpu6050_input.h"
#include <lvgl_zephyr.h>

#define SCREEN_W 480
#define SCREEN_H 272
#define BALL_RADIUS 10

#define PHYSICS_PERIOD_MS 33

/* Tunable "feel" constants: how strongly a tilt accelerates the ball,
 * how fast it loses speed (friction), and how much energy a wall
 * bounce keeps. */
#define ACCEL_TO_PX 0.05f
#define DAMPING 0.98f
#define RESTITUTION 0.7f

/* Flip to -1 if a tilt moves the ball the wrong way - depends on how
 * the MPU6050 is physically mounted relative to the screen. */
#define ACCEL_X_SIGN 1
#define ACCEL_Y_SIGN 1

static lv_obj_t *ball_game_screen;
static lv_obj_t *ball;
static lv_timer_t *physics_timer;

static float ball_x = SCREEN_W / 2.0f;
static float ball_y = SCREEN_H / 2.0f;
static float ball_vx;
static float ball_vy;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[ball_game]: Back to menu\n");
    events_logs_add("[ball_game] Back to menu");
    lv_screen_load(launcher_screen_get());
}

static void physics_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    const float dt = PHYSICS_PERIOD_MS / 1000.0f;

    float ax = ACCEL_X_SIGN * mpu6050_input_get_accel_x_mms2() * ACCEL_TO_PX;
    float ay = ACCEL_Y_SIGN * mpu6050_input_get_accel_y_mms2() * ACCEL_TO_PX;

    ball_vx = (ball_vx + ax * dt) * DAMPING;
    ball_vy = (ball_vy + ay * dt) * DAMPING;

    ball_x += ball_vx * dt;
    ball_y += ball_vy * dt;

    if (ball_x < BALL_RADIUS) {
        ball_x = BALL_RADIUS;
        ball_vx = -ball_vx * RESTITUTION;
    } else if (ball_x > SCREEN_W - BALL_RADIUS) {
        ball_x = SCREEN_W - BALL_RADIUS;
        ball_vx = -ball_vx * RESTITUTION;
    }

    if (ball_y < BALL_RADIUS) {
        ball_y = BALL_RADIUS;
        ball_vy = -ball_vy * RESTITUTION;
    } else if (ball_y > SCREEN_H - BALL_RADIUS) {
        ball_y = SCREEN_H - BALL_RADIUS;
        ball_vy = -ball_vy * RESTITUTION;
    }

    lv_obj_set_pos(ball, (int32_t)(ball_x - BALL_RADIUS), (int32_t)(ball_y - BALL_RADIUS));
}

/* Physics timer only runs while this screen is visible, to spare the
 * LVGL thread. */
static void ball_game_visibility_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        ball_x = SCREEN_W / 2.0f;
        ball_y = SCREEN_H / 2.0f;
        ball_vx = 0.0f;
        ball_vy = 0.0f;
        lv_timer_resume(physics_timer);
    } else {
        lv_timer_pause(physics_timer);
    }
}

lv_obj_t *ball_game_screen_get(void)
{
    return ball_game_screen;
}

void ball_game_init(void)
{
    lvgl_lock();

    ball_game_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ball_game_screen, lv_color_black(), 0);
    lv_obj_clear_flag(ball_game_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back_btn = lv_button_create(ball_game_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    ball = lv_obj_create(ball_game_screen);
    lv_obj_set_size(ball, BALL_RADIUS * 2, BALL_RADIUS * 2);
    lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ball, lv_color_hex(0x00A651), 0);
    lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ball, 0, 0);
    lv_obj_clear_flag(ball, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ball, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(ball, (int32_t)(ball_x - BALL_RADIUS), (int32_t)(ball_y - BALL_RADIUS));

    physics_timer = lv_timer_create(physics_timer_cb, PHYSICS_PERIOD_MS, NULL);
    lv_timer_pause(physics_timer);

    lv_obj_add_event_cb(ball_game_screen, ball_game_visibility_cb,
                         LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ball_game_screen, ball_game_visibility_cb,
                         LV_EVENT_SCREEN_UNLOADED, NULL);

    lvgl_unlock();
}
