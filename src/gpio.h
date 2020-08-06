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

//     void set_interrupt_mode(gpio_int_mode mode);
//     void set_interrupt_handle(std::function<void(int)> callback);

// protected:
//     struct gpio_callback _callback;
};

}