#include "gpio.h"
#include <assert.h>

namespace GPIO {

Pin::Pin(struct device* dev, gpio_pin_t pin, gpio_flags_t flags)
    : _dev(dev)
    , _pin(pin)
{
    int ret = gpio_pin_configure(_dev, _pin, flags);
    assert(ret == 0);
}

Pin::~Pin() {
    gpio_pin_configure(_dev, _pin, GPIO_DISCONNECTED);
}

int
Pin::get() const {
    return gpio_pin_get(_dev, _pin);
}

Pin::operator int() const {
    return gpio_pin_get(_dev, _pin);
}

Output::Output(struct device* dev, gpio_pin_t pin, gpio_flags_t init_state)
    : Pin(dev, pin, GPIO_OUTPUT | init_state)
{}

void
Output::set(int value) {
    gpio_pin_set(_dev, _pin, value);
}

void
Output::toggle() {
    gpio_pin_toggle(_dev, _pin);
}

Output& Output::operator=(Output& other) {
    set(other);
    return *this;
}

Output& Output::operator=(int value) {
    set(value);
    return *this;
}

Input::Input(struct device* dev, gpio_pin_t pin)
    : Pin(dev, pin, GPIO_INPUT)
{}

// void
// Input::set_interrupt_mode(gpio_int_mode mode) {
//     int ret = gpio_pin_interrupt_configure(_dev, _pin, mode);
//     __ASSERT(ret != 0, "Error %d: failed to configure GPIO pin interrupt");
// }

// void
// Input::set_interrupt_handle(std::function<void(int)> callback) {

// }

} // namespace GPIO