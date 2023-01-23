#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_coap_server, LOG_LEVEL_DBG);

#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/udp.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/coap_link_format.h>
#include <zephyr/net/net_conn_mgr.h>
#include <zephyr/net/openthread.h>

#include <openthread/thread.h>

#define MAX_RETRANSMIT_COUNT 2

#define MAX_COAP_MSG_LEN 256

#define MY_COAP_PORT 5683

#define BLOCK_WISE_TRANSFER_SIZE_GET 2048

#define ADDRLEN(sock) \
	(((struct sockaddr *)sock)->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))

#define NUM_PENDINGS 10

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
K_SEM_DEFINE(connected, 0, 1);
static struct net_mgmt_event_callback mgmt_cb;

static void coap_server_process(void *a1, void *a2, void *a3);
K_THREAD_DEFINE(coap_server_id, 4096, coap_server_process, NULL, NULL, NULL, 7, 0, 0);

/* CoAP socket fd */
static int sock;

static struct coap_pending pendings[NUM_PENDINGS];

static int start_coap_server(void)
{
	struct sockaddr_in6 addr6;
	int r;

	memset(&addr6, 0, sizeof(addr6));
	addr6.sin6_family = AF_INET6;
	addr6.sin6_port = htons(MY_COAP_PORT);

	sock = socket(addr6.sin6_family, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
	{
		LOG_ERR("Failed to create UDP socket %d", errno);
		return -errno;
	}

	r = bind(sock, (struct sockaddr *)&addr6, sizeof(addr6));
	if (r < 0)
	{
		LOG_ERR("Failed to bind UDP socket %d", errno);
		return -errno;
	}

	return 0;
}

static int send_coap_reply(struct coap_packet *cpkt,
						   const struct sockaddr *addr,
						   socklen_t addr_len)
{
	int r;

	r = sendto(sock, cpkt->data, cpkt->offset, 0, addr, addr_len);
	if (r < 0)
	{
		LOG_ERR("Failed to send %d", errno);
		r = -errno;
	}

	return r;
}

static int well_known_core_get(struct coap_resource *resource,
							   struct coap_packet *request,
							   struct sockaddr *addr, socklen_t addr_len)
{
	struct coap_packet response;
	uint8_t *data;
	int r;

	data = (uint8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data)
	{
		return -ENOMEM;
	}

	r = coap_well_known_core_get(resource, request, &response,
								 data, MAX_COAP_MSG_LEN);
	if (r < 0)
	{
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

end:
	k_free(data);

	return r;
}

static int piggyback_get(struct coap_resource *resource,
						 struct coap_packet *request,
						 struct sockaddr *addr, socklen_t addr_len)
{
	struct coap_packet response;
	uint8_t payload[40];
	uint8_t token[COAP_TOKEN_MAX_LEN];
	uint8_t *data;
	uint16_t id;
	uint8_t type;
	uint8_t tkl;
	int r;

	type = coap_header_get_type(request);
	id = coap_header_get_id(request);
	tkl = coap_header_get_token(request, token);

	if (type == COAP_TYPE_CON)
	{
		type = COAP_TYPE_ACK;
	}
	else
	{
		type = COAP_TYPE_NON_CON;
	}

	data = (uint8_t *)k_malloc(MAX_COAP_MSG_LEN);
	if (!data)
	{
		return -ENOMEM;
	}

	r = coap_packet_init(&response, data, MAX_COAP_MSG_LEN,
						 COAP_VERSION_1, type, tkl, token,
						 COAP_RESPONSE_CODE_CONTENT, id);
	if (r < 0)
	{
		goto end;
	}

	r = coap_append_option_int(&response, COAP_OPTION_CONTENT_FORMAT,
							   COAP_CONTENT_FORMAT_TEXT_PLAIN);
	if (r < 0)
	{
		goto end;
	}

	r = coap_packet_append_payload_marker(&response);
	if (r < 0)
	{
		goto end;
	}

	/* The response that coap-client expects */
	r = snprintk((char *)payload, sizeof(payload),
				 "%d", *(bool *)(resource->user_data) ? 1 : 0);
	if (r < 0)
	{
		goto end;
	}

	r = coap_packet_append_payload(&response, (uint8_t *)payload,
								   strlen(payload));
	if (r < 0)
	{
		goto end;
	}

	r = send_coap_reply(&response, addr, addr_len);

end:
	k_free(data);

	return r;
}

static const char *const test_path[] = {"test", NULL};

static struct coap_resource resources[] = {
	{
		.get = well_known_core_get,
		.path = COAP_WELL_KNOWN_CORE_PATH,
	},
	{
		.get = piggyback_get,
		.path = test_path,
	},

	{},
};

static void process_coap_request(uint8_t *data, uint16_t data_len,
								 struct sockaddr *client_addr,
								 socklen_t client_addr_len)
{
	struct coap_packet request;
	struct coap_pending *pending;
	struct coap_option options[16] = {0};
	uint8_t opt_num = 16U;
	uint8_t type;
	int r;

	r = coap_packet_parse(&request, data, data_len, options, opt_num);
	if (r < 0)
	{
		LOG_ERR("Invalid data received (%d)\n", r);
		return;
	}

	type = coap_header_get_type(&request);

	pending = coap_pending_received(&request, pendings, NUM_PENDINGS);
	if (!pending)
	{
		goto not_found;
	}

	/* Clear CoAP pending request */
	if (type == COAP_TYPE_ACK || type == COAP_TYPE_RESET)
	{
		k_free(pending->data);
		coap_pending_clear(pending);
	}

	return;

not_found:
	r = coap_handle_request(&request, resources, options, opt_num,
							client_addr, client_addr_len);
	if (r < 0)
	{
		LOG_WRN("No handler for such request (%d)\n", r);
	}
}

static int process_client_request(void)
{
	int received;
	struct sockaddr client_addr;
	socklen_t client_addr_len;
	uint8_t request[MAX_COAP_MSG_LEN];

	do
	{
		client_addr_len = sizeof(client_addr);
		received = recvfrom(sock, request, sizeof(request), 0,
							&client_addr, &client_addr_len);
		if (received < 0)
		{
			LOG_ERR("Connection error %d", errno);
			return -errno;
		}

		process_coap_request(request, received, &client_addr,
							 client_addr_len);
	} while (true);

	return 0;
}

static void event_handler(struct net_mgmt_event_callback *cb,
						  uint32_t mgmt_event, struct net_if *iface)
{
	if ((mgmt_event & EVENT_MASK) != mgmt_event)
	{
		return;
	}

	if (mgmt_event == NET_EVENT_L4_CONNECTED)
	{
		LOG_INF("Network connected");
		k_sem_give(&connected);

		return;
	}
}

void coap_server_process(void *a, void *b, void *c)
{
	int r;

	if (IS_ENABLED(CONFIG_NET_CONNECTION_MANAGER))
	{
		net_mgmt_init_event_callback(&mgmt_cb,
									 event_handler, EVENT_MASK);
		net_mgmt_add_event_callback(&mgmt_cb);
		net_conn_mgr_resend_status();
		k_sem_take(&connected, K_FOREVER);
	}
	else
	{
		k_sleep(K_SECONDS(10U));
	}

	LOG_DBG("Start CoAP-server");

	r = start_coap_server();
	if (r < 0)
	{
		goto quit;
	}

	while (1)
	{
		r = process_client_request();
		if (r < 0)
		{
			goto quit;
		}
	}

	LOG_DBG("Done");
	return;

quit:
	LOG_ERR("Quit");
}
