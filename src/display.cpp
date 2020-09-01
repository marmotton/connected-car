#include <Arduino.h>

#include "globals.h"
#include "display.h"
#include "config.h"
#include "functions.h"

#include <TFT_eSPI.h>
#include <FS.h>

void display_task( void *parameter ) {
    SPIFFS.begin();

    TFT_eSPI tft = TFT_eSPI();
    TFT_eSprite spr_speed(&tft);
    TFT_eSprite spr_main(&tft);
    TFT_eSprite spr_icons(&tft);

    tft.init();
    tft.setRotation(3);
    tft.fillScreen( TFT_BLACK );
    
    spr_speed.setColorDepth(16);
    spr_speed.createSprite( TFT_HEIGHT, 32 );
    spr_speed.loadFont( "NotoSansBold36" );

    spr_main.setColorDepth(16);
    spr_main.createSprite( TFT_HEIGHT, 80 );
    spr_main.loadFont( "NotoSansBold15" );

    spr_icons.setColorDepth(16);
    spr_icons.createSprite( TFT_HEIGHT, 16 );
    spr_icons.loadFont( "NotoSansBold15" );
    

    float battery_kwh = 22.87543;
    float speed = 128.721;
    float last_speed = 0;
    float acceleration = 0;
    float power = 78.921;
    Message_status charger_status = Message_status::charger_idle;
    
    bool log_writing = false;
    unsigned long log_start_time = 0;

    int last_display_update = 0;

    for (;;) {
        // Empty the queue and update local variables
        Message received_msg;
        while ( xQueueReceive(q_display, &received_msg, 0 ) == pdTRUE ) {
            switch ( received_msg.name ) {
                case Message_name::battery_energy_kwh:
                    battery_kwh = received_msg.value_float;
                    break;

                case Message_name::speed_kmh:
                    exp_smooth(&speed, received_msg.value_float, DISPLAY_SMOOTHING);
                    // TODO: smooth acceleration
                    acceleration = ( speed - last_speed ) * 100;  // 100Hz message --> km/h / s
                    last_speed = speed;
                    break;

                case Message_name::battery_power_kw:
                    exp_smooth(&power, received_msg.value_float, DISPLAY_SMOOTHING);
                    break;

                case Message_name::logger_status:
                    if (received_msg.value_status == Message_status::logger_write_started) {
                        log_writing = true;
                        log_start_time = millis();
                    }
                    else if (received_msg.value_status == Message_status::logger_write_ended) {
                        log_writing = false;
                    }
                    break;

                case Message_name::charger_status:
                    charger_status = received_msg.value_status;
                    break;
                
                default:
                    break;
            }
        }
        
        // Update display
        if ( millis() - last_display_update > 40 ) {
            last_display_update = millis();

            // *******************
            // Speed display
            // *******************
            spr_speed.fillSprite( TFT_BLACK );
            spr_speed.setTextDatum( TC_DATUM );
            spr_speed.setTextColor( TFT_WHITE, TFT_BLACK );
            spr_speed.drawNumber(speed, 80, 0);


            // *******************
            // Acceleration display
            // *******************
            spr_main.fillSprite( TFT_BLACK );
            if (acceleration >= 0) {
                int bar_length = acceleration * 78;
                bar_length = bar_length > 78 ? 78 : bar_length;
                spr_main.fillRect(78, 0, bar_length + 4, 3, TFT_SKYBLUE);
            }
            else {
                int bar_length = -acceleration * 78;
                bar_length = bar_length > 78 ? 78 : bar_length;
                spr_main.fillRect(78 - bar_length, 0, bar_length + 4, 3, TFT_SKYBLUE);
            }


            // *******************
            // Economy display (kWh/100km)
            // *******************
            spr_main.setTextDatum( MR_DATUM );

            // Green for charging, white for discharging
            if ( power > 0 ) {
                // Charging
                spr_main.setTextColor( TFT_GREEN, TFT_BLACK );
            }
            else {
                spr_main.setTextColor( TFT_WHITE, TFT_BLACK );
            }

            // Do not compute economy for very low speeds (and avoid divide by 0)
            if ( speed > 1 ) {
                float kwh_100km = power / speed * 100;
                spr_main.drawNumber( std::abs(kwh_100km), 43, 16);
            }
            else {
                spr_main.drawString("---", 43, 16);
            }

            // TODO: kWh/100km icon


            // *******************
            // Battery kW display
            // *******************
            spr_main.setTextDatum( MR_DATUM );
            if ( power > 0 ) {
                // Charging
                spr_main.setTextColor( TFT_GREEN, TFT_BLACK );
            }
            else {
                spr_main.setTextColor( TFT_WHITE, TFT_BLACK );
            }

            // Show decimal only while charging
            if ( charger_status == Message_status::charger_charging ) {
                spr_main.drawFloat( std::abs(power), 1, 123, 16 );
            }
            else {
                spr_main.drawNumber( std::abs(power), 123, 16 );
            }
            

            spr_main.setTextColor( TFT_WHITE, TFT_BLACK );
            spr_main.setTextDatum( ML_DATUM );
            spr_main.drawString( "kW", 126, 16 );


            // *******************
            // Battery kWh display
            // *******************
            spr_main.setTextColor( TFT_WHITE, TFT_BLACK );
            spr_main.setTextDatum( MR_DATUM );
            spr_main.drawFloat(battery_kwh, 1, 123, 40);

            spr_main.setTextDatum( ML_DATUM );
            spr_main.drawString("kWh", 126, 40);


            // *******************
            // Spares
            // *******************
            spr_main.setTextDatum( MR_DATUM );
            spr_main.drawFloat( speed, 3, 43, 40);
            spr_main.drawFloat( acceleration, 3, 43, 64);
            spr_main.drawString( "---", 123, 64);


            // *******************
            // Icons
            // *******************
            spr_icons.fillSprite( TFT_BLACK );
            
            // Display the icon for at least 200ms
            if ( log_writing || millis() - log_start_time < 200 ) {
                spr_icons.setTextDatum( TL_DATUM );
                spr_icons.setTextColor(TFT_SKYBLUE, TFT_BLACK);
                spr_icons.drawString("W", 0, 0);
            }


            // *******************
            // Display on TFT
            // *******************
            spr_speed.pushSprite(0, 0);
            spr_main.pushSprite(0, 32);
            spr_icons.pushSprite(0, 112);
        }

        // Slow down the task
        else {
            delay(10);
        }
    }
}
