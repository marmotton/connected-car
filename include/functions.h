#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <Arduino.h>
#include "globals.h"

// Function that converts a two's complement n bits number into a signed int
int twosComplementToInt(uint32_t twosComplement, uint32_t nBits);

void exp_smooth(float *out, float in, float smoothing );

void send_msg(Message msg_out);
void send_msg(Message_name msg_name, float val);
void send_msg(Message_name msg_name, int val);
void send_msg(Message_name msg_name, Message_status val);

#endif