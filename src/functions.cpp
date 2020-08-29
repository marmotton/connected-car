#include "functions.h"
#include <Arduino.h>

// Function that converts a two's complement n bits number into a signed int
int twosComplementToInt(uint32_t twosComplement, uint32_t nBits) {
    int integer;

    // Check if the number is negative (first bit = 1)
    if ( ( twosComplement >> ( nBits - 1 ) ) & 1 ) {
        // Fill the unused MSBs with 1 (e.g. the first 13 bits if nBits = 19)
        twosComplement = twosComplement | (0xFFFFFFFF << nBits );
        // Add 1, invert the bits, make it negative
        integer = -( ~( twosComplement + 1 ) );
    }
    // Positive number
    else {
        integer = twosComplement;
    }

    return integer;
}