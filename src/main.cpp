#include <Arduino.h>
#include "esp_freertos_hooks.h"

#include "globals.h"
#include "config.h"

#include "msg_forwarder.h"
// #include "display.h"
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

// Called when idle, for debugging purposes
int idle_counter = 0;
bool idle_task_hook( void ) {
    idle_counter++;
    return true;
}

void setup() {
    esp_register_freertos_idle_hook(idle_task_hook);

    // CAN transceiver mode
    pinMode(15, OUTPUT);
    digitalWrite(15, LOW);  // high speed (read-write) mode

    // USB serial port
    Serial.begin( SERIAL_BAUDRATE );
    Serial.setDebugOutput( ENABLE_SERIAL_DEBUG );

    xTaskCreatePinnedToCore( msg_forwarder_task, "msg_forwarder_task", 4096, NULL, 5, NULL, 1);  // high watermark 2304
    // xTaskCreatePinnedToCore( display_task, "display_task", 4096, NULL, 5, NULL, 1);  // high watermark 1048
    xTaskCreatePinnedToCore( leafcan_task, "leafcan_task", 4096, NULL, 5, NULL, 1); // high watermark 2220
    xTaskCreatePinnedToCore( logger_task, "logger_task", 4096, NULL, 5, NULL, 1);  // high watermark 2328
    xTaskCreatePinnedToCore( comm_gnss_task, "comm_gnss_task", 4096, NULL, 4, NULL, 1); // high watermark 1736
    xTaskCreatePinnedToCore( pressure_task, "pressure_task", 4096, NULL, 5, NULL, 1);  // high watermark 2448
}

void loop() {
    delay(1000);
}
