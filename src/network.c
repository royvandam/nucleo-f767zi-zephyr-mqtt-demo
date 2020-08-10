#include <logging/log.h>
LOG_MODULE_REGISTER(network, LOG_LEVEL_INF);

#include <net/net_core.h>
#include <net/net_if.h>
#include <net/net_mgmt.h>

#if !defined(CONFIG_NET_CONFIG_SETTINGS)

static struct net_mgmt_event_callback _network_l2_event_cb;
static void _network_l2_event_handler(struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface) {
    switch (event) {
        case NET_EVENT_IF_UP: {
            LOG_INF("Interface: %s up", log_strdup(iface->if_dev->dev->name));
        } return;

        case NET_EVENT_IF_DOWN: {
            LOG_INF("Interface: %s down", log_strdup(iface->if_dev->dev->name));
        } return;
    }
}

#if defined(CONFIG_NET_DHCPV4)

static struct net_mgmt_event_callback _network_l3_event_cb;
static void _network_l3_event_handler(struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface) {
    switch (event) {
        case NET_EVENT_IPV4_ADDR_ADD: {
            char hr_addr[NET_IPV4_ADDR_LEN];
            int i = 0;

            for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
                struct net_if_addr *if_addr = &iface->config.ip.ipv4->unicast[i];
                if (if_addr->addr_type != NET_ADDR_DHCP || !if_addr->is_used) {
                    continue;
                }

                LOG_INF("IPv4 address: %s",
                    log_strdup(net_addr_ntop(AF_INET,
                                &if_addr->address.in_addr,
                                hr_addr, NET_IPV4_ADDR_LEN)));
                LOG_INF("Lease time: %u seconds",
                    iface->config.dhcpv4.lease_time);
                LOG_INF("Subnet: %s",
                    log_strdup(net_addr_ntop(AF_INET,
                                &iface->config.ip.ipv4->netmask,
                                hr_addr, NET_IPV4_ADDR_LEN)));
                LOG_INF("Gateway: %s",
                    log_strdup(net_addr_ntop(AF_INET,
                                &iface->config.ip.ipv4->gw,
                                hr_addr, NET_IPV4_ADDR_LEN)));
                break;
            }
        } return;
    }
}

static void _network_init_ipv4(struct net_if* iface) {
	net_mgmt_init_event_callback(&_network_l3_event_cb, _network_l3_event_handler, NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&_network_l3_event_cb);

    LOG_INF("Starting dhcpv4 client...");
    net_dhcpv4_start(iface);

    LOG_INF("Waiting for IPv4 address to be assigned...");
    net_mgmt_event_wait_on_iface(iface, NET_EVENT_IPV4_ADDR_ADD, NULL, NULL, NULL, K_FOREVER);
}

#else

static void _network_init_ipv4(struct net_if* iface) {
    char hr_addr[NET_IPV4_ADDR_LEN];
	struct in_addr addr;

	if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_MY_IPV4_ADDR, &addr)) {
		LOG_ERR("Invalid address: %s", CONFIG_NET_CONFIG_MY_IPV4_ADDR);
		return;
	}

	net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
	LOG_INF("IPv4 address: %s",
		log_strdup(net_addr_ntop(AF_INET, &addr, hr_addr,
					 NET_IPV4_ADDR_LEN)));

	if (net_addr_pton(AF_INET, CONFIG_NET_CONFIG_MY_IPV4_GW, &addr)) {
		LOG_ERR("Invalid gateway: %s", CONFIG_NET_CONFIG_MY_IPV4_GW);
		return;
	}

    net_if_ipv4_set_gw(iface, &addr);
    LOG_INF("Gateway: %s",
        log_strdup(net_addr_ntop(AF_INET,
                    &iface->config.ip.ipv4->gw,
                    hr_addr, NET_IPV4_ADDR_LEN)));
}

#endif
#endif

void network_init() {
#if !defined(CONFIG_NET_CONFIG_SETTINGS)
	net_mgmt_init_event_callback(&_network_l2_event_cb, _network_l2_event_handler, NET_EVENT_IF_UP | NET_EVENT_IF_DOWN);
	net_mgmt_add_event_callback(&_network_l2_event_cb);

    struct net_if* iface = net_if_get_default();
    LOG_INF("Waiting for interface %s to be up...", log_strdup(iface->if_dev->dev->name));
    net_mgmt_event_wait_on_iface(iface, NET_EVENT_IF_UP, NULL, NULL, NULL, K_FOREVER);

    _network_init_ipv4(iface);
#endif
}