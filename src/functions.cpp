#include "functions.h"
#include <Arduino.h>
#include "globals.h"

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


// Exponential smoothing
void exp_smooth(float *out, float in, float smoothing ) {
    *out = smoothing * *out + (1 - smoothing) * in;
}


// Sending a message on the q_out queue
void send_msg(Message msg_out) {
    xQueueSendToBack(q_out, &msg_out, 0);
}
void send_msg(Message_name msg_name, float val) {
    Message msg_out;

    msg_out.name = msg_name;
    msg_out.value_float = val;

    send_msg(msg_out);
}
void send_msg(Message_name msg_name, int val) {
    Message msg_out;

    msg_out.name = msg_name;
    msg_out.value_int = val;

    send_msg(msg_out);
}
void send_msg(Message_name msg_name, Message_status val) {
    Message msg_out;

    msg_out.name = msg_name;
    msg_out.value_status = val;

    send_msg(msg_out);
}
