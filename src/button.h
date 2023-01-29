#ifndef _BUTTON_H_
#define _BUTTON_H_

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

struct button_work {
  struct k_work click_work;
  struct k_work_delayable release_work;
  const struct gpio_dt_spec gpio;
};

void press_button(struct k_work *work);
void release_button(struct k_work *work);

#endif /* _BUTTON_H_ */
