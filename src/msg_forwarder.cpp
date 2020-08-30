#include <Arduino.h>

#include "globals.h"
#include "msg_forwarder.h"

/*
This task reads messages from the q_out queue
and forwards them to all the other tasks that need them.
*/

void msg_forwarder_task( void *parameter ) {
    for (;;) {
        Message received_msg;

        if ( xQueueReceive(q_out, &received_msg, 5 / portTICK_PERIOD_MS) == pdTRUE ) {
            // Display
            if ( received_msg.name == Message_name::speed_kmh
                || received_msg.name == Message_name::battery_power_kw
                || received_msg.name == Message_name::battery_energy_kwh
                || received_msg.name == Message_name::network_status
                || received_msg.name == Message_name::power_save_mode
                || received_msg.name == Message_name::logger_status
                ) {
                xQueueSendToBack(q_display, &received_msg, 0);
            }

            // Leaf CAN
            if ( received_msg.name == Message_name::ac_request
                || received_msg.name == Message_name::charge_request
                ) {
                xQueueSendToBack(q_leafcan, &received_msg, 0);
            }

            // Logger
            if ( received_msg.name == Message_name::speed_kmh
                || received_msg.name == Message_name::gnss_speed
                || received_msg.name == Message_name::gnss_latitude
                || received_msg.name == Message_name::gnss_longitude
                || received_msg.name == Message_name::gnss_altitude
                || received_msg.name == Message_name::network_latitude
                || received_msg.name == Message_name::network_longitude
                || received_msg.name == Message_name::battery_power_kw
                || received_msg.name == Message_name::battery_energy_kwh
                || received_msg.name == Message_name::car_status
                ) {
                xQueueSendToBack(q_logger, &received_msg, 0);
            }
        }
    }
}
