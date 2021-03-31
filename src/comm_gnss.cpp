#include <Arduino.h>

#include <globals.h>
#include <functions.h>
#include <config.h>
#include <config_comm.h>

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_DEBUG Serial
#include <TinyGsmClient.h>
#include <PubSubClient.h>

#include <StreamDebugger.h>

void mqttCallback(char* topic, byte* payload, unsigned int len) {
    if (strcmp(topic, MQTT_CONTROL_TOPIC) == 0) {
        if (memcmp(payload, "poll", 4) == 0) {
            send_msg(Message_name::update_request);
        }
        else if (memcmp(payload, "start_ac", 8) == 0) {
            send_msg(Message_name::ac_request, Message_status::request_ac_start);
        }
        else if (memcmp(payload, "stop_ac", 7) == 0) {
            send_msg(Message_name::ac_request, Message_status::request_ac_stop);
        }
        else if (memcmp(payload, "start_charge", 12) == 0) {
            send_msg(Message_name::charge_request, Message_status::request_charge_start);
        }
    }
}

boolean mqttConnect(PubSubClient &mqtt) {
    boolean status = mqtt.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_PASS, MQTT_ONLINE_TOPIC, 1, true, "offline");

    if (status == false) {
        return false;
    }

    mqtt.publish(MQTT_ONLINE_TOPIC, "online", true);
    mqtt.subscribe(MQTT_CONTROL_TOPIC);

    return mqtt.connected();
}

void modemInit(TinyGsm &modem) {
    modem.restart();
    modem.setNetworkMode(51); // GSM and LTE
    modem.setPreferredMode(1); // Cat-M
    modem.waitForNetwork();
    modem.enableGPS();
}

void comm_gnss_task( void *parameter ) {
    StreamDebugger debugger(Serial1, Serial);
    TinyGsm modem(debugger);
    TinyGsmClientSecure client(modem);
    PubSubClient mqtt(client);

    Serial1.begin(115200, SERIAL_8N1, 26, 25);

    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);

    // Local storage of values
    float battery_power_kw = 0;
    float battery_energy_kwh = 0;
    float charger_max_amps = 0;
    float gnss_latitude = 0;
    float gnss_longitude = 0;
    float gnss_altitude = 0;
    float gnss_speed = 0;
    Message_status ac_status = Message_status::invalid_status;
    Message_status charger_status = Message_status::invalid_status;
    Message_status car_status = Message_status::invalid_status;
    Message_status network_status = Message_status::invalid_status;
    int gsm_day;
    int gsm_month;
    int gsm_year;
    int gsm_hours;
    int gsm_minutes;
    int gsm_seconds;

    int32_t lastReconnectAttempt = -999999; // Negative value to avoid waiting 10s before connecting at boot
    int32_t lastGNSSUpdate = 0;
    int32_t lastDateTimeUpdate = 0;
    int32_t lastMqttUpdate = 0;

    boolean updateRequestFlag = false;

    for(;;) {
        // Make sure we stay connected
        if (!mqtt.connected()) {
            // Reconnect every 10 seconds
            int32_t t = millis();

            if (t - lastReconnectAttempt > 10000L) {

                lastReconnectAttempt = t;

                if (!modem.isNetworkConnected()) {
                    modemInit(modem);
                }

                if (!modem.isGprsConnected()) {
                    modem.gprsConnect(GSM_APN, "", "");
                }

                if (mqttConnect(mqtt)) {
                    lastReconnectAttempt = 0;
                }
            }
            delay(100);
        }
        mqtt.loop();

        // Empty the queue and update local variables
        Message received_msg;
        while ( xQueueReceive(q_comm_gnss, &received_msg, 0 ) == pdTRUE ) {
            switch ( received_msg.name ) {
                case Message_name::battery_power_kw:
                    exp_smooth(&battery_power_kw, received_msg.value_float, MQTT_SMOOTHING);
                    break;

                case Message_name::battery_energy_kwh:
                    battery_energy_kwh = received_msg.value_float;
                    break;

                case Message_name::car_status:
                    car_status = received_msg.value_status;
                    break;

                case Message_name::ac_status:
                    ac_status = received_msg.value_status;
                    break;

                case Message_name::charger_status:
                    charger_status = received_msg.value_status;
                    break;

                case Message_name::charger_max_amps:
                    charger_max_amps = received_msg.value_float;
                    break;

                case Message_name::update_request:
                    updateRequestFlag = true;
                    break;

                default:
                    break;
            }
        }

        // Update GNSS
        if (millis() - lastGNSSUpdate > 500) {
            lastGNSSUpdate = millis();

            // TODO: Check units, datasheet says km/h, TinyGSM says knots
            float speed_knots = 0;
            modem.getGPS(&gnss_latitude, &gnss_longitude, &speed_knots, &gnss_altitude, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            gnss_speed = speed_knots * 1.852;

            send_msg(Message_name::gnss_latitude, gnss_latitude);
            send_msg(Message_name::gnss_longitude, gnss_longitude);
            send_msg(Message_name::gnss_speed, gnss_speed);
            send_msg(Message_name::gnss_altitude, gnss_altitude);
        }

        // Update date and time
        if (millis() - lastDateTimeUpdate > 1000) {
            lastDateTimeUpdate = millis();

            String gsm_date_time = modem.getGSMDateTime(TinyGSMDateTimeFormat::DATE_FULL);

            gsm_year = gsm_date_time.substring(0, 2).toInt();
            gsm_month = gsm_date_time.substring(3, 5).toInt();
            gsm_day = gsm_date_time.substring(6, 8).toInt();
            gsm_hours = gsm_date_time.substring(9, 11).toInt();
            gsm_minutes = gsm_date_time.substring(12, 14).toInt();
            gsm_seconds = gsm_date_time.substring(16, 18).toInt();

            send_msg(Message_name::gsm_year, gsm_year);
            send_msg(Message_name::gsm_month, gsm_month);
            send_msg(Message_name::gsm_day, gsm_day);
            send_msg(Message_name::gsm_hours, gsm_hours);
            send_msg(Message_name::gsm_minutes, gsm_minutes);
            send_msg(Message_name::gsm_seconds, gsm_seconds);
        }

        // Publish messages on MQTT
        // Default once an hour
        int publish_interval = 60*60000;

        // Publish more frequently when charging or when car is on
        if (charger_status == Message_status::charger_quick_charging) {
            publish_interval = 10000;
        }
        else if (charger_status == Message_status::charger_charging) {
            publish_interval = 10*60000;
        }
        else if (car_status == Message_status::car_is_on) {
            publish_interval = 60000;
        }

        if (millis() - lastMqttUpdate > publish_interval || updateRequestFlag) {
            updateRequestFlag = false;
            lastMqttUpdate = millis();

            mqtt.publish(MQTT_PREFIX "lat", String(gnss_latitude, 6).c_str());
            mqtt.publish(MQTT_PREFIX "lon", String(gnss_longitude, 6).c_str());
            mqtt.publish(MQTT_PREFIX "speed", String(gnss_speed, 1).c_str());
            mqtt.publish(MQTT_PREFIX "batteryKWH", String(battery_energy_kwh, 1).c_str());
            mqtt.publish(MQTT_PREFIX "batteryKW", String(battery_power_kw, 1).c_str());
            mqtt.publish(MQTT_PREFIX "chargerMaxAmps", String(charger_max_amps, 1).c_str());

            // Status: send the integer value of the Message_status enum
            mqtt.publish(MQTT_PREFIX "acStatus", String(ac_status).c_str());
            mqtt.publish(MQTT_PREFIX "chargerStatus", String(charger_status).c_str());
        }
        
        delay(10);
    }
}
