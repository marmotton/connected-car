#include <Arduino.h>
#include <FS.h>
#include <SD.h>

#include "sdcard_logger.h"

#include "globals.h"
#include "config.h"
#include "functions.h"

const int SD_CS = 4;


void logger_task( void *parameter ) {

    float battery_kwh = 0;
    float battery_kw = 0;
    float speed_tacho = 0;
    float speed_gnss = 0;
    float latitude = 0;
    float longitude = 0;
    float altitude = 0;
    float latitude_network = 0;
    float longitude_network = 0;

    unsigned long last_log_time = 0;

    for(;;) {
        // Empty the queue and update local variables
        Message received_msg;
        while ( xQueueReceive(q_logger, &received_msg, 0 ) == pdTRUE ) {
            switch ( received_msg.name ) {
                case Message_name::speed_kmh:
                    exp_smooth(&speed_tacho, received_msg.value_float, LOG_SMOOTHING);
                    break;

                case Message_name::gnss_speed:
                    speed_gnss = received_msg.value_float;
                    break;

                case Message_name::gnss_latitude:
                    latitude = received_msg.value_float;
                    break;

                case Message_name::gnss_longitude:
                    longitude = received_msg.value_float;
                    break;

                case Message_name::gnss_altitude:
                    altitude = received_msg.value_float;
                    break;

                case Message_name::network_latitude:
                    latitude_network = received_msg.value_float;
                    break;

                case Message_name::network_longitude:
                    longitude_network = received_msg.value_float;
                    break;

                case Message_name::battery_power_kw:
                    exp_smooth(&battery_kw, received_msg.value_float, LOG_SMOOTHING);
                    break;

                case Message_name::battery_energy_kwh:
                    battery_kwh = received_msg.value_float;
                    break;
                
                default:
                    break;
            }
        }

        // Log every second
        if ( millis() - last_log_time > 1000 ) {

            last_log_time = millis();

            Message msg_out;
            msg_out.name = Message_name::logger_status;

            if ( SD.begin(SD_CS) ) {
                msg_out.value_status = Message_status::logger_write_started;
                xQueueSendToBack(q_out, &msg_out, 1);

                bool log_exists = SD.exists("/log.csv");

                File logfile = SD.open("/log.csv", FILE_APPEND);

                if ( !log_exists ) {
                    logfile.println("time (ms),speed (km/h),GNSS speed (km/h),latitude (deg),longitude (deg),altitude (m),network latitude (deg),network longitude (deg),battery power (kW),battery energy (kWh)");
                }

                logfile.printf("%lu,%.1f,%.1f,%.6f,%.6f,%.1f,%.6f,%.6f,%.2f,%.2f\n",
                    millis(), speed_tacho, speed_gnss,
                    latitude, longitude, altitude,
                    latitude_network, longitude_network,
                    battery_kw, battery_kwh);

                logfile.close();
            }

            SD.end();

            msg_out.value_status = Message_status::logger_write_ended;
            xQueueSendToBack(q_out, &msg_out, 1);
        }

        // Slow down the task
        delay(100);
    }
}
