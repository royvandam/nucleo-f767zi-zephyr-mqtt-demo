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

void
Input::set_interrupt(gpio_flags_t mode) {
    int ret = gpio_pin_interrupt_configure(_dev, _pin, mode);
    assert(ret == 0);
}

void
Input::set_interrupt_handler(InterruptHandler handler) {
    _interrupt_handler = handler;
    _interrupt_callback.context = this;
    auto cb = reinterpret_cast<gpio_callback*>(&_interrupt_callback.base);
    gpio_init_callback(cb, Input::_raw_interrupt_handler, BIT(this->_pin));
	gpio_add_callback(this->_dev, cb);
}

void
Input::clear_interrupt_handler() {
    if (_interrupt_handler) {
        auto cb = reinterpret_cast<gpio_callback*>(&_interrupt_callback.base);
        gpio_remove_callback(this->_dev, cb);
        _interrupt_handler = nullptr;
    }
}

void Input::_base_interrupt_handler() {
    if (_interrupt_handler) {
        _interrupt_handler(*this, get());
    }
}

void
Input::_raw_interrupt_handler(struct device *dev __unused,
    struct gpio_callback *cb, uint32_t pin __unused)
{
    auto self = reinterpret_cast<Input*>(
        reinterpret_cast<Input::InterruptCallback*>(cb)->context);
    self->_base_interrupt_handler();
}

} // namespace GPIO