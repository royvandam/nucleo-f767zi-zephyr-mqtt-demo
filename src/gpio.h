#pragma once

#include <device.h>
#include <drivers/gpio.h>

#include <functional>

namespace GPIO {

class Pin
{
public:
    ~Pin();

    int get() const;
    operator int() const;

protected:
    Pin(struct device* dev, gpio_pin_t pin, gpio_flags_t flags);

    struct device* _dev;
    gpio_pin_t _pin;
};

class Output
    : public Pin
{
public:
    Output(struct device* dev,
           gpio_pin_t pin,
           gpio_flags_t init = GPIO_OUTPUT_INIT_LOW);

    void set(int value);
    void toggle();

    Output& operator=(Output& other);
    Output& operator=(int value);
};

class Input
    : public Pin
{
public:
    Input(struct device* dev, gpio_pin_t pin);

    using InterruptHandler = std::function<void(Input&, int)>;
    void set_interrupt(gpio_flags_t mode);
    void set_interrupt_handler(InterruptHandler handler);
    void clear_interrupt_handler();

protected:
    struct InterruptCallback {
        gpio_callback base;
        void* context;
    } _interrupt_callback;
    InterruptHandler _interrupt_handler;

    void _base_interrupt_handler();
    static void _raw_interrupt_handler(struct device *dev,
        struct gpio_callback *cb, uint32_t pin);
};

}