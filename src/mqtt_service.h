#pragma once

#include <zephyr.h>
#include <net/mqtt.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_SERVICE_STACK_SIZE 2048
#define MQTT_SERVICE_PRIO 8 
#define MQTT_MAX_TOPIC_LEN 48

enum mqtt_service_state {
    MQTT_SERVICE_DISCONNECTED = 0,
    MQTT_SERVICE_CONNECTED = 1,
};

struct mqtt_service_client {
    struct mqtt_client client;
    void* context;
};

struct mqtt_service;

typedef int(*mqtt_service_callback_t)(
    struct mqtt_service* service, const char* topic, size_t payload_len);

typedef struct mqtt_service {
    struct mqtt_service_client client;
    struct sockaddr_storage broker;

    struct {
        uint8_t rx[256];
        uint8_t tx[256];
    } buffer;

    struct {
        struct k_thread data;
        k_tid_t id;
        K_THREAD_STACK_MEMBER(stack, MQTT_SERVICE_STACK_SIZE);
    } thread;
    
    enum mqtt_service_state state;
    mqtt_service_callback_t callback;
} mqtt_service_t;

void mqtt_service_init(struct mqtt_service* self,
    const char* client_id,
    const char* broker_addr, uint16_t broker_port,
    mqtt_service_callback_t callback);

void mqtt_service_start(struct mqtt_service* self);

int mqtt_service_subscribe(struct mqtt_service* self, const char* topic, uint8_t qos, void* data, size_t len);
int mqtt_service_publish(struct mqtt_service* self, const char* topic, uint8_t qos, void* data, size_t len);

int mqtt_service_read_payload(struct mqtt_service* self, void* buffer, size_t len);

#ifdef __cplusplus
}
#endif