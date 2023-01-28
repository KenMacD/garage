#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>

#include <hal/nrf_saadc.h>

#include "button.h"
#include "switch.h"
#include "coap_server.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

bool open_pressed = false;
bool lock_pressed = false;
bool lights_pressed = false;

bool locked = false;

typedef enum
{
	CLOSED = 0,
	UNKNOWN = 1,
	OPEN = 2
} state;

static K_SEM_DEFINE(connected, 0, 1);

#if !DT_NODE_EXISTS(DT_PATH(zephyr_user)) || \
	!DT_NODE_HAS_PROP(DT_PATH(zephyr_user), io_channels)
#error "No suitable devicetree overlay specified"
#endif

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)
#define ZEPHYR_GPIO(N) const struct gpio_dt_spec N = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, N##_gpios)

ZEPHYR_GPIO(lock_switch);
SWITCH(open_limit);
SWITCH(close_limit);

struct button_work open_button =
	{
		.click_work = Z_WORK_INITIALIZER(press_button),
		.release_work = Z_WORK_DELAYABLE_INITIALIZER(release_button),
		.gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, open_cmd_gpios),
};

struct button_work lights_button =
	{
		.click_work = Z_WORK_INITIALIZER(press_button),
		.release_work = Z_WORK_DELAYABLE_INITIALIZER(release_button),
		.gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, lights_cmd_gpios),
};

#if 0
static const struct adc_dt_spec ctrl_signal = ADC_DT_SPEC_GET_BY_IDX(ZEPHYR_USER_NODE, 0);
#endif

#define SLEEP_TIME_MS 1000
#define LED0_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static void switch_event_handler(enum switch_evt evt, char *name)
{
	LOG_WRN("Switch event: %s => %d\n", name, evt);
}

void setup_pins()
{
	gpio_pin_configure_dt(&open_button.gpio, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&lock_switch, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&lights_button.gpio, GPIO_OUTPUT_INACTIVE);

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	switch_init(&open_limit, switch_event_handler);
	switch_init(&close_limit, switch_event_handler);

	// adc_channel_setup_dt(&ctrl_signal);
}

void main(void)
{
	int ret;

	open_limit.name = "OPEN";
	close_limit.name = "CLOSE";

	if (!(
			device_is_ready(led.port) &&
			device_is_ready(open_button.gpio.port) &&
			device_is_ready(lock_switch.port) &&
			device_is_ready(lights_button.gpio.port))) // &&
													   // device_is_ready(ctrl_signal.dev)))
	{
		LOG_ERR("%s", "GPIO not ready.\n");
		return;
	}

	setup_pins();
	usb_enable(NULL);

	coap_add_bool_resource("open", &open_limit.active);
	coap_add_bool_resource("close", &close_limit.active);

	coap_add_button("open", &open_button);
	coap_add_button("lights", &lights_button);

	while (1)
	{
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0)
		{
			return;
		}
		k_msleep(SLEEP_TIME_MS);
	}
}
