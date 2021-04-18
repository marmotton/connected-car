#ifndef GLOBALS_H
#define GLOBALS_H

enum Message_name {
    invalid,

    ac_status,
    charger_status,
    car_status,
    logger_status,

    charger_max_amps,

    network_status,
    network_latitude,
    network_longitude,

    gsm_day,
    gsm_month,
    gsm_year,
    gsm_hours,
    gsm_minutes,
    gsm_seconds,

    gnss_latitude,
    gnss_longitude,
    gnss_altitude,
    gnss_speed,

    battery_power_kw,
    battery_energy_kwh,
    speed_kmh,

    ac_request,
    charge_request,
    update_request,

    power_save_mode,

    pressure,
    pcb_temperature,
    pressure_altitude,
};

enum Message_status {
    invalid_status = 0,

    charger_idle = 1,
    charger_plugged_in_timer_wait = 2,
    charger_charging = 3,
    charger_quick_charging = 4,
    charger_finished = 5,
    request_charge_start = 6,

    ac_is_on = 7,
    ac_is_off = 8,

    car_is_on = 9,
    car_is_off = 10,

    request_ac_start,
    request_ac_stop,

    network_not_connected,
    network_connected,
    network_connected_mqtt,

    logger_write_started,
    logger_write_ended,
};

struct Message {
    enum Message_name name;
    union {
        float value_float;
        int value_int;
        enum Message_status value_status;
    };
};

extern QueueHandle_t q_out;
extern QueueHandle_t q_leafcan;
extern QueueHandle_t q_display;
extern QueueHandle_t q_logger;
extern QueueHandle_t q_comm_gnss;

#endif
