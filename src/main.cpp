#include <logging/log.h>
LOG_MODULE_REGISTER(pcu, LOG_LEVEL_DBG);

#include "network.h"
#include "gpio.h"
#include "mqtt_service.h"

#include <zephyr.h>
#include <stdlib.h>
#include <random/rand32.h>

#define SW0_NODE	DT_ALIAS(sw0)
#define SW0_GPIO_LABEL	DT_GPIO_LABEL(SW0_NODE, gpios)
#define SW0_GPIO_PIN	DT_GPIO_PIN(SW0_NODE, gpios)

#define LED0_NODE	DT_ALIAS(led0)
#define LED0_GPIO_LABEL	DT_GPIO_LABEL(LED0_NODE, gpios)
#define LED0_GPIO_PIN	DT_GPIO_PIN(LED0_NODE, gpios)

#define LED1_NODE	DT_ALIAS(led1)
#define LED1_GPIO_LABEL	DT_GPIO_LABEL(LED1_NODE, gpios)
#define LED1_GPIO_PIN	DT_GPIO_PIN(LED1_NODE, gpios)

#define LED2_NODE	DT_ALIAS(led2)
#define LED2_GPIO_LABEL	DT_GPIO_LABEL(LED2_NODE, gpios)
#define LED2_GPIO_PIN	DT_GPIO_PIN(LED2_NODE, gpios)

static GPIO::Output led[] = {
    GPIO::Output(device_get_binding(LED0_GPIO_LABEL), LED0_GPIO_PIN, GPIO_OUTPUT_INIT_LOW),
    GPIO::Output(device_get_binding(LED0_GPIO_LABEL), LED1_GPIO_PIN, GPIO_OUTPUT_INIT_LOW),
    GPIO::Output(device_get_binding(LED0_GPIO_LABEL), LED2_GPIO_PIN, GPIO_OUTPUT_INIT_LOW)
};

static GPIO::Input sw[] = {
    GPIO::Input(device_get_binding(SW0_GPIO_LABEL), SW0_GPIO_PIN)
};

// MQTT client
#define MQTT_CLIENTID           "fancom:pcu:42"

// MQTT broker address information
#define MQTT_BROKER_ADDR        "10.0.0.131"
#define MQTT_BROKER_PORT		1883

// MQTT topic definitions
#define MQTT_TOPIC_DEVICE "pcu"
// #define MQTT_TOPIC_DEV_UUID "378cac49-a79e-4634-b343-7057a7878f03"
#define MQTT_TOPIC_UUID "42"
#define MQTT_TOPIC_PREFIX   "dev/" MQTT_TOPIC_DEVICE "/uuid/" MQTT_TOPIC_UUID
#define MQTT_TOPIC_LED_0    MQTT_TOPIC_PREFIX "/out/led[0]"
#define MQTT_TOPIC_LED_1    MQTT_TOPIC_PREFIX "/out/led[1]"
#define MQTT_TOPIC_LED_2    MQTT_TOPIC_PREFIX "/out/led[2]"
#define MQTT_TOPIC_SW_0     MQTT_TOPIC_PREFIX "/in/sw[0]"

static mqtt_service_t mqtt_service;

static int mqtt_update_led_state(struct mqtt_service* service, size_t payload_len, int index) {
    if (payload_len != 1) {
        LOG_ERR("Invalid payload length");
        return -1;
    }
    char value[2] = {0};
    if (mqtt_service_read_payload(service, &value, 1) < 0) {
        return -1;
    }

    led[index].set(atoi(value));

    return 0;
}

static int mqtt_topic_callback(struct mqtt_service* service, const char* topic, size_t payload_len) {
    if (strcmp(MQTT_TOPIC_LED_0, topic) == 0) {
        return mqtt_update_led_state(service, payload_len, 0);
    } else if (strcmp(MQTT_TOPIC_LED_1, topic) == 0) {
        return mqtt_update_led_state(service, payload_len, 1);
    } else if (strcmp(MQTT_TOPIC_LED_2, topic) == 0) {
        return mqtt_update_led_state(service, payload_len, 2);
    }

    LOG_ERR("Unexpected topic: %s", log_strdup(topic));
    return -1;
}

void main(void) {
	network_init();
    k_sleep(K_MSEC(1000));

    mqtt_service_init(&mqtt_service,
        MQTT_CLIENTID,
        MQTT_BROKER_ADDR, MQTT_BROKER_PORT,
        mqtt_topic_callback);
    mqtt_service_start(&mqtt_service);

    while (mqtt_service.state != MQTT_SERVICE_CONNECTED) {
        k_sleep(K_MSEC(1000));
    }

    char initial_value = '0';
    mqtt_service_publish(&mqtt_service, MQTT_TOPIC_SW_0, MQTT_QOS_0_AT_MOST_ONCE, &initial_value, sizeof(initial_value));
    mqtt_service_subscribe(&mqtt_service, MQTT_TOPIC_LED_0, MQTT_QOS_0_AT_MOST_ONCE, &initial_value, sizeof(initial_value));
    mqtt_service_subscribe(&mqtt_service, MQTT_TOPIC_LED_1, MQTT_QOS_0_AT_MOST_ONCE, &initial_value, sizeof(initial_value));
    mqtt_service_subscribe(&mqtt_service, MQTT_TOPIC_LED_2, MQTT_QOS_0_AT_MOST_ONCE, &initial_value, sizeof(initial_value));

    sw[0].set_interrupt(GPIO_INT_EDGE_BOTH);
    sw[0].set_interrupt_handler([](GPIO::Input&, int value) {
        char strbuf[2];
        snprintf(strbuf, sizeof(strbuf), "%d", value);
        mqtt_service_publish(&mqtt_service,
            MQTT_TOPIC_SW_0, MQTT_QOS_0_AT_MOST_ONCE, strbuf, 1);
        LOG_INF("Button SW0: %s", value ? "pressed" : "released");
    });

    while (1) {
        k_sleep(K_MSEC(10000));
    }
}
