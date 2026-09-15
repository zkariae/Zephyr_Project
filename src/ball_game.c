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

/* Sawtooth obstacle border drawn along all 4 screen edges. TRI_BASE
 * divides both SCREEN_W and SCREEN_H evenly (GCD(480, 272) = 16) so
 * the teeth tile edge-to-edge on every side with no gap or clipped
 * triangle. The ball's play area is inset by TRIANGLE_BAND so it
 * never actually overlaps the teeth yet - collision handling (game
 * over) is a separate follow-up. */
#define TRI_BASE 16
#define TRI_HEIGHT 14
#define TRIANGLE_BAND TRI_HEIGHT
#define TRIANGLE_COLOR lv_color_hex(0xFF8000)

#define PHYSICS_PERIOD_MS 33
#define RENDER_PERIOD_MS 33

#define PHYSICS_THREAD_STACK_SIZE 1024
#define PHYSICS_THREAD_PRIORITY 5

/* Tunable "feel" constants: how strongly a tilt accelerates the ball
 * and how fast it loses speed (friction). Walls no longer bounce the
 * ball (see physics_thread_fn) since touching the obstacle border
 * will become "game over" once collision handling lands. */
#define ACCEL_TO_PX 0.05f
#define DAMPING 0.98f

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

/* Coordinate readout, off by default: only touched (text re-render)
 * when visible, and throttled below the 33ms physics rate since a
 * human doesn't need it refreshed that fast. */
#define DATA_LABEL_PERIOD_TICKS 4

static lv_obj_t *ball_game_screen;
static lv_obj_t *ball;
static lv_timer_t *render_timer;

static lv_obj_t *trace_marks[TRACE_MAX_COUNT];
static int trace_write_index;
static int trace_tick_counter;

static lv_obj_t *data_label;
static bool data_visible;
static int data_tick_counter;

static lv_obj_t *gameover_overlay;
static lv_obj_t *gameover_score_label;
static bool gameover_shown;

struct ball_state {
    float x;
    float y;
    float vx;
    float vy;
    int32_t ax;
    int32_t ay;
    bool game_over;
    int64_t elapsed_ms;
};

static struct ball_state shared_state = {
    .x = SCREEN_W / 2.0f,
    .y = SCREEN_H / 2.0f,
};
static struct k_spinlock state_lock;

static atomic_t physics_active;
static atomic_t reset_requested;
K_SEM_DEFINE(physics_start_sem, 0, 1);

K_THREAD_STACK_DEFINE(physics_thread_stack, PHYSICS_THREAD_STACK_SIZE);
static struct k_thread physics_thread;

/* Paints the orange sawtooth obstacle border on all 4 edges as vector
 * triangles instead of individual lv_obj_t children, since a couple
 * hundred small widgets would waste LVGL object memory for something
 * that never moves. Hooked to the screen's own LV_EVENT_DRAW_MAIN, so
 * it's drawn once as the screen's background, underneath the ball,
 * trace marks and buttons which are drawn afterwards as children. */
static void obstacles_draw_cb(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_obj_t *screen = lv_event_get_target_obj(e);

    lv_area_t coords;
    lv_obj_get_coords(screen, &coords);

    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.color = TRIANGLE_COLOR;
    dsc.opa = LV_OPA_COVER;

    int32_t count_h = SCREEN_W / TRI_BASE;
    int32_t count_v = SCREEN_H / TRI_BASE;

    for (int32_t i = 0; i < count_h; i++) {
        int32_t x0 = coords.x1 + i * TRI_BASE;

        /* Top edge, apex pointing down. */
        dsc.p[0].x = x0;                dsc.p[0].y = coords.y1;
        dsc.p[1].x = x0 + TRI_BASE;     dsc.p[1].y = coords.y1;
        dsc.p[2].x = x0 + TRI_BASE / 2; dsc.p[2].y = coords.y1 + TRI_HEIGHT;
        lv_draw_triangle(layer, &dsc);

        /* Bottom edge, apex pointing up. */
        dsc.p[0].x = x0;                dsc.p[0].y = coords.y2;
        dsc.p[1].x = x0 + TRI_BASE;     dsc.p[1].y = coords.y2;
        dsc.p[2].x = x0 + TRI_BASE / 2; dsc.p[2].y = coords.y2 - TRI_HEIGHT;
        lv_draw_triangle(layer, &dsc);
    }

    for (int32_t i = 0; i < count_v; i++) {
        int32_t y0 = coords.y1 + i * TRI_BASE;

        /* Left edge, apex pointing right. */
        dsc.p[0].x = coords.x1;         dsc.p[0].y = y0;
        dsc.p[1].x = coords.x1;         dsc.p[1].y = y0 + TRI_BASE;
        dsc.p[2].x = coords.x1 + TRI_HEIGHT; dsc.p[2].y = y0 + TRI_BASE / 2;
        lv_draw_triangle(layer, &dsc);

        /* Right edge, apex pointing left. */
        dsc.p[0].x = coords.x2;         dsc.p[0].y = y0;
        dsc.p[1].x = coords.x2;         dsc.p[1].y = y0 + TRI_BASE;
        dsc.p[2].x = coords.x2 - TRI_HEIGHT; dsc.p[2].y = y0 + TRI_BASE / 2;
        lv_draw_triangle(layer, &dsc);
    }
}

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
    atomic_set(&reset_requested, 1);
}

/* Just re-arms the same reset mechanism calibrate_cb uses - the
 * physics thread clears its frozen game-over state and restarts the
 * timer on its next tick. The overlay itself is hidden by
 * render_timer_cb once it observes shared_state.game_over go false,
 * keeping all LVGL object mutation inside that single timer. */
static void gameover_continue_cb(lv_event_t *e)
{
    (void)e;
    printk("[ball_game]: Continue after game over\n");
    events_logs_add("[ball_game] Continue after game over");
    atomic_set(&reset_requested, 1);
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

static void toggle_data_cb(lv_event_t *e)
{
    (void)e;
    data_visible = !data_visible;
    if (data_visible) {
        lv_obj_clear_flag(data_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(data_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void physics_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        k_sem_take(&physics_start_sem, K_FOREVER);

        float x = SCREEN_W / 2.0f;
        float y = SCREEN_H / 2.0f;
        float vx = 0.0f;
        float vy = 0.0f;
        int32_t raw_ax = 0;
        int32_t raw_ay = 0;
        bool game_over = false;
        int64_t game_over_elapsed_ms = 0;
        int64_t game_start_ms = k_uptime_get();
        int64_t next_wake = k_uptime_get();
        int64_t last_tick_ms = next_wake;

        while (atomic_get(&physics_active)) {
            if (atomic_cas(&reset_requested, 1, 0)) {
                x = SCREEN_W / 2.0f;
                y = SCREEN_H / 2.0f;
                vx = 0.0f;
                vy = 0.0f;
                game_over = false;
                game_start_ms = k_uptime_get();
            }

            int64_t now = k_uptime_get();
            float dt = (now - last_tick_ms) / 1000.0f;
            last_tick_ms = now;
            if (dt > 2.0f * PHYSICS_PERIOD_MS / 1000.0f) {
                dt = 2.0f * PHYSICS_PERIOD_MS / 1000.0f;
            }

            /* Ball is frozen where it hit an obstacle until "Continue"
             * or "Menu" is picked (see gameover_continue_cb / the
             * SCREEN_LOADED reset), so skip physics entirely and just
             * keep republishing the frozen state below. */
            if (!game_over) {
                raw_ax = mpu6050_input_get_accel_x_mms2();
                raw_ay = mpu6050_input_get_accel_y_mms2();

                if (raw_ax > -ACCEL_DEADZONE_MMS2 && raw_ax < ACCEL_DEADZONE_MMS2) {
                    raw_ax = 0;
                }
                if (raw_ay > -ACCEL_DEADZONE_MMS2 && raw_ay < ACCEL_DEADZONE_MMS2) {
                    raw_ay = 0;
                }

                float ax = ACCEL_X_SIGN * raw_ax * ACCEL_TO_PX;
                float ay = ACCEL_Y_SIGN * raw_ay * ACCEL_TO_PX;

                vx = (vx + ax * dt) * (ax == 0.0f ? RELEASE_DAMPING : DAMPING);
                vy = (vy + ay * dt) * (ay == 0.0f ? RELEASE_DAMPING : DAMPING);

                if (ax == 0.0f && vx > -VELOCITY_EPSILON_PXS && vx < VELOCITY_EPSILON_PXS) {
                    vx = 0.0f;
                }
                if (ay == 0.0f && vy > -VELOCITY_EPSILON_PXS && vy < VELOCITY_EPSILON_PXS) {
                    vy = 0.0f;
                }

                x += vx * dt;
                y += vy * dt;

                bool hit = false;

                if (x < TRIANGLE_BAND + BALL_RADIUS) {
                    x = TRIANGLE_BAND + BALL_RADIUS;
                    hit = true;
                } else if (x > SCREEN_W - TRIANGLE_BAND - BALL_RADIUS) {
                    x = SCREEN_W - TRIANGLE_BAND - BALL_RADIUS;
                    hit = true;
                }

                if (y < TRIANGLE_BAND + BALL_RADIUS) {
                    y = TRIANGLE_BAND + BALL_RADIUS;
                    hit = true;
                } else if (y > SCREEN_H - TRIANGLE_BAND - BALL_RADIUS) {
                    y = SCREEN_H - TRIANGLE_BAND - BALL_RADIUS;
                    hit = true;
                }

                if (hit) {
                    vx = 0.0f;
                    vy = 0.0f;
                    game_over = true;
                    game_over_elapsed_ms = now - game_start_ms;
                }
            }

            K_SPINLOCK(&state_lock) {
                shared_state.x = x;
                shared_state.y = y;
                shared_state.vx = vx;
                shared_state.vy = vy;
                shared_state.ax = raw_ax;
                shared_state.ay = raw_ay;
                shared_state.game_over = game_over;
                shared_state.elapsed_ms = game_over_elapsed_ms;
            }

            next_wake += PHYSICS_PERIOD_MS;
            k_sleep(K_TIMEOUT_ABS_MS(next_wake));
        }
    }
}

static void render_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    struct ball_state state;

    K_SPINLOCK(&state_lock) {
        state = shared_state;
    }

    lv_obj_set_pos(ball, (int32_t)(state.x - BALL_RADIUS), (int32_t)(state.y - BALL_RADIUS));

    float speed_sq = state.vx * state.vx + state.vy * state.vy;
    if (speed_sq > TRACE_MIN_SPEED_PXS * TRACE_MIN_SPEED_PXS &&
        ++trace_tick_counter >= TRACE_MARK_PERIOD_TICKS) {
        trace_tick_counter = 0;

        lv_obj_t *mark = trace_marks[trace_write_index];
        lv_obj_set_pos(mark, (int32_t)state.x - TRACE_MARK_W / 2, (int32_t)state.y - TRACE_MARK_H / 2);
        lv_obj_clear_flag(mark, LV_OBJ_FLAG_HIDDEN);
        trace_write_index = (trace_write_index + 1) % TRACE_MAX_COUNT;
    }

    if (data_visible && ++data_tick_counter >= DATA_LABEL_PERIOD_TICKS) {
        data_tick_counter = 0;
        lv_label_set_text_fmt(data_label,
                               "X:%d Y:%d\nVx:%d Vy:%d\nAx:%d Ay:%d",
                               (int)state.x, (int)state.y,
                               (int)state.vx, (int)state.vy,
                               (int)state.ax, (int)state.ay);
    }

    if (state.game_over && !gameover_shown) {
        gameover_shown = true;
        int32_t sec = (int32_t)(state.elapsed_ms / 1000);
        int32_t hundredths = (int32_t)((state.elapsed_ms % 1000) / 10);
        lv_label_set_text_fmt(gameover_score_label, "Score: %d.%02ds", (int)sec, (int)hundredths);
        lv_obj_clear_flag(gameover_overlay, LV_OBJ_FLAG_HIDDEN);
    } else if (!state.game_over && gameover_shown) {
        gameover_shown = false;
        lv_obj_add_flag(gameover_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ball_game_visibility_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        gameover_shown = false;
        lv_obj_add_flag(gameover_overlay, LV_OBJ_FLAG_HIDDEN);
        /* Clear the shared flag too, not just the overlay/gameover_shown
         * above - otherwise render_timer_cb can fire before the physics
         * thread (a separate thread) gets scheduled to overwrite this
         * stale "true" from the previous run's freeze, and immediately
         * re-shows the overlay we just hid. */
        K_SPINLOCK(&state_lock) {
            shared_state.game_over = false;
        }
        atomic_set(&physics_active, 1);
        k_sem_give(&physics_start_sem);
        lv_timer_resume(render_timer);
    } else {
        atomic_set(&physics_active, 0);
        lv_timer_pause(render_timer);
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
    lv_obj_add_event_cb(ball_game_screen, obstacles_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

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

    lv_obj_t *data_btn = lv_button_create(ball_game_screen);
    lv_obj_set_size(data_btn, 80, 30);
    lv_obj_align(data_btn, LV_ALIGN_TOP_LEFT, 10, 130);
    lv_obj_add_event_cb(data_btn, toggle_data_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *data_btn_label = lv_label_create(data_btn);
    lv_label_set_text(data_btn_label, "Data");
    lv_obj_center(data_btn_label);

    data_label = lv_label_create(ball_game_screen);
    lv_label_set_text(data_label, "X:0 Y:0\nVx:0 Vy:0\nAx:0 Ay:0");
    lv_obj_set_style_text_color(data_label, lv_color_black(), 0);
    lv_obj_align(data_label, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_clear_flag(data_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(data_label, LV_OBJ_FLAG_HIDDEN);

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
    lv_obj_set_pos(ball, (int32_t)(shared_state.x - BALL_RADIUS), (int32_t)(shared_state.y - BALL_RADIUS));

    gameover_overlay = lv_obj_create(ball_game_screen);
    lv_obj_set_size(gameover_overlay, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(gameover_overlay, 0, 0);
    lv_obj_set_style_radius(gameover_overlay, 0, 0);
    lv_obj_set_style_bg_color(gameover_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(gameover_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(gameover_overlay, 0, 0);
    lv_obj_clear_flag(gameover_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(gameover_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *gameover_title = lv_label_create(gameover_overlay);
    lv_label_set_text(gameover_title, "Game Over");
    lv_obj_set_style_text_color(gameover_title, lv_color_white(), 0);
    lv_obj_align(gameover_title, LV_ALIGN_CENTER, 0, -50);

    gameover_score_label = lv_label_create(gameover_overlay);
    lv_label_set_text(gameover_score_label, "Score: 0.00s");
    lv_obj_set_style_text_color(gameover_score_label, lv_color_white(), 0);
    lv_obj_align(gameover_score_label, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *continue_btn = lv_button_create(gameover_overlay);
    lv_obj_set_size(continue_btn, 120, 40);
    lv_obj_align(continue_btn, LV_ALIGN_CENTER, 0, 30);
    lv_obj_add_event_cb(continue_btn, gameover_continue_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *continue_label = lv_label_create(continue_btn);
    lv_label_set_text(continue_label, "Continue");
    lv_obj_center(continue_label);

    lv_obj_t *gameover_menu_btn = lv_button_create(gameover_overlay);
    lv_obj_set_size(gameover_menu_btn, 120, 40);
    lv_obj_align(gameover_menu_btn, LV_ALIGN_CENTER, 0, 80);
    lv_obj_add_event_cb(gameover_menu_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *gameover_menu_label = lv_label_create(gameover_menu_btn);
    lv_label_set_text(gameover_menu_label, "Menu");
    lv_obj_center(gameover_menu_label);

    render_timer = lv_timer_create(render_timer_cb, RENDER_PERIOD_MS, NULL);
    lv_timer_pause(render_timer);

    k_thread_create(&physics_thread, physics_thread_stack, PHYSICS_THREAD_STACK_SIZE,
                     physics_thread_fn, NULL, NULL, NULL,
                     PHYSICS_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&physics_thread, "ball_physics");

    lv_obj_add_event_cb(ball_game_screen, ball_game_visibility_cb,
                         LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ball_game_screen, ball_game_visibility_cb,
                         LV_EVENT_SCREEN_UNLOADED, NULL);

    lvgl_unlock();
}
