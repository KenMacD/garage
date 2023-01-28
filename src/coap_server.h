#ifndef _COAP_SERVER_H_
#define _COAP_SERVER_H_

#include <stdbool.h>

#include "button.h"
void coap_main(void);

void coap_add_bool_resource(char *name, bool *value);
void coap_add_button(char *name, struct button_work *cmd);

#endif /* _COAP_SERVER_H_ */
