#ifndef CONSTANTS_H
#define CONSTANTS_H

#define ENABLE_SLCAN
#define SERIAL_BAUDRATE 460800
#define ENABLE_SERIAL_DEBUG true

#define KWH_PER_GID 0.08
#define MOTOR_RPM_TO_KMH 0.01212

// Smoothing factors for power and speed reported by the car (exponential moving average)
#define DISPLAY_SMOOTHING 0.9
#define LOG_SMOOTHING 0.95

#endif
