#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include<unistd.h>


#include <periphery/gpio.h>

#define LED_COUNT 4

#define MEMO_COUNT 3

void update_led(gpio_t* id, unsigned int value) {
    if (gpio_write(id, !value) < 0) {
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


unsigned int get_value_from_button(unsigned int i) {
    return 1 << (LED_COUNT - 1 - i);
}

unsigned int get_number_from_buttons(bool* buttons) {
    int sum = 0;
    for(int i =0; i < LED_COUNT; i++) {
        if(buttons[i]) {printf("dupa\n"); sum += get_value_from_button(i);}
    }
    return sum;
}

gpio_t* create_led(unsigned int i) {
    return create_gpio(i,24, GPIO_DIR_OUT_LOW);
}

gpio_t* create_button(unsigned int i) {
    return create_gpio(i,12, GPIO_DIR_IN);
}

void set_edge(gpio_t* id) {
    if (gpio_set_edge(id, GPIO_EDGE_RISING) < 0) {
        fprintf(stderr, "gpio_set_edge(): %s\n", gpio_errmsg(id));
        exit(1);
    }
}

void signal_leds(gpio_t** leds) {
    for(int i =0; i < LED_COUNT; i++) {
        update_led(leds[i],1);
    }

    sleep(1);

    for(int i =0; i < LED_COUNT; i++) {
        update_led(leds[i],0);
    }
}

void signal_victory(gpio_t** leds) {

    for(int i =0; i < LED_COUNT; i++) {
        update_led(leds[i],1);
        usleep(2 * 100 * 1000);
        update_led(leds[i],0);
    }

    for(int i =LED_COUNT -1 ; i >= 0; i--) {
        update_led(leds[i],1);
        usleep(2 * 100 * 1000);
        update_led(leds[i],0);
    }


}

int main(void) {
    gpio_t* leds[LED_COUNT];
    gpio_t* buttons[LED_COUNT];
    unsigned int numbers[MEMO_COUNT];

    //CREATE LEDS
    for(int i =0; i<LED_COUNT; i++) {
        leds[i] = create_led(i);
        buttons[i] = create_button(i);
    }

    srand(time(NULL));


    for(int number_index = 0; number_index < MEMO_COUNT; number_index++) {
        numbers[number_index] = rand() % 16;
        //check if led should be lighten
        for(int led_index =0; led_index < LED_COUNT; led_index++) {
            update_led(leds[led_index], numbers[number_index] & (1 << led_index));
        }

       sleep(2);
    }

    for(int i =0; i < 3; i++) {
        signal_leds(leds);
    }

    //NOW USER HAS TO PUT THOSE NUMBERS
    int memo_correct = 0;
    bool correct = true;

    bool clicked_buttons[LED_COUNT] = {false,false,false,false};
    


    while(correct && memo_correct < MEMO_COUNT) {

        //for(int i =0; i < LED_COUNT; i++ ) set_edge(buttons[i]);
        printf("waiting for input: \n");
        int poll_count = gpio_poll_multiple(buttons, LED_COUNT, 5 * 1000 * 1000, clicked_buttons);
        sleep(5);

        printf("POLL_COUNT == %d\n",poll_count);
        unsigned int result = get_number_from_buttons(clicked_buttons);

        if(result != numbers[memo_correct]) {
            correct = false;
            printf("LOSER %d != %d\n",numbers[memo_correct], result);
            break;
        }

        memo_correct++;
    }

    if(correct) signal_victory(leds);
    
    for(int i =0; i<LED_COUNT; i++) {
        gpio_close(leds[i]);
        gpio_close(buttons[i]);
        gpio_free(leds[i]);
        gpio_free(buttons[i]);
    }

    return 0;
}