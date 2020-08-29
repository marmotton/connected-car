#include <Arduino.h>

#include "globals.h"
#include "config.h"

#include "msg_forwarder.h"
#include "display.h"
#include "leafCAN.h"

QueueHandle_t q_out = xQueueCreate(50, sizeof(Message));
QueueHandle_t q_leafcan = xQueueCreate(10, sizeof(Message));
QueueHandle_t q_display = xQueueCreate(10, sizeof(Message));

float some_test_value = 0;

void setup() {
    // Temporary forcing of some outputs (new PCB)
    // Enable power on the GPS and display
    pinMode(5, OUTPUT);
    digitalWrite(5, LOW);
    // CAN transceiver in listen-only mode
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);

    // USB serial port
    Serial.begin( SERIAL_BAUDRATE );

    xTaskCreatePinnedToCore( msg_forwarder_task, "msg_forwarder_task", 2048, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore( display_task, "display_task", 2048, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore( leafcan_task, "leafcan_task", 2048, NULL, 5, NULL, 1);
}

void loop() {
    delay(1000);
}
