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
    TFT_eSprite spr_big(&tft);
    TFT_eSprite spr_small(&tft);
    TFT_eSprite spr_icons(&tft);

    tft.init();
    tft.setRotation(3);
    tft.fillScreen( TFT_BLACK );
    
    spr_big.setColorDepth(16);
    spr_big.createSprite( TFT_HEIGHT, 72 );
    spr_big.loadFont( "NotoSansBold36" );

    spr_small.setColorDepth(16);
    spr_small.createSprite( TFT_HEIGHT, 40 );
    spr_small.setTextFont(2);

    spr_icons.setColorDepth(16);
    spr_icons.createSprite( TFT_HEIGHT, 16 );
    spr_icons.loadFont( "NotoSansBold15" );
    

    float battery_kwh = 22.87543;
    float speed = 128.721;
    float last_speed = 0;
    float acceleration = 0.8;
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
                    exp_smooth(&acceleration, ( speed - last_speed ) * 100, DISPLAY_SMOOTHING);  // 100Hz message --> km/h / s
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
            spr_big.fillSprite( TFT_BLACK );
            spr_big.setTextDatum( TC_DATUM );
            spr_big.setTextColor( TFT_WHITE, TFT_BLACK );
            spr_big.drawNumber(speed, 80, 0);


            // *******************
            // Acceleration display
            // *******************
            if (acceleration >= 0) {
                int bar_length = acceleration / 5 * 78;
                bar_length = bar_length > 78 ? 78 : bar_length;
                spr_big.fillRect(78, 32, bar_length + 4, 5, TFT_SKYBLUE);
            }
            else {
                int bar_length = -acceleration / 5 * 78;
                bar_length = bar_length > 78 ? 78 : bar_length;
                spr_big.fillRect(78 - bar_length, 32, bar_length + 4, 5, TFT_SKYBLUE);
            }

            spr_big.fillRect(78, 31, 4, 7, TFT_SKYBLUE);  // Center mark


            // *******************
            // Economy display (kWh/100km)
            // *******************
            spr_big.setTextDatum( MC_DATUM );

            // Green for charging, white for discharging
            if ( power > 0 ) {
                // Charging
                spr_big.setTextColor( TFT_GREEN, TFT_BLACK );
            }
            else {
                spr_big.setTextColor( TFT_WHITE, TFT_BLACK );
            }

            // Do not compute economy for very low speeds (and avoid divide by 0)
            if ( speed > 1 ) {
                float kwh_100km = power / speed * 100;
                spr_big.drawNumber( std::abs(kwh_100km), 40, 56);
            }
            else {
                spr_big.drawString("---", 40, 56);
            }


            // *******************
            // Battery kW display
            // *******************
            spr_big.setTextDatum( MC_DATUM );
            if ( power > 0 ) {
                // Charging
                spr_big.setTextColor( TFT_GREEN, TFT_BLACK );
            }
            else {
                spr_big.setTextColor( TFT_WHITE, TFT_BLACK );
            }

            // Show decimal only while charging
            if ( charger_status == Message_status::charger_charging ) {
                spr_big.drawFloat( std::abs(power), 1, 120, 56 );
            }
            else {
                spr_big.drawNumber( std::abs(power), 120, 56 );
            }
            

            spr_big.setTextColor( TFT_WHITE, TFT_BLACK );
            spr_big.setTextDatum( ML_DATUM );


            // *******************
            // Units
            // *******************
            spr_small.fillSprite( TFT_BLACK );
            spr_small.setTextColor( TFT_SKYBLUE, TFT_BLACK );
            spr_small.setTextDatum( TC_DATUM );
            spr_small.drawString("kWh/100km", 40, 0);
            spr_small.drawString("kW", 120, 0);


            // *******************
            // Battery kWh display
            // *******************
            spr_small.setTextDatum( MR_DATUM );
            spr_small.drawFloat(battery_kwh, 1, 123, 30);

            spr_small.setTextDatum( ML_DATUM );
            spr_small.drawString("kWh", 126, 30);


            // *******************
            // Spare
            // *******************
            spr_small.setTextDatum( MC_DATUM );
            spr_small.drawString("---", 40, 30);


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
            spr_big.pushSprite(0, 0);
            spr_small.pushSprite(0, 72);
            spr_icons.pushSprite(0, 112);
        }

        // Slow down the task
        else {
            delay(10);
        }
    }
}
