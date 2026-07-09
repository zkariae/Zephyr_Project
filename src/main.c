#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        printk("Erreur : l'ecran n'est pas pret\n");
        return -1;
    }

    display_blanking_off(display_dev);

    printk("Ecran initialise avec succes\n");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}