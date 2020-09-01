#include <Arduino.h>

#include <driver/can.h>

#include "globals.h"
#include "config.h"
#include "leafCAN.h"
#include "functions.h"

void leafcan_task( void *parameter ) {
    can_general_config_t can_general_config = {
        .mode = CAN_MODE_NORMAL,
        .tx_io = (gpio_num_t) GPIO_NUM_25,
        .rx_io = (gpio_num_t) GPIO_NUM_39,
        .clkout_io = (gpio_num_t) CAN_IO_UNUSED,
        .bus_off_io = (gpio_num_t) CAN_IO_UNUSED,
        .tx_queue_len = 10,
        .rx_queue_len = 10,
        .alerts_enabled = CAN_ALERT_NONE,
        .clkout_divider = 0,
    };

    can_timing_config_t can_timing_config = CAN_TIMING_CONFIG_500KBITS();
    can_filter_config_t can_filter_config = CAN_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t error;

    error = can_driver_install(&can_general_config, &can_timing_config, &can_filter_config);

    if ( error == ESP_OK ) {
        Serial.println("CAN driver installed successfully.");
    }
    else {
        Serial.println("Error with CAN driver install.");
    }

    error = can_start();
    if ( error == ESP_OK ) {
        Serial.println("CAN driver started successfully.");
    }
    else {
        Serial.println("Error with CAN driver start.");
    }

    for (;;) {
        // Read messages from EVCAN bus
        can_message_t can_msg_rx;
        if ( can_receive(&can_msg_rx, 2) == ESP_OK ) {
            // Output the received CAN frame to the serial port (SLCAN format)
            #ifdef ENABLE_SLCAN
            printf( "t%03x%d", can_msg_rx.identifier, can_msg_rx.data_length_code );
            for ( int i = 0; i < can_msg_rx.data_length_code; i++ ){
                printf( "%02x", can_msg_rx.data[i] );
            }
            printf("\r");
            #endif

            switch ( can_msg_rx.identifier ) {
                // Battery energy (2Hz)
                case 0x5BC: {
                    // Extract the 10 first bits of the message (byte[0] + 2 first bits of byte[1])
                    int gids = ( can_msg_rx.data[0] << 2 ) | ( can_msg_rx.data[1] >> 6 );

                    float energy = (float) gids * KWH_PER_GID;

                    send_msg(Message_name::battery_energy_kwh, energy);
                    break;
                }
                
                // Battery power (100Hz)
                case 0x1DB: {
                    float current = 0.5 * twosComplementToInt( ( ( can_msg_rx.data[0] << 3 ) | ( can_msg_rx.data[1] >> 5 ) ), 11 ); // 11 bits, 0.5A per LSB, 2's complement
                    float voltage = 0.5 * ( ( can_msg_rx.data[2] << 2 ) | ( can_msg_rx.data[3] >> 6 ) ); // 10 bits, 0.5V per LSB

                    float power = 0.001 * current * voltage;

                    send_msg(Message_name::battery_power_kw, power);
                    break;
                }

                // Speed (taken from motor RPM, 100Hz)
                case 0x1DA: {
                    float rpm = (float)twosComplementToInt( ( can_msg_rx.data[4] << 7 | can_msg_rx.data[5] >> 1 ), 15 );
                    float speed = rpm * MOTOR_RPM_TO_KMH;

                    send_msg(Message_name::speed_kmh, speed);
                    break;
                }

                // Climate control status (10Hz)
                case 0x54b: {
                    if ( ( can_msg_rx.data[0] & 0x01 ) == 0 ) {
                        send_msg(Message_name::ac_status, Message_status::ac_is_on);
                    }
                    else {
                        send_msg(Message_name::ac_status, Message_status::ac_is_off);
                    }
                }

                // Charger state
                case 0x390: {
                    float max_amps = can_msg_rx.data[6] * 0.5;

                    send_msg(Message_name::charger_max_amps, max_amps);

                    Serial.printf("%ld 0x390: %.1fA, ", millis(), max_amps);
                    for ( int i = 0; i < can_msg_rx.data_length_code; i++ ){
                        Serial.printf( "%02x ", can_msg_rx.data[i] );
                    }
                    Serial.printf("\n");

                    switch ( can_msg_rx.data[5] ) {
                        case 0x80:
                        case 0x82:
                        case 0x92:
                            send_msg(Message_name::charger_status, Message_status::charger_idle);
                            break;

                        case 0x83:
                            send_msg(Message_name::charger_status, Message_status::charger_quick_charging);
                            break;

                        case 0x98:
                            send_msg(Message_name::charger_status, Message_status::charger_plugged_in_timer_wait);

                            break;

                        case 0x88:
                            send_msg(Message_name::charger_status, Message_status::charger_charging);

                            break;

                        case 0x84:
                            send_msg(Message_name::charger_status, Message_status::charger_finished);
                            break;
                    }
                }

                default:
                    break;
            }
        }

        // Write messages to CAN bus
        Message received_msg;
        if ( xQueueReceive(q_leafcan, &received_msg, 0) == pdTRUE ) {
            //can_message_t can_msg_tx;
            //can_transmit(&can_msg_tx, pdMS_TO_TICKS(10));
        }
        /*
        if ( Serial.available() > 0) {
            int c = Serial.read();
            if ( c == 'o') {
                Serial.println("Opening doors...");
                // This doesn't open the doors, but wakes up the bus
                can_message_t can_msg_tx;
                can_msg_tx.identifier = 0x56E;
                can_msg_tx.flags = CAN_MSG_FLAG_NONE;
                can_msg_tx.data_length_code = 1;
                can_msg_tx.data[0] = 47;
                ESP_ERROR_CHECK_WITHOUT_ABORT(can_transmit(&can_msg_tx, pdMS_TO_TICKS(100)));
            }
        }
        */
    }
}
