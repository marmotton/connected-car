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

    gnss_latitude,
    gnss_longitude,
    gnss_altitude,
    gnss_speed,

    battery_power_kw,
    battery_energy_kwh,
    speed_kmh,
    acceleration_kmh_s,

    ac_request,
    charge_request,

    power_save_mode,
};

enum Message_status {
    ac_is_on,
    ac_is_off,
    request_ac_start,
    request_ac_stop,

    charger_idle,
    charger_plugged_in_timer_wait,
    charger_charging,
    charger_finished,
    request_charge_start,

    car_is_on,
    car_is_off,

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

#endif
