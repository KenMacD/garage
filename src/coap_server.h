#ifndef _COAP_SERVER_H_
#define _COAP_SERVER_H_

#include <zephyr/drivers/adc.h>

#include <stdbool.h>

#include "button.h"

void coap_main(void);

void coap_add_bool_resource(char *name, bool *value);
void coap_add_button(char *name, struct button_work *cmd);
void coap_add_adc(char *name, struct adc_dt_spec const *adc_chan);

#endif /* _COAP_SERVER_H_ */
