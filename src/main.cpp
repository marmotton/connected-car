#include <Arduino.h>

#include "globals.h"
#include "config.h"

#include "msg_forwarder.h"
#include "display.h"
#include "leafCAN.h"
#include "sdcard_logger.h"
#include "comm_gnss.h"
#include "pressure.h"

QueueHandle_t q_out = xQueueCreate(50, sizeof(Message));
QueueHandle_t q_leafcan = xQueueCreate(50, sizeof(Message));
QueueHandle_t q_display = xQueueCreate(50, sizeof(Message));
QueueHandle_t q_logger = xQueueCreate(50, sizeof(Message));
QueueHandle_t q_comm_gnss = xQueueCreate(50, sizeof(Message));

float some_test_value = 0;

void setup() {
    // Temporary forcing of some outputs (new PCB)
    // Enable power on the GPS and display
    pinMode(5, OUTPUT);
    digitalWrite(5, LOW);
    // CAN transceiver mode
    pinMode(15, OUTPUT);
    //digitalWrite(15, HIGH);  // listen-only mode
    digitalWrite(15, LOW);  // high speed (read-write) mode

    // USB serial port
    Serial.begin( SERIAL_BAUDRATE );
    Serial.setDebugOutput( ENABLE_SERIAL_DEBUG );

    xTaskCreatePinnedToCore( msg_forwarder_task, "msg_forwarder_task", 2048, NULL, 5, NULL, 1);
    // xTaskCreatePinnedToCore( display_task, "display_task", 8192, NULL, 5, NULL, 1);
    // xTaskCreatePinnedToCore( leafcan_task, "leafcan_task", 2048, NULL, 5, NULL, 1);
    // xTaskCreatePinnedToCore( logger_task, "logger_task", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore( comm_gnss_task, "comm_gnss_task", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore( pressure_task, "pressure_task", 2048, NULL, 5, NULL, 1);
}

void loop() {
    delay(1000);
}
