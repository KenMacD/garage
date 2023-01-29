#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "switch.h"

LOG_MODULE_REGISTER(sw, LOG_LEVEL_DBG);

void cooldown_expired(struct k_work *cooldown_work) {
  struct switch_ctx *s_ctx =
      CONTAINER_OF(cooldown_work, struct switch_ctx, cooldown_work);
  bool active = gpio_pin_get_dt(&s_ctx->spec);
  if (active == s_ctx->active) {
    return;
  } else {
    s_ctx->active = active;
  }

  enum switch_evt evt = active ? SWITCH_EVT_CLOSED : SWITCH_EVT_OPENED;
  if (s_ctx->handler) {
    s_ctx->handler(evt, s_ctx->name);
  }
}

void sw_pressed(const struct device *dev, struct gpio_callback *cb,
                uint32_t pins) {
  struct switch_ctx *s_ctx = CONTAINER_OF(cb, struct switch_ctx, cb);
  k_work_reschedule(&s_ctx->cooldown_work, K_MSEC(50));
}

int switch_init(struct switch_ctx *s_ctx, switch_event_handler_t handler) {
  int err = -1;

  if (!handler) {
    return -EINVAL;
  }

  s_ctx->handler = handler;

  if (!device_is_ready(s_ctx->spec.port)) {
    return -EIO;
  }

  err = gpio_pin_configure_dt(&s_ctx->spec, GPIO_INPUT);
  if (err) {
    LOG_WRN("%s", "pin config failed");
    return err;
  }

  err = gpio_pin_interrupt_configure_dt(&s_ctx->spec, GPIO_INT_EDGE_BOTH);
  if (err) {
    LOG_WRN("%s", "pin inter config failed");
    return err;
  }

  gpio_init_callback(&s_ctx->cb, sw_pressed, BIT(s_ctx->spec.pin));
  err = gpio_add_callback(s_ctx->spec.port, &s_ctx->cb);
  if (err) {
    LOG_WRN("%s", "pin add callback");
    return err;
  }

  return 0;
}
