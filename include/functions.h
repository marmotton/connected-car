#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <Arduino.h>

// Function that converts a two's complement n bits number into a signed int
int twosComplementToInt(uint32_t twosComplement, uint32_t nBits);

void exp_smooth(float *out, float in, float smoothing );

#endif