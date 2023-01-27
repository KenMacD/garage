#ifndef _COAP_SERVER_H_
#define _COAP_SERVER_H_

#include <stdbool.h>

void coap_main(void);

void coap_add_bool_resource(char *name, bool *value);

#endif /* _COAP_SERVER_H_ */
