#include <Arduino.h>
#include <globals.h>
#include <functions.h>

#include <Wire.h>
#include <math.h>

#define PRESSURE_SLAVE_ADDR 0x77

uint8_t write_register(uint8_t addr, uint8_t val) {
    Wire.beginTransmission(PRESSURE_SLAVE_ADDR);
    Wire.write(addr); // "Reset" register
    Wire.write(val); // Soft-reset
    Wire.endTransmission();

    return 0;
}

uint8_t read_register(uint8_t addr) {
    Wire.beginTransmission(PRESSURE_SLAVE_ADDR);
    Wire.write(addr);
    Wire.endTransmission(false);
    Wire.requestFrom(PRESSURE_SLAVE_ADDR, 1);

    uint8_t val = Wire.read();

    return val;
}

void pressure_task( void *parameter ) {
    const int slave_address = 0x77;

    // Wait a little for debugging so that Serial is available for printing
    delay(1000);

    // Initialize i2c
    Wire.begin(21, 22);

    // Reset the sensor
    write_register(0x0C, 0x09); // Soft reset

    // Wait until sensor is ready
    // Right after reset, MEAS_CFG will contain 255, then 0, and finally 192 when both the coefficients and sensor are ready
    for (;;) {
        uint8_t meas_cfg = read_register(0x08);
        if (meas_cfg == 192) break;
        delay(10);
    }

    // Set to "high precision" mode
    write_register(0x06, 0x06); // Pressure 64x oversampling, 1 measurement/s
    write_register(0x07, 0x80); // Temperature sensor on MEMS, 1x oversampling, 1 measurement/s
    write_register(0x09, 0x04); // Enable pressure results bit-shift in data register (needed for >8 oversampling)

    // Scale factors (see table 4 in datasheet)
    const int scale_factor_p = 1040384;  // 64x oversampling
    const int scale_factor_t = 524288;  // no oversampling

    // Read calibration coefficients register
    Wire.beginTransmission(slave_address);
    Wire.write(0x10); // COEF register
    Wire.endTransmission(false);
    Wire.requestFrom(slave_address, 18); // read 18 bytes

    uint8_t coef_register[18];

    for (int i = 0; i < 18; i++) {
        coef_register[i] = Wire.read();
    }

    // Assemble bits of calibration coefficients
    int c0 = twosComplementToInt(
        (coef_register[0] << 4) | (coef_register[1] >> 4),
        12
    );
    int c1 = twosComplementToInt(
        ((coef_register[1] & 0x0F) << 8) | coef_register[2],
        12
    );
    int c00 = twosComplementToInt(
        (coef_register[3] << 12) | (coef_register[4] << 4) | (coef_register[5] >> 4),
        20
    );
    int c10 = twosComplementToInt(
        ((coef_register[5] & 0x0F) << 16) | (coef_register[6] << 8) | coef_register[7],
        20
    );
    int c01 = twosComplementToInt(
        (coef_register[8] << 8) | coef_register[9],
        16
    );
    int c11 = twosComplementToInt(
        (coef_register[10] << 8) | coef_register[11],
        16
    );
    int c20 = twosComplementToInt(
        (coef_register[12] << 8) | coef_register[13],
        16
    );
    int c21 = twosComplementToInt(
        (coef_register[14] << 8) | coef_register[15],
        16
    );
    int c30 = twosComplementToInt(
        (coef_register[16] << 8) | coef_register[17],
        16
    );

    // Start measuring
    write_register(0x08, 0x07); // Continous pressure and temperature measurement

    for (;;) {
        // Check if new measurements are available
        uint8_t meas_cfg = read_register(0x08);

        bool temperature_ready = meas_cfg & 0x20;
        bool pressure_ready = meas_cfg & 0x10;

        if (pressure_ready && temperature_ready) {
            Wire.beginTransmission(slave_address);
            Wire.write(0x00); // First of 6 registers containing measurements
            Wire.endTransmission(false);
            Wire.requestFrom(slave_address, 6);

            // Read 24bits for pressure (MSB is read first)
            uint32_t pressure_bits = 0;
            for (int i = 0; i < 3; i++) {
                pressure_bits |= Wire.read() << ((2 - i)*8);
            }

            // Read 24 bits for temperature (MSB is read first)
            uint32_t temperature_bits = 0;
            for (int i = 0; i < 3; i++) {
                temperature_bits |= Wire.read() << ((2 - i)*8);
            }

            // Scale raw values
            float p_raw_sc = (float)twosComplementToInt(pressure_bits, 24) / (float)scale_factor_p;
            float t_raw_sc = (float)twosComplementToInt(temperature_bits, 24) / (float)scale_factor_t;

            // Convert to real-world values
            float pressure_Pa = c00 + p_raw_sc * (c10 + p_raw_sc * (c20 + p_raw_sc * c30)) + t_raw_sc * c01 + t_raw_sc + p_raw_sc * (c11 + p_raw_sc * c21);
            float temperature_degC = c0 * 0.5 + c1 * t_raw_sc;
            float altitude_m = 44330 * (1 - pow(pressure_Pa / 101325, 1/5.255));

            send_msg(Message_name::pressure, pressure_Pa);
            send_msg(Message_name::pcb_temperature, temperature_degC);
            send_msg(Message_name::pressure_altitude, altitude_m);
        }

        delay(200);
    }
}
