#include "mqtt_service.h"

#include <net/socket.h>
#include <net/mqtt.h>
#include <random/rand32.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(mqtt_service, LOG_LEVEL_INF);

#define NULL_PARAM_CHECK(param) \
	do { \
		if ((param) == NULL) { \
			return -EINVAL; \
		} \
	} while (0)

#define NULL_PARAM_CHECK_VOID(param) \
	do { \
		if ((param) == NULL) { \
			return; \
		} \
	} while (0)

static int _mqtt_service_discard_payload(struct mqtt_service* self, size_t len) {
    struct mqtt_client* client = (struct mqtt_client*)&self->client;

    int bytes_read = 0;
    while (bytes_read < len) {
        uint8_t buffer[32] = {0};
        size_t remaining = len - bytes_read;

        int ret = mqtt_read_publish_payload(client, buffer, MIN(remaining, sizeof(buffer)));
        if (ret < 0 && ret != -EAGAIN) {
            LOG_ERR("mqtt_read_publish_payload: %d", ret);
            return ret;
        } else if(ret == -EAGAIN) {
            return 0;
        }

        bytes_read += ret;
    }

    return 0;
}

int mqtt_service_read_payload(struct mqtt_service* self, void* buffer, size_t len) {
    struct mqtt_client* client = (struct mqtt_client*)&self->client;

    NULL_PARAM_CHECK(self);
    NULL_PARAM_CHECK(buffer);

    int ret = mqtt_read_publish_payload_blocking(client, buffer, len);
    if (ret < 0) {
        LOG_ERR("mqtt_read_publish_payload_block: %d", ret);
        return ret;
    } 

    return 0;
}

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
        LOG_INF("Connected!");
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("Disconnected: %d", evt->result);
        self->state = MQTT_SERVICE_DISCONNECTED;
        break;

    case MQTT_EVT_PUBLISH:
		if (evt->result != 0) {
			LOG_ERR("PUBLISH error %d", evt->result);
			break;
		}

        // Parse topic in to string buffer
        char topic[MQTT_MAX_TOPIC_LEN];
        size_t topic_len = MIN(evt->param.publish.message.topic.topic.size, MQTT_MAX_TOPIC_LEN);
        if (evt->param.publish.message.topic.topic.size > MQTT_MAX_TOPIC_LEN) {
            LOG_WRN("Truncating %d bytes from the topic", evt->param.publish.message.topic.topic.size - topic_len);
        }
        strncpy(topic, evt->param.publish.message.topic.topic.utf8, topic_len);
        topic[topic_len] = '\0';
		LOG_DBG("PUBLISH on topic: %s", log_strdup(topic));

        // Invoke callback handler to read and parse the message payload
        size_t payload_len = evt->param.publish.message.payload.len;
        if (self->callback == NULL || self->callback(self, topic, payload_len) < 0) {
            // The callback is either not set or did not return successfully. Discard the pending data in order
            // to prevent the process handler for going mental on the remaining bytes in the buffer.
            // It has cost me at least a f*$&ng day to figure this one out...
            LOG_WRN("Discarding %d bytes of payload", payload_len);
            _mqtt_service_discard_payload(self, payload_len);
        }

        // Reply to the broker we received the message in good order.
        switch (evt->param.publish.message.topic.qos) {
            case MQTT_QOS_1_AT_LEAST_ONCE: {
                const struct mqtt_puback_param param = {
                    .message_id = evt->param.publish.message_id
                };

                err = mqtt_publish_qos1_ack(client, &param);
                if (err != 0) {
                    LOG_ERR("Failed to send PUBACK: %d", err);
                }
            } break;

            case MQTT_QOS_2_EXACTLY_ONCE: {
                const struct mqtt_pubrec_param param = {
                    .message_id = evt->param.publish.message_id
                };

                err = mqtt_publish_qos2_receive(client, &param);
                if (err != 0) {
                    LOG_ERR("Failed to send PUBREC: %d", err);
                }
            } break;
        }
        break;

    case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			LOG_ERR("PUBACK error %d", evt->result);
			break;
		}

		LOG_DBG("PUBACK packet id: %u", evt->param.puback.message_id);
		break;

    case MQTT_EVT_PUBREC: {
		if (evt->result != 0) {
			LOG_ERR("PUBREC error %d", evt->result);
			break;
		}
		LOG_DBG("PUBREC packet id: %u", evt->param.pubrec.message_id);

		const struct mqtt_pubrel_param param = {
			.message_id = evt->param.pubrec.message_id
		};

		err = mqtt_publish_qos2_release(client, &param);
		if (err != 0) {
			LOG_ERR("Failed to send PUBREL: %d", err);
		}
    } break;

    case MQTT_EVT_PUBREL: {
		if (evt->result != 0) {
			LOG_ERR("PUBREL error %d", evt->result);
			break;
		}
		LOG_DBG("PUBREL packet id: %u", evt->param.pubrel.message_id);
        
		const struct mqtt_pubcomp_param param = {
			.message_id = evt->param.pubrel.message_id
		};

		err = mqtt_publish_qos2_complete(client, &param);
		if (err != 0) {
			LOG_ERR("Failed to send PUBCOMP: %d", err);
		}
    } break;

	case MQTT_EVT_PUBCOMP:
		if (evt->result != 0) {
			LOG_ERR("PUBCOMP error %d", evt->result);
			break;
		}

		LOG_DBG("PUBCOMP packet id: %u", evt->param.pubcomp.message_id);
		break;

    case MQTT_EVT_SUBACK:
		if (evt->result != 0) {
			LOG_ERR("SUBACK error %d", evt->result);
			break;
		}
		LOG_DBG("SUBACK packet id: %u", evt->param.suback.message_id);
        break;

    case MQTT_EVT_UNSUBACK:
		if (evt->result != 0) {
			LOG_ERR("UNSUBACK error %d", evt->result);
			break;
		}
		LOG_DBG("UNSUBACK packet id: %u", evt->param.unsuback.message_id);
        break;

	case MQTT_EVT_PINGRESP:
		LOG_DBG("PINGRESP packet");
		break;

	default:
		break;
	}
}

static int _mqtt_service_wait(struct mqtt_service *self, int timeout) {
    struct mqtt_client* client = (struct mqtt_client*)&self->client;
    struct pollfd fds[1];

    fds[0].fd = client->transport.tcp.sock;
    fds[0].events = ZSOCK_POLLIN;

    int ret = zsock_poll(fds, 1, timeout);
    if (ret < 0) {
        LOG_ERR("poll error: %d", errno);
    }

    if (fds[0].revents & ZSOCK_POLLHUP) {
        LOG_WRN("poll HUP event");
    }

	return ret;
}

static int _mqtt_service_connect(struct mqtt_service* self, int attempts) {
    struct mqtt_client* client = (struct mqtt_client*)&self->client;
    int rc, attempt = 0;

    while (attempt++ < attempts && self->state != MQTT_SERVICE_CONNECTED) {
        LOG_INF("Connecting to broker... (%d/%d)", attempt, attempts);
        rc = mqtt_connect(client);
        if (rc != 0) {
            LOG_ERR("mqtt_connect: %d", rc);
            k_sleep(K_MSEC(1000));
            continue;
        }

        if (_mqtt_service_wait(self, 5000)) {
            rc = mqtt_input(client);
            if (rc != 0) {
                LOG_ERR("mqtt_input: %d", rc);
            }
        }

        if (self->state != MQTT_SERVICE_CONNECTED) {
            LOG_WRN("Failed to connect");
			mqtt_abort(client);
		} else {
            return 0;
        }
    }

    return -1;
}

static int _mqtt_service_process(struct mqtt_service* self) {
    struct mqtt_client* client = (struct mqtt_client*)&self->client;
    int rc = 0;

    if (_mqtt_service_wait(self, 1000) != 0) {
        rc = mqtt_input(client);
        if (rc != 0) {
            LOG_ERR("mqtt_input: %d", rc);
        }
    } else {
        rc = mqtt_live(client);
        if (rc != 0 && rc != -EAGAIN) {
            LOG_ERR("mqtt_live: %d", rc);
        }
    }

    return rc;
}

static void _mqtt_service_task(void* context, void* b __unused, void* c __unused) {
    struct mqtt_service* self = (struct mqtt_service*)(context);

    while(1) {
        switch (self->state) {
            case MQTT_SERVICE_DISCONNECTED:
                // Connect to MQTT broker
                if (_mqtt_service_connect(self, 5) != 0) {
                    LOG_ERR("Unable to connect");
                    LOG_INF("Retrying in 30 sec...");
                    k_sleep(K_SECONDS(30));
                }
                break;
            
            case MQTT_SERVICE_CONNECTED:
                // Process MQTT communication
                _mqtt_service_process(self);
                break;

            default: return;
        }
    }
}

void mqtt_service_init(struct mqtt_service* self,
    const char* client_id,
    const char* broker_addr, uint16_t broker_port,
    mqtt_service_callback_t callback)
{
    NULL_PARAM_CHECK_VOID(self);
    NULL_PARAM_CHECK_VOID(client_id);
    NULL_PARAM_CHECK_VOID(broker_addr);

    struct mqtt_client* client = (struct mqtt_client*)&self->client;

    // MQTT service configuration
    self->state = MQTT_SERVICE_DISCONNECTED;
    self->callback = callback;
    self->client.context = self;

    // MQTT broker configuration
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&self->broker;
    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(broker_port);
    inet_pton(AF_INET, broker_addr, &broker4->sin_addr);

    // MQTT client configuration
    mqtt_client_init(client);
    client->broker = &self->broker;
    client->evt_cb = _mqtt_service_evt_handler;
    client->client_id.utf8 = (uint8_t*)client_id;
    client->client_id.size = strlen(client_id);
    client->password = NULL;
    client->user_name = NULL;
    client->protocol_version = MQTT_VERSION_3_1_1;
    client->transport.type = MQTT_TRANSPORT_NON_SECURE;

    // MQTT buffers configuration
    client->rx_buf = self->buffer.rx;
    client->rx_buf_size = sizeof(self->buffer.rx);
    client->tx_buf = self->buffer.tx;
    client->tx_buf_size = sizeof(self->buffer.tx); 
}

void mqtt_service_start(struct mqtt_service* self) {
    NULL_PARAM_CHECK_VOID(self);

    self->thread.id = k_thread_create(&self->thread.data,
        self->thread.stack, K_THREAD_STACK_SIZEOF(self->thread.stack),
        _mqtt_service_task, self, NULL, NULL,
        MQTT_SERVICE_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(self->thread.id, "mqtt_service");
}

int mqtt_service_subscribe(struct mqtt_service* self, const char* topic, uint8_t qos, void* data, size_t len) {
    NULL_PARAM_CHECK(self);
    NULL_PARAM_CHECK(topic);

    int rc = 0;
    struct mqtt_client* client = (struct mqtt_client*)&self->client;

    if (data != NULL) {
        rc = mqtt_service_publish(self, topic, MQTT_QOS_0_AT_MOST_ONCE, data, len);
        if (rc != 0) {
            return rc;
        }
    }

    struct mqtt_topic topics[] = {
        { .topic = { .utf8 = (const uint8_t*)topic, .size = strlen(topic) }, .qos = qos },
    };

    struct mqtt_subscription_list subscriptions = {
        .list = topics,
        .list_count = 1,
        .message_id = sys_rand32_get()
    };

    rc = mqtt_subscribe(client, &subscriptions);
    if (rc != 0) {
        LOG_ERR("mqtt_subscribe: %d", rc);
        return rc;
    }

    return 0;
}

int mqtt_service_publish(struct mqtt_service* self, const char* topic, uint8_t qos, void* data, size_t len) {
    NULL_PARAM_CHECK(self);
    NULL_PARAM_CHECK(topic);
    NULL_PARAM_CHECK(data);

    struct mqtt_client* client = (struct mqtt_client*)&self->client;

	struct mqtt_publish_param param;
	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = (const uint8_t*)topic;
	param.message.topic.topic.size = strlen(topic);
	param.message.payload.data = data;
	param.message.payload.len = len;
	param.message_id = sys_rand32_get();
	param.dup_flag = 0U;
	param.retain_flag = 1U;

	int rc = mqtt_publish(client, &param);
    if (rc != 0) {
        LOG_ERR("mqtt_publish: %d", rc);
    }
    return rc;
}


