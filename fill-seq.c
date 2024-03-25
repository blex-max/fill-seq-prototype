#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <inttypes.h>

// pins 
#define CLOCK_PIN 8
#define RESET_PIN 9
#define BUTTON_PIN 10
#define BUTTON_LED_PIN 11
#define CLK_RCV_LED_PIN 12
#define OUT_LED_PIN 13

uint32_t gpio_pin_mask = (1 << CLOCK_PIN) | (1 << RESET_PIN) | (1 << BUTTON_PIN) 
    | (1 << BUTTON_LED_PIN) | (1 << OUT_LED_PIN) | (1 << CLK_RCV_LED_PIN);
uint32_t gpio_set_output_mask = (1 << BUTTON_LED_PIN) | (1 << OUT_LED_PIN) 
    | (1 << CLK_RCV_LED_PIN);
// for some reason led_pin_mask = gpio_pin_output_mask doesn't work
uint32_t led_pin_mask = (1 << BUTTON_LED_PIN) | (1 << OUT_LED_PIN) 
    | (1 << CLK_RCV_LED_PIN);
uint32_t led_pulse_pin_mask = (1 << OUT_LED_PIN) | (1 << CLK_RCV_LED_PIN);
int in_pulldown_arr[3] = {CLOCK_PIN, RESET_PIN, BUTTON_PIN};

enum state {
    SOFF,
    SON
};
int s = SOFF; // initialise system state

uint64_t t_clk = 0;
uint64_t t_last_clk = 0;
uint64_t t_period = 0;
uint64_t t_hi_until = 0;

int clk_step = 0;
int reset_at = 16;

bool pulse_hi = 0;
bool clock_on_debug = 0;

void clock_irq_handler(uint gpio, uint32_t events) {
    // get time
    t_clk = time_us_64();
    clk_step = (clk_step + 1) % reset_at;
    // fire outputs appropriately
    printf("clock recieved, step %d\n", clk_step);
    if (s == SON) {
        gpio_put_masked(led_pulse_pin_mask, led_pulse_pin_mask);
    } else if (s == SOFF) {
        gpio_put(CLK_RCV_LED_PIN, 1);
    }
    // do time calcs
    t_period = t_clk - t_last_clk;
    printf("t_last_clk: %" PRIu64 "\n", t_last_clk);
    t_last_clk = t_clk;
    t_hi_until = t_clk + (t_period * 0.5); // period multiplied by duty cycle
    printf("t_clk: %" PRIu64 " | period: %" PRIu64 " | t_hi_unitl: %" PRIu64 "\n", t_clk, t_period, t_hi_until);
    if (pulse_hi) {
        printf("catastrophic error");
        exit(1);
    }
    pulse_hi = 1;
    gpio_acknowledge_irq(CLOCK_PIN, GPIO_IRQ_EDGE_RISE); // this should be unnecessary as interrupts clear themselves?
}


void main() {
    stdio_init_all();
    // set up gpio
    gpio_init_mask(gpio_pin_mask);
    gpio_set_dir_masked(gpio_pin_mask, gpio_set_output_mask);
    // set pulls
    for (int i = 0; i < 3; i++) {
        gpio_pull_down(in_pulldown_arr[i]);
    }
    // set states
    gpio_put_masked(led_pin_mask, 0);
    // init variables
    int i; // button input

    // startup blink
    gpio_put(CLK_RCV_LED_PIN, 1);
    sleep_ms(350);
    gpio_put_masked(led_pin_mask, (1 << BUTTON_LED_PIN));
    sleep_ms(350);
    gpio_put_masked(led_pin_mask, (1 << OUT_LED_PIN));
    sleep_ms(350);
    gpio_put_masked(led_pin_mask, 0);

    // finally, enable interrupts
    gpio_set_irq_enabled_with_callback(CLOCK_PIN, GPIO_IRQ_EDGE_RISE,
        true, clock_irq_handler);

    while (true) {
        // clock_on_debug = gpio_get(CLK_RCV_LED_PIN);
        // printf("clock pin state: %d\n", clock_on_debug);
        sleep_us(1000);
        if (t_hi_until <= time_us_64() && gpio_get(CLK_RCV_LED_PIN) != 0) {
            printf("clock off\n");
            gpio_put_masked(led_pulse_pin_mask, ~led_pulse_pin_mask);
            pulse_hi = 0; // catch if interrupt is occuring in the wrong place?
        }
        i = gpio_get(BUTTON_PIN);
        switch (s) {
            case SOFF:
                if (i) {
                    s = SON;
                    gpio_put(BUTTON_LED_PIN, 1);
                    printf("button pushed\n");
                }
                break;
            case SON:
                if (!i) {
                    s = SOFF;
                    gpio_put(BUTTON_LED_PIN, 0);
                }
                break;
            default:
                s = SOFF;
        }

    }

}
