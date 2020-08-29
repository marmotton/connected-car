#include <Arduino.h>

#include "globals.h"
#include "display.h"
#include "config.h"

#include <TFT_eSPI.h>

void display_task( void *parameter ) {
    TFT_eSPI tft = TFT_eSPI();
    TFT_eSprite img(&tft);

    tft.init();
    tft.setRotation(3);
    tft.fillScreen( TFT_BLACK );
    
    img.setColorDepth(8);
    img.createSprite( TFT_HEIGHT, TFT_WIDTH );

    float battery_kwh = 0;
    float speed = 0;
    float power = 0;

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
                    speed = SPEED_SMOOTHING * speed + (1 - SPEED_SMOOTHING) * received_msg.value_float;  // Exponential smoothing
                    break;

                case Message_name::battery_power_kw:
                    power = POWER_SMOOTHING * power + (1 - POWER_SMOOTHING) * received_msg.value_float;  // Exponential smoothing
                    break;
                
                default:
                    break;
            }
        }
        
        // Update display
        if ( millis() - last_display_update > 40 ) {
            last_display_update = millis();
            
            img.fillSprite( TFT_BLACK );

            // *******************
            // Speed display
            // *******************
            img.setFreeFont( &FreeSansBold24pt7b );
            img.setTextDatum( MR_DATUM );
            img.setTextColor( TFT_WHITE );
            img.drawNumber(speed, 85, 16);

            img.setFreeFont(&FreeSans9pt7b);
            img.setTextDatum( ML_DATUM );
            img.drawString( "km / h", 95, 16 );


            // *******************
            // Economy display (kWh/100km)
            // *******************
            img.setFreeFont( &FreeSansBold12pt7b );
            img.setTextDatum( MR_DATUM );

            // Green for charging, white for discharging
            if ( power > 0 ) {
                // Charging
                img.setTextColor( TFT_GREEN );
            }
            else {
                img.setTextColor( TFT_WHITE );
            }

            // Do not compute economy for very low speeds (and avoid divide by 0)
            if ( speed > 1 ) {
                float kwh_100km = power / speed * 100;
                img.drawFloat( std::abs(kwh_100km), 1, 44, 58);
            }
            else {
                img.drawString("---", 44, 58);
            }

            img.setTextFont(1);
            img.setTextDatum( ML_DATUM );
            img.setTextColor( TFT_WHITE );
            img.drawString("kWh", 53, 55);
            img.drawString("100km", 47, 65);
            img.drawLine(49, 59, 74, 59, TFT_WHITE);


            // *******************
            // Battery kWh display
            // *******************
            img.setTextColor( TFT_WHITE );
            img.setFreeFont( &FreeSansBold12pt7b );
            img.setTextDatum( MR_DATUM );
            img.drawFloat(battery_kwh, 1, 130, 58);

            img.setFreeFont( &FreeSans9pt7b );
            img.setTextFont(1);
            img.setTextDatum( ML_DATUM );
            img.drawString("kWh", 133, 65);


            // *******************
            // Battery kW display
            // *******************
            img.setFreeFont( &FreeSansBold12pt7b );
            img.setTextDatum( MR_DATUM );
            if ( power > 0 ) {
                // Charging
                img.setTextColor( TFT_GREEN );
            }
            else {
                img.setTextColor( TFT_WHITE );
            }

            img.drawFloat( std::abs(power), 1, 130, 90 );

            img.setTextColor( TFT_WHITE );
            img.setFreeFont( &FreeSans9pt7b );
            img.setTextDatum( ML_DATUM );
            img.drawString( "kW", 133, 90 );


            img.pushSprite(0, 0);
        }

        // Slow down the task
        else {
            delay(10);
        }
    }
}
