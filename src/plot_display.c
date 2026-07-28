#include "plot_display.h"
#include "launcher.h"
#include "events_logs.h"
#include "adc_input.h"
#include <lvgl_zephyr.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/* ts du trace = periode d'echantillonnage reelle de l'ADC (voir
 * ADC_SAMPLE_PERIOD_MS dans adc_input.c) : chaque tick fait avancer le
 * temps affiche de la meme valeur que le materiel echantillonne. */
#define PLOT_SAMPLE_PERIOD_MS 100
#define PLOT_POINT_COUNT       12
#define PLOT_RANGE_MIN_MV      0
#define PLOT_RANGE_MAX_MV      3300
#define PLOT_WINDOW_MS         (PLOT_POINT_COUNT * PLOT_SAMPLE_PERIOD_MS)

/* Positionnement absolu (ecran 480x272) : pas d'API de graduation native
 * dans cette version de LVGL (pas de lv_chart_set_axis_tick), les echelles
 * X/Y et la legende sont donc des lv_label places a la main autour du
 * graphique. */
#define CHART_X       38
#define CHART_Y       46
#define CHART_W       380
#define CHART_H       190
#define SCALE_DIVS_Y  6   /* 7 graduations mV : pas de 550 mV */
#define SCALE_DIVS_X  6   /* 7 graduations temps : pas de 1000 ms */
#define LEGEND_X      (CHART_X + CHART_W + 8)
#define X_TICK_LABEL_W 50

static lv_obj_t *plot_display_screen;
static lv_obj_t *chart;
static lv_chart_series_t *series[ADC_CHANNEL_COUNT];

/* Chaque graduation temps est ancree a l'instant ou elle a ete emise
 * (x_tick_time_ms) et glisse vers la gauche au meme rythme que les
 * echantillons du chart (LV_CHART_UPDATE_MODE_SHIFT) : elle reste ainsi
 * au niveau X de l'echantillon auquel elle correspond. Quand elle sort a
 * gauche, elle est recyclee a droite avec l'instant courant. */
static lv_obj_t *x_tick_labels[SCALE_DIVS_X + 1];
static int32_t x_tick_time_ms[SCALE_DIVS_X + 1];
static uint32_t plot_now_ms;

/* Echelle Y dynamique : zoom sur la bande de 1000mV qui contient les 4
 * canaux, avec repli sur [min-20, max] si l'ecart depasse 1000mV. Un
 * debounce sur l'identite de la bande (pas sur les bornes exactes, qui
 * varient en continu en mode repli) evite que l'axe ne clignote quand
 * les valeurs oscillent pile a une frontiere. */
#define Y_ZONE_DYNAMIC        (-1)
#define Y_ZONE_NONE           (-2)
#define Y_RANGE_STABLE_TICKS  3 /* ticks avant de committer un changement de bande */
#define Y_DYNAMIC_REFRESH_TICKS 3 /* ticks entre deux rescales en mode repli */

static lv_obj_t *y_labels[SCALE_DIVS_Y + 1];
static int32_t y_range_lo = PLOT_RANGE_MIN_MV;
static int32_t y_range_hi = PLOT_RANGE_MAX_MV;
static int y_active_zone = Y_ZONE_NONE;
static int y_pending_zone = Y_ZONE_NONE;
static int y_pending_count;
static int y_dynamic_tick_count;

static void y_labels_refresh(void)
{
    for (int i = 0; i <= SCALE_DIVS_Y; i++) {
        int32_t value = y_range_hi - i * (y_range_hi - y_range_lo) / SCALE_DIVS_Y;

        lv_label_set_text_fmt(y_labels[i], "%d", (int)value);
    }
}

static int plot_y_zone_select(int32_t min_mv, int32_t max_mv, int32_t *lo, int32_t *hi)
{
    static const int32_t band_lo[] = { 0, 300, 1300, 2300 };

    for (int zone = 0; zone < (int)ARRAY_SIZE(band_lo); zone++) {
        if (min_mv >= band_lo[zone] && max_mv <= band_lo[zone] + 1000) {
            *lo = band_lo[zone];
            *hi = band_lo[zone] + 1000;
            return zone;
        }
    }

    *lo = MAX(min_mv - 20, 0);
    *hi = MIN(max_mv, PLOT_RANGE_MAX_MV);
    return Y_ZONE_DYNAMIC;
}

static void plot_y_range_update(int32_t min_mv, int32_t max_mv)
{
    int32_t lo, hi;
    int zone = plot_y_zone_select(min_mv, max_mv, &lo, &hi);

    if (zone == y_pending_zone) {
        y_pending_count++;
    } else {
        y_pending_zone = zone;
        y_pending_count = 1;
    }

    if (zone == y_active_zone && zone == Y_ZONE_DYNAMIC) {
        /* Deja en mode repli : on suit min/max, mais on limite la
         * frequence de rescale (sinon l'axe change a chaque tick de
         * 100ms des que les potars bougent, ce qui donne un refresh
         * visuel permanent). */
        y_dynamic_tick_count++;
        if (y_dynamic_tick_count < Y_DYNAMIC_REFRESH_TICKS) {
            return;
        }
        y_dynamic_tick_count = 0;

        y_range_lo = lo;
        y_range_hi = hi;
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, lo, hi);
        y_labels_refresh();
        return;
    }

    if (y_pending_count < Y_RANGE_STABLE_TICKS || zone == y_active_zone) {
        return;
    }

    y_active_zone = zone;
    y_range_lo = lo;
    y_range_hi = hi;
    y_dynamic_tick_count = 0;
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, lo, hi);
    y_labels_refresh();
}

static void x_tick_update(int idx)
{
    int32_t age_ms = (int32_t)plot_now_ms - x_tick_time_ms[idx];

    if (age_ms >= PLOT_WINDOW_MS) {
        x_tick_time_ms[idx] = (int32_t)plot_now_ms;
        age_ms = 0;
    }

    if (age_ms < 0) {
        /* Naissance dans le futur (warm-up des 6 premieres secondes) :
         * le tick n'est pas encore atteint, on le masque pour eviter
         * qu'il ne deborde sur la zone de legende a droite du chart. */
        lv_obj_add_flag(x_tick_labels[idx], LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(x_tick_labels[idx], LV_OBJ_FLAG_HIDDEN);

    int32_t pos_x = CHART_X + CHART_W - (age_ms * CHART_W) / PLOT_WINDOW_MS;

    lv_obj_set_x(x_tick_labels[idx], pos_x - X_TICK_LABEL_W / 2);
    lv_label_set_text_fmt(x_tick_labels[idx], "%d", (int)x_tick_time_ms[idx]);
}

static const lv_palette_t series_palette[ADC_CHANNEL_COUNT] = {
    LV_PALETTE_RED,
    LV_PALETTE_GREEN,
    LV_PALETTE_BLUE,
    LV_PALETTE_ORANGE,
};

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[plot_display]: Back to menu\n");
    events_logs_add("[plot_display] Retour menu");
    lv_screen_load(launcher_screen_get());
}

static void plot_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    int32_t adc_mv[ADC_CHANNEL_COUNT];
    int32_t min_mv = PLOT_RANGE_MAX_MV;
    int32_t max_mv = PLOT_RANGE_MIN_MV;

    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        adc_mv[i] = adc_input_get_mv(i);
        lv_chart_set_next_value(chart, series[i], adc_mv[i]);
        min_mv = MIN(min_mv, adc_mv[i]);
        max_mv = MAX(max_mv, adc_mv[i]);
    }
    plot_y_range_update(min_mv, max_mv);

    plot_now_ms += PLOT_SAMPLE_PERIOD_MS;
    for (int i = 0; i <= SCALE_DIVS_X; i++) {
        x_tick_update(i);
    }
}

lv_obj_t *plot_display_screen_get(void)
{
    return plot_display_screen;
}

void plot_display_init(void)
{
    lvgl_lock();

    plot_display_screen = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(plot_display_screen);
    lv_label_set_text(title, "PLOT DISPLAY");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *back_btn = lv_button_create(plot_display_screen);
    lv_obj_set_size(back_btn, 70, 26);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 4);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    lv_obj_t *y_axis_title = lv_label_create(plot_display_screen);
    lv_label_set_text(y_axis_title, "Tension (mV)");
    lv_obj_set_pos(y_axis_title, CHART_X, CHART_Y - 16);

    chart = lv_chart_create(plot_display_screen);
    lv_obj_set_pos(chart, CHART_X, CHART_Y);
    lv_obj_set_size(chart, CHART_W, CHART_H);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, PLOT_POINT_COUNT);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, y_range_lo, y_range_hi);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

    /* Par defaut LVGL dessine un marqueur rond (LV_PART_INDICATOR, rayon
     * cercle) a chaque echantillon. On le remplace par un tiret plat. */
    lv_obj_set_style_radius(chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_width(chart, 8, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart, 2, LV_PART_INDICATOR);

    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        series[i] = lv_chart_add_series(chart, lv_palette_main(series_palette[i]),
                                         LV_CHART_AXIS_PRIMARY_Y);
    }

    /* Echelle Y (mV) : SCALE_DIVS_Y+1 graduations, valeur max en haut.
     * Positions fixes ; le texte est rafraichi dynamiquement par
     * y_labels_refresh() quand la bande active change (voir
     * plot_y_range_update()). */
    for (int i = 0; i <= SCALE_DIVS_Y; i++) {
        y_labels[i] = lv_label_create(plot_display_screen);

        lv_obj_set_width(y_labels[i], CHART_X - 4);
        lv_obj_set_style_text_align(y_labels[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(y_labels[i], 0, CHART_Y + i * CHART_H / SCALE_DIVS_Y - 7);
    }
    y_labels_refresh();

    /* Echelle X (ms) : dynamique, glisse de droite a gauche en meme temps
     * que les echantillons (voir x_tick_update()). Compteur de temps
     * ecoule qui demarre a 0 : les naissances initiales sont reparties
     * dans le futur (0, +1000, ..., +PLOT_WINDOW_MS) et chaque tick reste
     * masque tant que son instant n'est pas atteint (age_ms < 0). */
    for (int i = 0; i <= SCALE_DIVS_X; i++) {
        x_tick_labels[i] = lv_label_create(plot_display_screen);
        lv_obj_set_y(x_tick_labels[i], CHART_Y + CHART_H + 2);
        lv_obj_set_width(x_tick_labels[i], X_TICK_LABEL_W);
        lv_obj_set_style_text_align(x_tick_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        x_tick_time_ms[i] = -(SCALE_DIVS_X - i) * (PLOT_WINDOW_MS / SCALE_DIVS_X) + PLOT_WINDOW_MS;
        x_tick_update(i);
    }

    lv_obj_t *x_axis_title = lv_label_create(plot_display_screen);
    lv_label_set_text(x_axis_title, "Temps (ms)");
    lv_obj_set_width(x_axis_title, CHART_W);
    lv_obj_set_style_text_align(x_axis_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(x_axis_title, CHART_X, CHART_Y + CHART_H + 18);

    /* Legende a droite du graphique : carre de couleur + nom du canal. */
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        int32_t item_y = CHART_Y + 8 + i * (CHART_H / ADC_CHANNEL_COUNT);

        lv_obj_t *swatch = lv_obj_create(plot_display_screen);
        lv_obj_remove_style_all(swatch);
        lv_obj_set_size(swatch, 12, 12);
        lv_obj_set_style_bg_color(swatch, lv_palette_main(series_palette[i]), 0);
        lv_obj_set_style_bg_opa(swatch, LV_OPA_COVER, 0);
        lv_obj_set_pos(swatch, LEGEND_X, item_y);

        lv_obj_t *legend_label = lv_label_create(plot_display_screen);
        lv_label_set_text_fmt(legend_label, "pot%d", i);
        lv_obj_set_pos(legend_label, LEGEND_X + 16, item_y - 2);
    }

    lv_timer_create(plot_timer_cb, PLOT_SAMPLE_PERIOD_MS, NULL);

    lvgl_unlock();
}
