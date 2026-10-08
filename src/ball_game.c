/**
 * @file
 * @brief Tilt-controlled bouncing ball screen, driven by the MPU6050.
 */

#include "ball_game.h"
#include "launcher.h"
#include "events_logs.h"
#include "mpu6050_input.h"
#include <lvgl_zephyr.h>
#include <math.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

#define SCREEN_W 480
#define SCREEN_H 272
#define BALL_RADIUS 10

/* Moving obstacles: small circles that chase the ball's current
 * position at a fixed speed every physics tick (see
 * physics_thread_fn), clamped to stay on screen. Touching one ends
 * the game. Neither starts at the screen center, since that's also
 * where the ball spawns on every (re)start. Since every obstacle
 * chases the same point, two of them can otherwise drift into each
 * other's spot and sit stacked for a while - OBSTACLE_MIN_GAP_PXS
 * keeps them pushed apart instead. */
#define OBSTACLE_COUNT 2
#define OBSTACLE_RADIUS 9
#define OBSTACLE_SPEED_PXS 70.0f
#define OBSTACLE_MIN_GAP_PXS (4.0f * OBSTACLE_RADIUS)
#define OBSTACLE_COLOR lv_color_hex(0xFF8000)

#define PHYSICS_PERIOD_MS 33
#define RENDER_PERIOD_MS 33

#define PHYSICS_THREAD_STACK_SIZE 1024
#define PHYSICS_THREAD_PRIORITY 5

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
static lv_obj_t *gameover_best_label;
static bool gameover_shown;

static lv_obj_t *obstacle_objs[OBSTACLE_COUNT];

struct ball_state {
    float x;
    float y;
    float vx;
    float vy;
    int32_t ax;
    int32_t ay;
    bool game_over;
    int64_t elapsed_ms;
    float obstacle_x[OBSTACLE_COUNT];
    float obstacle_y[OBSTACLE_COUNT];
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

struct obstacle_phys {
    float x;
    float y;
    float vx;
    float vy;
};

/* Starting positions only - velocity is recomputed every tick to
 * chase the ball (see physics_thread_fn), so it doesn't matter here.
 * Kept away from the screen center, where the ball spawns. */
static void obstacles_reset(struct obstacle_phys *obs)
{
    obs[0].x = SCREEN_W * 0.25f;
    obs[0].y = 70.0f;
    obs[0].vx = 0.0f;
    obs[0].vy = 0.0f;

    obs[1].x = SCREEN_W - 60.0f;
    obs[1].y = 60.0f;
    obs[1].vx = 0.0f;
    obs[1].vy = 0.0f;
}

static void obstacle_clamp(struct obstacle_phys *o)
{
    if (o->x < OBSTACLE_RADIUS) {
        o->x = OBSTACLE_RADIUS;
    } else if (o->x > SCREEN_W - OBSTACLE_RADIUS) {
        o->x = SCREEN_W - OBSTACLE_RADIUS;
    }

    if (o->y < OBSTACLE_RADIUS) {
        o->y = OBSTACLE_RADIUS;
    } else if (o->y > SCREEN_H - OBSTACLE_RADIUS) {
        o->y = SCREEN_H - OBSTACLE_RADIUS;
    }
}

#define HIGHSCORE_MAGIC 0x48534331 /* "HSC1" */

struct highscore_record {
    uint32_t magic;
    uint32_t best_ms;
};

static uint32_t best_score_ms;

static void highscore_load(void)
{
    const struct device *dev = PARTITION_DEVICE(highscore_partition);
    struct highscore_record rec;

    if (flash_read(dev, PARTITION_OFFSET(highscore_partition), &rec, sizeof(rec)) == 0 &&
        rec.magic == HIGHSCORE_MAGIC) {
        best_score_ms = rec.best_ms;
    }
}

static void highscore_save(uint32_t best_ms)
{
    const struct device *dev = PARTITION_DEVICE(highscore_partition);
    off_t offset = PARTITION_OFFSET(highscore_partition);
    struct highscore_record rec = {
        .magic = HIGHSCORE_MAGIC,
        .best_ms = best_ms,
    };
    int ret;

    ret = flash_erase(dev, offset, PARTITION_SIZE(highscore_partition));
    if (ret != 0) {
        printk("[ball_game]: highscore flash_erase failed (%d)\n", ret);
        return;
    }

    ret = flash_write(dev, offset, &rec, sizeof(rec));
    if (ret != 0) {
        printk("[ball_game]: highscore flash_write failed (%d)\n", ret);
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

static void clear_trace(void)
{
    for (int i = 0; i < TRACE_MAX_COUNT; i++) {
        lv_obj_add_flag(trace_marks[i], LV_OBJ_FLAG_HIDDEN);
    }
    trace_write_index = 0;
}

/* Just re-arms the same reset mechanism calibrate_cb uses - the
 * physics thread clears its frozen game-over state and restarts the
 * timer on its next tick. The overlay itself is hidden by
 * render_timer_cb once it observes shared_state.game_over go false,
 * keeping all LVGL object mutation inside that single timer. The
 * previous run's trail is cleared here, though, so the new game
 * doesn't start with stale dashes from the ball that just died. */
static void gameover_continue_cb(lv_event_t *e)
{
    (void)e;
    printk("[ball_game]: Continue after game over\n");
    events_logs_add("[ball_game] Continue after game over");
    clear_trace();
    atomic_set(&reset_requested, 1);
}

static void clear_trace_cb(lv_event_t *e)
{
    (void)e;
    clear_trace();
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
        struct obstacle_phys obstacles[OBSTACLE_COUNT];

        obstacles_reset(obstacles);

        while (atomic_get(&physics_active)) {
            if (atomic_cas(&reset_requested, 1, 0)) {
                x = SCREEN_W / 2.0f;
                y = SCREEN_H / 2.0f;
                vx = 0.0f;
                vy = 0.0f;
                game_over = false;
                game_start_ms = k_uptime_get();
                obstacles_reset(obstacles);
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

                if (x < BALL_RADIUS) {
                    x = BALL_RADIUS;
                    vx = -vx * RESTITUTION;
                } else if (x > SCREEN_W - BALL_RADIUS) {
                    x = SCREEN_W - BALL_RADIUS;
                    vx = -vx * RESTITUTION;
                }

                if (y < BALL_RADIUS) {
                    y = BALL_RADIUS;
                    vy = -vy * RESTITUTION;
                } else if (y > SCREEN_H - BALL_RADIUS) {
                    y = SCREEN_H - BALL_RADIUS;
                    vy = -vy * RESTITUTION;
                }

                bool hit = false;

                for (int i = 0; i < OBSTACLE_COUNT; i++) {
                    float dx = x - obstacles[i].x;
                    float dy = y - obstacles[i].y;
                    float dist = sqrtf(dx * dx + dy * dy);

                    /* Steer straight at the ball's current position
                     * every tick, at a fixed speed - a simple chase
                     * rather than a fixed bounce pattern. Guard
                     * against dist == 0 (ball and obstacle exactly
                     * overlapping, which the hit check below already
                     * catches as a collision anyway). */
                    if (dist > 0.01f) {
                        obstacles[i].vx = (dx / dist) * OBSTACLE_SPEED_PXS;
                        obstacles[i].vy = (dy / dist) * OBSTACLE_SPEED_PXS;
                    }

                    obstacles[i].x += obstacles[i].vx * dt;
                    obstacles[i].y += obstacles[i].vy * dt;
                    obstacle_clamp(&obstacles[i]);
                }

                /* All obstacles chase the same point, so without this
                 * a pair can drift into the same spot and sit stacked
                 * on top of each other for a while. Push any pair
                 * closer than OBSTACLE_MIN_GAP_PXS apart symmetrically
                 * along the line between them. */
                for (int i = 0; i < OBSTACLE_COUNT; i++) {
                    for (int j = i + 1; j < OBSTACLE_COUNT; j++) {
                        float odx = obstacles[j].x - obstacles[i].x;
                        float ody = obstacles[j].y - obstacles[i].y;
                        float odist = sqrtf(odx * odx + ody * ody);

                        if (odist >= OBSTACLE_MIN_GAP_PXS) {
                            continue;
                        }

                        float nx, ny;

                        if (odist > 0.01f) {
                            nx = odx / odist;
                            ny = ody / odist;
                        } else {
                            nx = 1.0f;
                            ny = 0.0f;
                        }

                        float push = (OBSTACLE_MIN_GAP_PXS - odist) * 0.5f;

                        obstacles[i].x -= nx * push;
                        obstacles[i].y -= ny * push;
                        obstacles[j].x += nx * push;
                        obstacles[j].y += ny * push;

                        obstacle_clamp(&obstacles[i]);
                        obstacle_clamp(&obstacles[j]);
                    }
                }

                for (int i = 0; i < OBSTACLE_COUNT; i++) {
                    float min_dist = BALL_RADIUS + OBSTACLE_RADIUS;
                    float post_dx = x - obstacles[i].x;
                    float post_dy = y - obstacles[i].y;

                    if (post_dx * post_dx + post_dy * post_dy < min_dist * min_dist) {
                        hit = true;
                    }
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
                for (int i = 0; i < OBSTACLE_COUNT; i++) {
                    shared_state.obstacle_x[i] = obstacles[i].x;
                    shared_state.obstacle_y[i] = obstacles[i].y;
                }
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

    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        lv_obj_set_pos(obstacle_objs[i],
                       (int32_t)(state.obstacle_x[i] - OBSTACLE_RADIUS),
                       (int32_t)(state.obstacle_y[i] - OBSTACLE_RADIUS));
    }

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

        uint32_t elapsed_ms = (uint32_t)state.elapsed_ms;

        if (elapsed_ms > best_score_ms) {
            best_score_ms = elapsed_ms;
            highscore_save(best_score_ms);
        }

        lv_label_set_text_fmt(gameover_score_label, "Score: %d.%02ds",
                               (int)(elapsed_ms / 1000), (int)((elapsed_ms % 1000) / 10));
        lv_label_set_text_fmt(gameover_best_label, "Best: %d.%02ds",
                               (int)(best_score_ms / 1000), (int)((best_score_ms % 1000) / 10));
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
        clear_trace();
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
    highscore_load();

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

    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        lv_obj_t *obs = lv_obj_create(ball_game_screen);
        lv_obj_set_size(obs, OBSTACLE_RADIUS * 2, OBSTACLE_RADIUS * 2);
        lv_obj_set_style_radius(obs, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(obs, OBSTACLE_COLOR, 0);
        lv_obj_set_style_bg_opa(obs, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(obs, 0, 0);
        lv_obj_clear_flag(obs, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(obs, LV_OBJ_FLAG_CLICKABLE);
        obstacle_objs[i] = obs;
    }

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
    lv_obj_align(gameover_title, LV_ALIGN_CENTER, 0, -55);

    gameover_score_label = lv_label_create(gameover_overlay);
    lv_label_set_text(gameover_score_label, "Score: 0.00s");
    lv_obj_set_style_text_color(gameover_score_label, lv_color_white(), 0);
    lv_obj_align(gameover_score_label, LV_ALIGN_CENTER, 0, -28);

    gameover_best_label = lv_label_create(gameover_overlay);
    lv_label_set_text(gameover_best_label, "Best: 0.00s");
    lv_obj_set_style_text_color(gameover_best_label, lv_color_white(), 0);
    lv_obj_align(gameover_best_label, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t *continue_btn = lv_button_create(gameover_overlay);
    lv_obj_set_size(continue_btn, 120, 40);
    lv_obj_align(continue_btn, LV_ALIGN_CENTER, 0, 35);
    lv_obj_add_event_cb(continue_btn, gameover_continue_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *continue_label = lv_label_create(continue_btn);
    lv_label_set_text(continue_label, "Continue");
    lv_obj_center(continue_label);

    lv_obj_t *gameover_menu_btn = lv_button_create(gameover_overlay);
    lv_obj_set_size(gameover_menu_btn, 120, 40);
    lv_obj_align(gameover_menu_btn, LV_ALIGN_CENTER, 0, 85);
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
