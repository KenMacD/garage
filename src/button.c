#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "button.h"

void press_button(struct k_work *work)
{
    struct button_work *button =
        CONTAINER_OF(work, struct button_work, click_work);
    gpio_pin_set_dt(&button->gpio, 1);
    k_work_reschedule(&button->release_work, K_SECONDS(1U));
}

void release_button(struct k_work *work)
{
    struct button_work *button =
        CONTAINER_OF(work, struct button_work, release_work);
    gpio_pin_set_dt(&button->gpio, 0);
}
