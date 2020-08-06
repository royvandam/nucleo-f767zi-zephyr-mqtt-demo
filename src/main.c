#include <logging/log.h>
LOG_MODULE_REGISTER(pcu, LOG_LEVEL_DBG);

#include "network.h"
#include "mqtt_service.h"
#include <random/rand32.h>

#include <zephyr.h>

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

/* MQTT client */
#define MQTT_CLIENTID           "fancom_pcu"

/* MQTT Broker address information. */
#define MQTT_BROKER_ADDR        "10.0.0.131"
#define MQTT_BROKER_PORT		1883


static void publish(struct mqtt_service* service,
    const char* topic, enum mqtt_qos qos,
    const char* value)
{
    struct mqtt_client* client = (struct mqtt_client*)&service->client;

	struct mqtt_publish_param param;

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = topic;
	param.message.topic.topic.size = strlen(topic);
	param.message.payload.data = value;
	param.message.payload.len = strlen(value);
	param.message_id = sys_rand32_get();
	param.dup_flag = 0U;
	param.retain_flag = 1U;

	mqtt_publish(client, &param);
}

static mqtt_service_t mqtt_service;

#define TOPIC_A "pcu/sensor/a"
#define TOPIC_B "pcu/sensor/b"
#define TOPIC_C "pcu/sensor/c"

void main(void) {
	network_init();

    mqtt_service_init(&mqtt_service, MQTT_CLIENTID, MQTT_BROKER_ADDR, MQTT_BROKER_PORT);
    mqtt_service_start(&mqtt_service);

    while (mqtt_service.state != MQTT_SERVICE_CONNECTED) {
        k_sleep(K_MSEC(1000));
    }

    struct mqtt_topic topics[] = {
        { .qos = MQTT_QOS_0_AT_MOST_ONCE,  .topic = MQTT_UTF8_LITERAL(TOPIC_A) },
        // { .qos = MQTT_QOS_1_AT_LEAST_ONCE, .topic = MQTT_UTF8_LITERAL(TOPIC_B) },
        // { .qos = MQTT_QOS_2_EXACTLY_ONCE,  .topic = MQTT_UTF8_LITERAL(TOPIC_C) },
    };

    LOG_INF("Topic: %s", topics[0].topic.utf8);

    struct mqtt_subscription_list subscriptions = {
        .list = topics,
        .list_count = 1,
        .message_id = sys_rand32_get()
    };

    mqtt_subscribe((struct mqtt_client*)&mqtt_service.client, &subscriptions);
    k_sleep(K_MSEC(1000));

    while (1) {
        static char value[30];
        snprintk(value, sizeof(value), "{d:{value:%d}", (uint8_t)sys_rand32_get());

        publish(&mqtt_service, TOPIC_A, MQTT_QOS_0_AT_MOST_ONCE, value);
        // publish(&mqtt_service, TOPIC_B, MQTT_QOS_1_AT_LEAST_ONCE, value);
        // publish(&mqtt_service, TOPIC_C, MQTT_QOS_2_EXACTLY_ONCE, value);

        k_sleep(K_MSEC(5000));
    }


    // GPIO::Output led[3] = {
    //     GPIO::Output(device_get_binding(LED0_GPIO_LABEL), LED0_GPIO_PIN, GPIO_OUTPUT_INIT_LOW),
    //     GPIO::Output(device_get_binding(LED0_GPIO_LABEL), LED0_GPIO_PIN, GPIO_OUTPUT_INIT_LOW),
    //     GPIO::Output(device_get_binding(LED0_GPIO_LABEL), LED0_GPIO_PIN, GPIO_OUTPUT_INIT_LOW)
    // };

    // GPIO::Input button(
    //     device_get_binding(SW0_GPIO_LABEL), SW0_GPIO_PIN);
    // button.set_interrupt_mode(GPIO_INT_EDGE_RISING);
    // button.set_interrupt_handle([](int value){
    //     printk("Button pressed: %d", value);
    // });

}
