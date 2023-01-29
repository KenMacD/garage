#ifndef _SWITCH_H_
#define _SWITCH_H_

#include <zephyr/kernel.h>

void cooldown_expired(struct k_work *);

#define SWITCH(G)                                                              \
  static struct switch_ctx G = {                                               \
      .spec = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, G##_gpios),                   \
      .cooldown_work = Z_WORK_DELAYABLE_INITIALIZER(cooldown_expired),         \
  }

enum switch_evt { SWITCH_EVT_CLOSED, SWITCH_EVT_OPENED };

typedef void (*switch_event_handler_t)(enum switch_evt evt, char *name);

struct switch_ctx {
  struct gpio_dt_spec spec;
  struct k_work_delayable cooldown_work;
  char *name;

  bool active;

  struct gpio_callback cb;

  switch_event_handler_t handler;
};

int switch_init(struct switch_ctx *s_ctx, switch_event_handler_t handler);

#endif /* _SWITCH_H_ */
