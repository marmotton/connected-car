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
    int gsm_day = 0;
    int gsm_month = 0;
    int gsm_year = 0;
    int gsm_hours = 0;
    int gsm_minutes = 0;
    int gsm_seconds = 0;

    Message_status car_status = Message_status::car_is_off;
    Message_status charger_status = Message_status::charger_idle;

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

                case Message_name::gsm_year:
                    gsm_year = received_msg.value_int;
                    break;
                
                case Message_name::gsm_month:
                    gsm_month = received_msg.value_int;
                    break;
                
                case Message_name::gsm_day:
                    gsm_day = received_msg.value_int;
                    break;
                
                case Message_name::gsm_hours:
                    gsm_hours = received_msg.value_int;
                    break;
                
                case Message_name::gsm_minutes:
                    gsm_minutes = received_msg.value_int;
                    break;
                
                case Message_name::gsm_seconds:
                    gsm_seconds = received_msg.value_int;
                    break;

                case Message_name::car_status:
                    car_status = received_msg.value_status;
                    break;

                case Message_name::charger_status:
                    charger_status = received_msg.value_status;
                    break;
                
                default:
                    break;
            }
        }

        // Log every 15min by default
        int log_interval = 15*60000;

        // Log more frequently when car is on or charging
        if (car_status == Message_status::car_is_on) {
            log_interval = 1000;
        }
        else if (charger_status == Message_status::charger_quick_charging) {
            log_interval = 10000;
        }
        else if (charger_status == Message_status::charger_charging) {
            log_interval = 60000;
        }

        // Log to SD-card
        if ( millis() - last_log_time > log_interval ) {

            last_log_time = millis();

            if ( SD.begin(SD_CS) ) {
                send_msg(Message_name::logger_status, Message_status::logger_write_started);

                bool log_exists = SD.exists("/log.csv");

                File logfile = SD.open("/log.csv", FILE_APPEND);

                if ( !log_exists ) {
                    logfile.println("time (ms),GSM date,GSM time,speed (km/h),GNSS speed (km/h),latitude (deg),longitude (deg),altitude (m),network latitude (deg),network longitude (deg),battery power (kW),battery energy (kWh)");
                }

                logfile.printf("%lu,%d/%d/%d,%d:%d:%d,%.1f,%.1f,%.6f,%.6f,%.1f,%.6f,%.6f,%.2f,%.2f\n",
                    millis(), gsm_year, gsm_month, gsm_day, gsm_hours, gsm_minutes, gsm_seconds,
                    speed_tacho, speed_gnss,
                    latitude, longitude, altitude,
                    latitude_network, longitude_network,
                    battery_kw, battery_kwh);

                logfile.close();
            }

            SD.end();

            send_msg(Message_name::logger_status, Message_status::logger_write_ended);
        }

        // Slow down the task
        delay(100);
    }
}
