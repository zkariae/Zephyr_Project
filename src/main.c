#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/linker/devicetree_regions.h>

#define RECT_W 16
#define RECT_H 16
#define RECT_W_MAX 480
#define RECT_H_MAX 272

/* Format RGB_888 : 3 octets/pixel, ordre B, G, R (voir display.h)
 * Place en SDRAM externe : trop volumineux pour la RAM interne. */
static uint8_t buf[RECT_W * RECT_H * 3]
    Z_GENERIC_SECTION(LINKER_DT_NODE_REGION_NAME(DT_NODELABEL(sdram1)));

const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));


 /* Remplit le buffer en blanc ou noir selon la valeur de mode */
void black_white_buff(uint8_t buf[RECT_W * RECT_H * 3], uint8_t mode)
{
    if(mode)
    {
        for (int i = 0; i < RECT_W * RECT_H; i++) {
            buf[i * 3 + 0] = 0x00; /* B */
            buf[i * 3 + 1] = 0x00; /* G */
            buf[i * 3 + 2] = 0x00; /* R */
                                                   }
    }
    else
    {
        for (int i = 0; i < RECT_W * RECT_H; i++) {
            buf[i * 3 + 0] = 0xFF; /* B */
            buf[i * 3 + 1] = 0xFF; /* G */
            buf[i * 3 + 2] = 0xFF; /* R */
                                                  }
    }
}


int full_display(const struct device *dev)
{
    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(buf),
        .width = RECT_W,
        .height = RECT_H,
        .pitch = RECT_W,
    };

    uint8_t mode = 0;
    int ret;
    for(int i_W =0; i_W < RECT_W_MAX; i_W += 16)
    {
        for(int i_H=0; i_H < RECT_H_MAX; i_H += 16)
        {
           black_white_buff(buf, mode);
           ret = display_write(dev, i_W, i_H, &desc, buf);
           if (ret < 0) {
           printk("Erreur display_write: %d\n", ret);
           return -1;
           }
           mode = (mode + 1) % 2;
        }
    }
    return 1;
}

/* Remplit tout l'ecran (RECT_W_MAX x RECT_H_MAX) d'une couleur unie.
 * color est au format 0xRRGGBB (comme en HTML/CSS), ex: 0xFFFFFF = blanc. */
void clear_screen(const struct device *dev, uint32_t color)
{
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(buf),
        .width = RECT_W,
        .height = RECT_H,
        .pitch = RECT_W,
    };

    for (int i = 0; i < RECT_W * RECT_H; i++) {
        buf[i * 3 + 0] = b;
        buf[i * 3 + 1] = g;
        buf[i * 3 + 2] = r;
    }

    for (int i_W = 0; i_W < RECT_W_MAX; i_W += RECT_W) {
        for (int i_H = 0; i_H < RECT_H_MAX; i_H += RECT_H) {
            int ret = display_write(dev, i_W, i_H, &desc, buf);
            if (ret < 0) {
                printk("Erreur display_write: %d\n", ret);
                return;
            }
        }
    }
}

/* Police numerique 5x7 (chiffres 0-9 uniquement pour l'instant).
 * Chaque ligne : bit4 = pixel le plus a gauche, bit0 = pixel le plus a droite. */
#define FONT_W 5
#define FONT_H 7
#define FONT_SCALE 4 /* agrandissement, sinon 5x7 pixels reels est illisible sur l'ecran */

static const uint8_t font_digits[10][FONT_H] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, /* 2 */
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
};

static uint8_t char_buf[(FONT_W * FONT_SCALE) * (FONT_H * FONT_SCALE) * 3]
    Z_GENERIC_SECTION(LINKER_DT_NODE_REGION_NAME(DT_NODELABEL(sdram1)));

/* Dessine un chiffre (0-9) agrandi en rouge sur fond blanc, a la position (x, y) */
void draw_char(const struct device *dev, char c, uint16_t x, uint16_t y)
{
    if (c < '0' || c > '9') {
        return;
    }

    const uint8_t *glyph = font_digits[c - '0'];
    uint16_t buf_w = FONT_W * FONT_SCALE;
    uint16_t buf_h = FONT_H * FONT_SCALE;

    for (int row = 0; row < FONT_H; row++) {
        for (int col = 0; col < FONT_W; col++) {
            uint8_t pixel_on = (glyph[row] >> (FONT_W - 1 - col)) & 0x01;

            for (int sy = 0; sy < FONT_SCALE; sy++) {
                for (int sx = 0; sx < FONT_SCALE; sx++) {
                    int px = col * FONT_SCALE + sx;
                    int py = row * FONT_SCALE + sy;
                    int idx = (py * buf_w + px) * 3;

                    if (pixel_on) {
                        char_buf[idx + 0] = 0x00; /* B */
                        char_buf[idx + 1] = 0x00; /* G */
                        char_buf[idx + 2] = 0x00; /* R -> trait rouge */
                    } else {
                        char_buf[idx + 0] = 0xFF; /* B */
                        char_buf[idx + 1] = 0xFF; /* G */
                        char_buf[idx + 2] = 0xFF; /* R -> fond blanc */
                    }
                }
            }
        }
    }

    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(char_buf),
        .width = buf_w,
        .height = buf_h,
        .pitch = buf_w,
    };

    display_write(dev, x, y, &desc, char_buf);
}

int main(void)
{


    if (!device_is_ready(display_dev)) {
        printk("Erreur : l'ecran n'est pas pret\n");
        return -1;
    }



    /* Efface l'ecran (blanc) : display_clear() n'est pas supporte par ce driver */
    clear_screen(display_dev, 0xFFFFFF);

    /* Demo : affiche "7" en haut a gauche */
    draw_char(display_dev, '7', 0, 0);

    display_blanking_off(display_dev);

    printk("Ecran initialise avec succes\n");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}