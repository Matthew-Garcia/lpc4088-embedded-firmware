#include "mbed.h"

DigitalOut led(LED1);   // LED1 = P1.18 on LPC4088 QSB

int main() {
    while (1) {
        led = 1;        // turn ON
        wait_ms(0.2);   // wait 
        led = 0;        // turn OFF
        wait_ms(0.2);   // wait 
    }
}
