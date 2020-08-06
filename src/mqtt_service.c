#include "mqtt_service.h"

#include <net/socket.h>
#include <net/mqtt.h>
#include <random/rand32.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(mqtt_service, LOG_LEVEL_DBG);

static void _mqtt_service_evt_handler(struct mqtt_client *client, const struct mqtt_evt *evt)
{
    struct mqtt_service* self = ((struct mqtt_service_client*)client)->context;

    int err;

    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        if (evt->result != 0) {
            LOG_ERR("MQTT connect failed %d", evt->result);
            break;
        }

        self->state = MQTT_SERVICE_CONNECTED;
        LOG_INF("MQTT client connected!");
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT client disconnected %d", evt->result);

        self->state = MQTT_SERVICE_DISCONNECTED;
        break;

    case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBACK error %d", evt->result);
			break;
		}

		LOG_INF("PUBACK packet id: %u", evt->param.puback.message_id);
		break;

    case MQTT_EVT_PUBREC:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBREC error %d", evt->result);
			break;
		}

		LOG_INF("PUBREC packet id: %u", evt->param.pubrec.message_id);

		const struct mqtt_pubrel_param rel_param = {
			.message_id = evt->param.pubrec.message_id
		};

		err = mqtt_publish_qos2_release(client, &rel_param);
		if (err != 0) {
			LOG_ERR("Failed to send MQTT PUBREL: %d", err);
		}
		break;

	case MQTT_EVT_PUBCOMP:
		if (evt->result != 0) {
			LOG_ERR("MQTT PUBCOMP error %d", evt->result);
			break;
		}

		LOG_INF("PUBCOMP packet id: %u",
			evt->param.pubcomp.message_id);
		break;

    case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT SUBACK error %d", evt->result);
			break;
		}
		LOG_INF("SUBACK packet id: %u", evt->param.suback.message_id);
        break;

    case MQTT_EVT_UNSUBACK:
		if (evt->result != 0) {
			LOG_ERR("MQTT UNSUBACK error %d", evt->result);
			break;
		}
		LOG_INF("UNSUBACK packet id: %u", evt->param.unsuback.message_id);
        break;

	case MQTT_EVT_PINGRESP:
		LOG_INF("PINGRESP packet");
		break;

	default:
		break;
	}
}

static int mqtt_service_wait(struct mqtt_service *self, int timeout) {
    struct mqtt_client* client = (struct mqtt_client*)&self->client;
    struct pollfd fds[1];

    if (client->transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds[0].fd = client->transport.tcp.sock;
	}
    fds[0].events = ZSOCK_POLLIN;

    int ret = poll(fds, 1, timeout);
    if (ret < 0) {
        LOG_ERR("poll error: %d", errno);
    }

	return ret;
}

static int _mqtt_service_connect(struct mqtt_service* self, int attempts) {
    struct mqtt_client* client = (struct mqtt_client*)&self->client;
    int rc, attempt = 0;

    while (attempt++ < attempts && self->state != MQTT_SERVICE_CONNECTED) {
        LOG_INF("Attempting to connect to MQTT broker... (attempt: %d)", attempt);
        rc = mqtt_connect(client);
        if (rc != 0) {
            LOG_ERR("mqtt_connect: %d", rc);
            k_sleep(K_MSEC(1000));
            continue;
        }

        if (mqtt_service_wait(self, 10000)) {
            mqtt_input(client);
        }

        if (self->state != MQTT_SERVICE_CONNECTED) {
            LOG_WRN("Failed to connect to MQTT broker");
			mqtt_abort(client);
		} else {
            LOG_INF("Connected to MQTT broker");
            return 0;
        }
    }

    return -1;
}

static void _mqtt_service_task(void* context, void* b __unused, void* c __unused) {
    uint8_t mqtt_rx_buffer[256];
    uint8_t mqtt_tx_buffer[256];

    int rc = 0;
    struct mqtt_service* self = (struct mqtt_service*)(context);
    struct mqtt_client* client = (struct mqtt_client*)&self->client;

    // MQTT buffers configuration
    client->rx_buf = mqtt_rx_buffer;
    client->rx_buf_size = sizeof(mqtt_rx_buffer);
    client->tx_buf = mqtt_tx_buffer;
    client->tx_buf_size = sizeof(mqtt_tx_buffer); 

    // Connect to MQTT broker
    if (_mqtt_service_connect(self, 5) < 0) {
        LOG_ERR("Unable to connect to MQTT broker, terminating service");
        return;
    }

    while(1) {
        if (mqtt_service_wait(self, 10000)) {
            rc = mqtt_input(client);
            if (rc != 0) {
                LOG_ERR("mqtt_input: %d", rc);
                continue;
            }
        }

        rc = mqtt_live(client);
        if (rc != 0 && rc != -EAGAIN) {
            LOG_ERR("mqtt_live: %d", rc);
            continue;
        }
        else if (rc == 0) {
            rc = mqtt_input(client);
            if (rc != 0) {
                LOG_ERR("mqtt_input: %d", rc);
                continue;
            }
        }
    }
}

void mqtt_service_init(struct mqtt_service* self,
    const char* client_id,
    const char* broker_addr, uint16_t broker_port)
{
    struct mqtt_client* client = (struct mqtt_client*)&self->client;

    // MQTT service configuration
    self->client.context = self;
    k_mutex_init(&self->mutex);

    // MQTT broker configuration
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&self->broker;
    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(broker_port);
    inet_pton(AF_INET, broker_addr, &broker4->sin_addr);

    // MQTT client configuration
    mqtt_client_init(client);
    client->broker = &self->broker;
    client->evt_cb = _mqtt_service_evt_handler;
    client->client_id.utf8 = client_id;
    client->client_id.size = sizeof(client_id) - 1;
    client->password = NULL;
    client->user_name = NULL;
    client->protocol_version = MQTT_VERSION_3_1_1;
    client->transport.type = MQTT_TRANSPORT_NON_SECURE;
}

void mqtt_service_start(struct mqtt_service* self) {
    self->thread.id = k_thread_create(&self->thread.data,
        self->thread.stack, K_THREAD_STACK_SIZEOF(self->thread.stack),
        _mqtt_service_task, self, NULL, NULL,
        MQTT_SERVICE_PRIO, 0, K_NO_WAIT);
}


