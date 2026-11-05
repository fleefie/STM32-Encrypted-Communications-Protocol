#pragma once

#include <stddef.h>
#include <stdint.h>

#include "protocol_icd.h"

#define MAX_ACK_WAIT_TIME_MS 2000U

void encrypt_decrypt(uint8_t *in, uint8_t *out, size_t length);
uint16_t calculate_crc16(uint8_t *data, size_t length);

uint8_t is_message_valid(Message_t *msg);
Message_t make_ack();
Message_t make_reset();
Message_t make_error();
Message_t make_handshake();
void send_reset();
void send_error();
