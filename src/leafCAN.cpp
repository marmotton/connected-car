#include <Arduino.h>

#include <driver/can.h>

#include "globals.h"
#include "config.h"
#include "leafCAN.h"
#include "functions.h"

// Some message # etc. taken from here: https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3/blob/master/vehicle/OVMS.V3/components/vehicle_nissanleaf/src/vehicle_nissanleaf.cpp

void wake_canbus() {
    can_message_t can_msg_tx;
    can_msg_tx.identifier = 0x68c;
    can_msg_tx.data[0] = 0x00;
    can_msg_tx.data_length_code = 1;
    can_msg_tx.flags = CAN_MSG_FLAG_NONE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(can_transmit(&can_msg_tx, pdMS_TO_TICKS(100)));
}

void write_to_canbus(can_message_t &can_msg_tx) {
    wake_canbus();
    // Send command multiple times
    for (int i=0; i < 30; i++) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(can_transmit(&can_msg_tx, pdMS_TO_TICKS(100)));
        delay(5);
    }
}

// For debugging purposes
void print_hex_msg(can_message_t &can_msg_rx) {
    printf("ID %x: ", can_msg_rx.identifier);
    
    for ( int i = 0; i < can_msg_rx.data_length_code; i++ ){
        printf( "%02x ", can_msg_rx.data[i] );
    }

    printf("\n");
}

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

    if ( error != ESP_OK ) {
        Serial.println("Error with CAN driver install.");
        delay(1000);
        ESP.restart();
    }

    error = can_start();
    if ( error != ESP_OK ) {
        Serial.println("Error starting CAN.");
        delay(1000);
        ESP.restart();
    }

    bool slcan_enabled = false;

    // Variables used to know if the car is on or off
    unsigned long last_speed_update = 0;
    unsigned long last_car_on_off_update = 0;

    // Some car state needed locally
    bool car_is_plugged_in = false;

    for (;;) {
        // UBaseType_t highWatermark = uxTaskGetStackHighWaterMark(NULL);
        // printf("LeafCAN task high watermark: %d\n", highWatermark);
        // Read messages from EVCAN bus
        can_message_t can_msg_rx;
        if ( can_receive(&can_msg_rx, 2) == ESP_OK ) {
            // Output the received CAN frame to the serial port (SLCAN format)
            if (slcan_enabled) {
                printf( "t%03x%d", can_msg_rx.identifier, can_msg_rx.data_length_code );
                for ( int i = 0; i < can_msg_rx.data_length_code; i++ ){
                    printf( "%02x", can_msg_rx.data[i] );
                }
                printf("\r");
            }

            switch ( can_msg_rx.identifier ) {
                // Battery energy (2Hz)
                case 0x5BC: {
                    // Extract the 10 first bits of the message (byte[0] + 2 first bits of byte[1])
                    int gids = ( can_msg_rx.data[0] << 2 ) | ( can_msg_rx.data[1] >> 6 );

                    float energy = (float) gids * KWH_PER_GID;
                    
                    // Only consider valid values. It is sometimes 81.84kWh (1023 gids) right after switching on the car.
                    if (energy < 81) {
                        send_msg(Message_name::battery_energy_kwh, energy);
                    }
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

                    last_speed_update = millis();

                    send_msg(Message_name::speed_kmh, speed);
                    break;
                }

                // Climate control status (10Hz)
                case 0x54b: {
                    if ( ( can_msg_rx.data[1] & 0x40 ) == 0 ) {
                        send_msg(Message_name::ac_status, Message_status::ac_is_off);
                    }
                    else {
                        send_msg(Message_name::ac_status, Message_status::ac_is_on);
                    }
                }

                // Charger state
                case 0x390: {
                    float max_amps = can_msg_rx.data[6] * 0.5;

                    send_msg(Message_name::charger_max_amps, max_amps);

                    switch ( can_msg_rx.data[5] ) {
                        case 0x80:
                        case 0x82:
                        case 0x92:
                            send_msg(Message_name::charger_status, Message_status::charger_idle);
                            car_is_plugged_in = false;

                            break;

                        case 0x83:
                            send_msg(Message_name::charger_status, Message_status::charger_quick_charging);
                            car_is_plugged_in = true;

                            break;

                        case 0x98:
                            send_msg(Message_name::charger_status, Message_status::charger_plugged_in_timer_wait);
                            car_is_plugged_in = true;

                            break;

                        case 0x88:
                            send_msg(Message_name::charger_status, Message_status::charger_charging);
                            car_is_plugged_in = true;

                            break;

                        case 0x84:
                            send_msg(Message_name::charger_status, Message_status::charger_finished);
                            car_is_plugged_in = true;

                            break;
                    }
                }

                default:
                    break;
            }
        }

        // Update car status (on/off) every 200ms
        if (millis() - last_car_on_off_update > 200) {
            // Speed (from motor RPM) is updated at 100Hz when the car is on
            if (millis() - last_speed_update < 100) {
                send_msg(Message_name::car_status, Message_status::car_is_on);
            }
            else {
                send_msg(Message_name::car_status, Message_status::car_is_off);
            }

            last_car_on_off_update = millis();
        }

        // Write messages to CAN bus (or toggle slcan)
        Message received_msg;
        if ( xQueueReceive(q_leafcan, &received_msg, 0) == pdTRUE ) {
            can_message_t can_msg_tx;

            switch (received_msg.name) {
                case Message_name::ac_request:
                    // TODO: check if CC stops after a while and what happens if the car is getting unplugged while CC is on
                    if (received_msg.value_status == Message_status::request_ac_start) {
                        // Start climate control
                        can_msg_tx.identifier = 0x56e;
                        can_msg_tx.data[0] = 0x4e;
                        can_msg_tx.data[1] = 0x08;
                        can_msg_tx.data[2] = 0x12;
                        can_msg_tx.data[3] = 0x00;
                        can_msg_tx.data_length_code = 4;
                        can_msg_tx.flags = CAN_MSG_FLAG_NONE;

                        write_to_canbus(can_msg_tx);
                        
                        // if (!car_is_plugged_in) {
                        //     // Auto disable climate control
                        //     can_msg_tx.data[0] = 0x46;
                        //     can_msg_tx.data[1] = 0x08;
                        //     can_msg_tx.data[2] = 0x32;
                        //     can_msg_tx.data[3] = 0x00;
                        // }
                    }

                    else if (received_msg.value_status == Message_status::request_ac_stop) {
                        can_msg_tx.identifier = 0x56e;
                        can_msg_tx.data[0] = 0x56;
                        can_msg_tx.data[1] = 0x00;
                        can_msg_tx.data[2] = 0x01;
                        can_msg_tx.data[3] = 0x00;
                        can_msg_tx.data_length_code = 4;
                        can_msg_tx.flags = CAN_MSG_FLAG_NONE;
                        
                        write_to_canbus(can_msg_tx);
                    }
                    
                    break;

                case Message_name::charge_request:
                    if (received_msg.value_status == Message_status::request_charge_start) {
                        can_msg_tx.identifier = 0x56e;
                        can_msg_tx.data[0] = 0x66;
                        can_msg_tx.data[1] = 0x08;
                        can_msg_tx.data[2] = 0x12;
                        can_msg_tx.data[3] = 0x00;
                        can_msg_tx.data_length_code = 4;
                        can_msg_tx.flags = CAN_MSG_FLAG_NONE;
                        
                        write_to_canbus(can_msg_tx);
                    }
                    break;

                // TODO: check this, it doesn't work. Might need the CAR CAN bus.
                case Message_name::doors_request:
                    if (received_msg.value_status == Message_status::request_doors_lock) {
                        can_msg_tx.identifier = 0x56e;
                        can_msg_tx.data[0] = 0x60;
                        can_msg_tx.data[1] = 0x80;
                        can_msg_tx.data[2] = 0x00;
                        can_msg_tx.data[3] = 0x00;
                        can_msg_tx.data_length_code = 4;
                        can_msg_tx.flags = CAN_MSG_FLAG_NONE;
                        
                        write_to_canbus(can_msg_tx);
                    }

                    else if (received_msg.value_status == Message_status::request_doors_unlock) {
                        can_msg_tx.identifier = 0x56e;
                        can_msg_tx.data[0] = 0x11;
                        can_msg_tx.data[1] = 0x00;
                        can_msg_tx.data[2] = 0x00;
                        can_msg_tx.data[3] = 0x00;
                        can_msg_tx.data_length_code = 4;
                        can_msg_tx.flags = CAN_MSG_FLAG_NONE;
                        
                        write_to_canbus(can_msg_tx);
                    }
                    break;

                case Message_name::toggle_slcan:
                    slcan_enabled = slcan_enabled ? false : true;
                    break;

                default:
                    break;
            }
        }
    }
}
