#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <periphery/gpio.h>

#define LED_COUNT 4
#define MEMO_COUNT 3

void update_led(gpio_t* id, bool value) {
    if (gpio_write(id, value) < 0) {
        fprintf(stderr, "gpio_write(): %s\n", gpio_errmsg(id));
        exit(1);
    }
}

gpio_t* create_gpio(unsigned int i, unsigned int line_offset, gpio_direction_t direction) {
    gpio_t* gpio_device = gpio_new();

    if (gpio_open(gpio_device, "/dev/gpiochip0", line_offset + i, direction) < 0) {
        fprintf(stderr, "gpio_open(): %s\n", gpio_errmsg(gpio_device));
        exit(1);
    }
    return gpio_device;
}

gpio_t* create_led(unsigned int i) {
    return create_gpio(i, 24, GPIO_DIR_OUT_LOW);
}

gpio_t* create_button(unsigned int i) {
    return create_gpio(i, 12, GPIO_DIR_IN);
}

void set_edge(gpio_t* id) {
    if (gpio_set_edge(id, GPIO_EDGE_RISING) < 0) {
        fprintf(stderr, "gpio_set_edge(): %s\n", gpio_errmsg(id));
        exit(1);
    }
}

void signal_leds(gpio_t** leds) {
    for (int i = 0; i < LED_COUNT; i++) {
        update_led(leds[i], true);
    }
    usleep(1000000); // Wait for 1 second
    for (int i = 0; i < LED_COUNT; i++) {
        update_led(leds[i], false);
    }
    usleep(1000000); // Wait for 1 second
}

void signal_victory(gpio_t** leds) {
    for (int i = 0; i < LED_COUNT; i++) {
        update_led(leds[i], true);
        usleep(200000); // Wait for 200 milliseconds
        update_led(leds[i], false);
        usleep(200000); // Wait for 200 milliseconds
    }
}

int main(void) {
    gpio_t* leds[LED_COUNT];
    gpio_t* buttons[LED_COUNT];
    unsigned int numbers[MEMO_COUNT];
    bool correct = true;

    srand(time(NULL));

    // Create LEDs and Buttons
    for (int i = 0; i < LED_COUNT; i++) {
        leds[i] = create_led(i);
        buttons[i] = create_button(i);
    }

    // Generate random numbers for the sequence
    for (int number_index = 0; number_index < MEMO_COUNT; number_index++) {
        numbers[number_index] = rand() % (1 << LED_COUNT);
        // Display the sequence on LEDs
        for (int led_index = 0; led_index < LED_COUNT; led_index++) {
            update_led(leds[led_index], (numbers[number_index] & (1 << led_index)) != 0);
        }
        usleep(2000000); // Wait for 2 seconds between each number
        // Turn off all LEDs
        for (int led_index = 0; led_index < LED_COUNT; led_index++) {
            update_led(leds[led_index], false);
        }
        usleep(500000); // Wait for 0.5 seconds before the next number
    }

    for (int i = 0; i < 3; i++) {
        signal_leds(leds);
    }

    // User Input Phase
    for (int memo_correct = 0; correct && memo_correct < MEMO_COUNT; memo_correct++) {
        printf("Waiting for input...\n");

        // Set edge detection for buttons
        for (int i = 0; i < LED_COUNT; i++) {
            set_edge(buttons[i]);
        }

        // Poll buttons for rising edges
        gpio_poll_multiple(buttons, LED_COUNT, -1, NULL);

        unsigned int result = 0;

        // Check which buttons were pressed
        for (int i = 0; i < LED_COUNT; i++) {
            bool value;
            if (gpio_read(buttons[i], &value) < 0) {
                fprintf(stderr, "gpio_read(): %s\n", gpio_errmsg(buttons[i]));
                exit(1);
            }
            if (value) {
                result |= (1 << i);
            }
        }

        // Compare user input with the sequence
        if (result != numbers[memo_correct]) {
            correct = false;
            printf("Incorrect input! Expected: %d, Received: %d\n", numbers[memo_correct], result);
            break;
        }

        printf("Correct input!\n");
    }

    // Display victory signal if the input was correct
    if (correct) {
        signal_victory(leds);
    }

    // Close and free GPIO handles
    for (int i = 0; i < LED_COUNT; i++) {
        gpio_close(leds[i]);
        gpio_close(buttons[i]);
        gpio_free(leds[i]);
        gpio_free(buttons[i]);
    }

    return 0;
}
