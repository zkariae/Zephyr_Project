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

int main(void)
{
    

    if (!device_is_ready(display_dev)) {
        printk("Erreur : l'ecran n'est pas pret\n");
        return -1;
    }



    /*Remplir display par des carrés noir et blanc de taille 16 * 16 */
    int ret = full_display(display_dev);
    display_blanking_off(display_dev);

    printk("Ecran initialise avec succes\n");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}