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

/* Stronger friction applied per-axis once its tilt input drops into
 * the deadzone, so residual velocity from the last active tilt bleeds
 * off in well under a second instead of coasting for several seconds
 * on DAMPING alone. */
#define RELEASE_DAMPING 0.80f

/* Flip to -1 if a tilt moves the ball the wrong way - depends on how
 * the MPU6050 is physically mounted relative to the screen. */
#define ACCEL_X_SIGN 1
#define ACCEL_Y_SIGN 1

/* Ignore residual noise left after mpu6050_input.c's boot calibration,
 * so the ball fully settles instead of creeping. */
#define ACCEL_DEADZONE_MMS2 80.0f
#define VELOCITY_EPSILON_PXS 2.0f

/* Trail left behind while the ball is actually moving: a fixed pool of
 * dash marks reused as a ring buffer, so it stays bounded instead of
 * growing the LVGL heap forever. Plain solid-color rectangles rather
 * than text labels - font glyph rendering (anti-aliased) is far more
 * expensive to redraw than a flat fill, and cost adds up once dozens
 * of marks sit under the ball's repeatedly-invalidated redraw area. */
#define TRACE_MAX_COUNT 150
#define TRACE_MARK_PERIOD_TICKS 3
#define TRACE_MIN_SPEED_PXS 3.0f
#define TRACE_MARK_W 6
#define TRACE_MARK_H 2

static lv_obj_t *ball_game_screen;
static lv_obj_t *ball;
static lv_timer_t *physics_timer;

static lv_obj_t *trace_marks[TRACE_MAX_COUNT];
static int trace_write_index;
static int trace_tick_counter;

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

/* Recalibrates the MPU6050's zero-offset in whatever orientation it's
 * held in right now - lets "neutral" be redefined on demand instead of
 * only at boot. mpu6050_input_calibrate() just signals a background
 * thread, so this doesn't block the LVGL/touch context. */
static void calibrate_cb(lv_event_t *e)
{
    (void)e;
    printk("[ball_game]: Recalibrating MPU6050 zero-offset\n");
    events_logs_add("[ball_game] Recalibrating MPU6050");
    mpu6050_input_calibrate();
    ball_x = SCREEN_W / 2.0f;
    ball_y = SCREEN_H / 2.0f;
    ball_vx = 0.0f;
    ball_vy = 0.0f;
}

static void clear_trace_cb(lv_event_t *e)
{
    (void)e;
    for (int i = 0; i < TRACE_MAX_COUNT; i++) {
        lv_obj_add_flag(trace_marks[i], LV_OBJ_FLAG_HIDDEN);
    }
    trace_write_index = 0;
    printk("[ball_game]: Trace cleared\n");
    events_logs_add("[ball_game] Trace cleared");
}

static void physics_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    const float dt = PHYSICS_PERIOD_MS / 1000.0f;

    int32_t raw_ax = mpu6050_input_get_accel_x_mms2();
    int32_t raw_ay = mpu6050_input_get_accel_y_mms2();

    if (raw_ax > -ACCEL_DEADZONE_MMS2 && raw_ax < ACCEL_DEADZONE_MMS2) {
        raw_ax = 0;
    }
    if (raw_ay > -ACCEL_DEADZONE_MMS2 && raw_ay < ACCEL_DEADZONE_MMS2) {
        raw_ay = 0;
    }

    float ax = ACCEL_X_SIGN * raw_ax * ACCEL_TO_PX;
    float ay = ACCEL_Y_SIGN * raw_ay * ACCEL_TO_PX;

    ball_vx = (ball_vx + ax * dt) * (ax == 0.0f ? RELEASE_DAMPING : DAMPING);
    ball_vy = (ball_vy + ay * dt) * (ay == 0.0f ? RELEASE_DAMPING : DAMPING);

    if (ax == 0.0f && ball_vx > -VELOCITY_EPSILON_PXS && ball_vx < VELOCITY_EPSILON_PXS) {
        ball_vx = 0.0f;
    }
    if (ay == 0.0f && ball_vy > -VELOCITY_EPSILON_PXS && ball_vy < VELOCITY_EPSILON_PXS) {
        ball_vy = 0.0f;
    }

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

    float speed_sq = ball_vx * ball_vx + ball_vy * ball_vy;
    if (speed_sq > TRACE_MIN_SPEED_PXS * TRACE_MIN_SPEED_PXS &&
        ++trace_tick_counter >= TRACE_MARK_PERIOD_TICKS) {
        trace_tick_counter = 0;

        lv_obj_t *mark = trace_marks[trace_write_index];
        lv_obj_set_pos(mark, (int32_t)ball_x - TRACE_MARK_W / 2, (int32_t)ball_y - TRACE_MARK_H / 2);
        lv_obj_clear_flag(mark, LV_OBJ_FLAG_HIDDEN);
        trace_write_index = (trace_write_index + 1) % TRACE_MAX_COUNT;
    }
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
    lv_obj_set_style_bg_color(ball_game_screen, lv_color_white(), 0);
    lv_obj_clear_flag(ball_game_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back_btn = lv_button_create(ball_game_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    lv_obj_t *calib_btn = lv_button_create(ball_game_screen);
    lv_obj_set_size(calib_btn, 80, 30);
    lv_obj_align(calib_btn, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_obj_add_event_cb(calib_btn, calibrate_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *calib_label = lv_label_create(calib_btn);
    lv_label_set_text(calib_label, "Calib");
    lv_obj_center(calib_label);

    lv_obj_t *clear_btn = lv_button_create(ball_game_screen);
    lv_obj_set_size(clear_btn, 80, 30);
    lv_obj_align(clear_btn, LV_ALIGN_TOP_LEFT, 10, 90);
    lv_obj_add_event_cb(clear_btn, clear_trace_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "Clear");
    lv_obj_center(clear_label);

    for (int i = 0; i < TRACE_MAX_COUNT; i++) {
        lv_obj_t *mark = lv_obj_create(ball_game_screen);
        lv_obj_set_size(mark, TRACE_MARK_W, TRACE_MARK_H);
        lv_obj_set_style_radius(mark, 0, 0);
        lv_obj_set_style_bg_color(mark, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(mark, 0, 0);
        lv_obj_clear_flag(mark, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(mark, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);
        trace_marks[i] = mark;
    }

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
