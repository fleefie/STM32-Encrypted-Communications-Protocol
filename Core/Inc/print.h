#pragma once

#include <stddef.h>
#include <stdint.h>
#include "protocol_icd.h"

void print(char *msg);
void print_payload(uint8_t *data, size_t length);

void print_debug(char* msg);
void print_debug_u8(uint8_t num);
void print_debug_u16(uint16_t num);
void print_debug_u32(uint32_t num);
void print_debug_payload(uint8_t *data, size_t len);
void print_debug_payload_ascii(uint8_t *data, size_t len);
void print_debug_header(MessageHeader_t *header);
void print_debug_message(Message_t *msg);
