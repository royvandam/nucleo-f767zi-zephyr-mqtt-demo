#pragma once

#include <zephyr.h>
#include <net/mqtt.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_SERVICE_STACK_SIZE 2048
#define MQTT_SERVICE_PRIO 5

enum mqtt_service_state {
    MQTT_SERVICE_DISCONNECTED = 0,
    MQTT_SERVICE_CONNECTED = 1,
};

struct mqtt_service;

struct mqtt_service_client {
    struct mqtt_client client;
    void* context;
};

typedef struct mqtt_service {
    struct mqtt_service_client client;
    struct sockaddr_storage broker;
    enum mqtt_service_state state;

    struct {
        k_tid_t id;
        K_THREAD_STACK_MEMBER(stack, MQTT_SERVICE_STACK_SIZE);
        struct k_thread data;
    } thread;

    struct k_mutex mutex;
} mqtt_service_t;

void mqtt_service_init(struct mqtt_service* self,
    const char* client_id,
    const char* broker_addr, uint16_t broker_port);
void mqtt_service_start(struct mqtt_service* self);

#ifdef __cplusplus
}
#endif