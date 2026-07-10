#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/linker/devicetree_regions.h>

#define RECT_W 480
#define RECT_H 272

/* Format RGB_888 : 3 octets/pixel, ordre B, G, R (voir display.h)
 * Place en SDRAM externe : trop volumineux pour la RAM interne. */
static uint8_t buf[RECT_W * RECT_H * 3]
    Z_GENERIC_SECTION(LINKER_DT_NODE_REGION_NAME(DT_NODELABEL(sdram1)));

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        printk("Erreur : l'ecran n'est pas pret\n");
        return -1;
    }

    /* Remplit le buffer en rouge (B=0x00, G=0x00, R=0xFF) */
    for (int i = 0; i < RECT_W * RECT_H; i++) {
        buf[i * 3 + 0] = 0x00; /* B */
        buf[i * 3 + 1] = 0x00; /* G */
        buf[i * 3 + 2] = 0xFF; /* R */
    }

    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(buf),
        .width = RECT_W,
        .height = RECT_H,
        .pitch = RECT_W,
    };

    /* Dessine le rectangle en haut a gauche (x=0, y=0) */
    int ret = display_write(display_dev, 0, 0, &desc, buf);
    if (ret < 0) {
        printk("Erreur display_write: %d\n", ret);
        return -1;
    }

    display_blanking_off(display_dev);

    printk("Ecran initialise avec succes\n");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}