#include <stdio.h>
#include <string.h>

#include "print.h"
#include "main.h"
#include "protocol_icd.h"

void print(char* msg)
{
    size_t len = strlen(msg);
    HAL_UART_Transmit(&huart2, (uint8_t*)msg ,len, 100);
}

void print_payload(uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        char ch[2] = {data[i], '\0'};
        print(ch);
    }

}

#ifdef DEBUG

void print_debug(char* msg)
{
    size_t len = strlen(msg);
    HAL_UART_Transmit(&huart2, (uint8_t*)msg ,len, 100);
}

void print_debug_u8(uint8_t num)
{
    char num_ascii[3] = {0};
    sprintf(num_ascii, "%x", num);
    print_debug(num_ascii);
}

void print_debug_u16(uint16_t num)
{
    char num_ascii[5] = {0};
    sprintf(num_ascii, "%x", num);
    print_debug(num_ascii);
}

void print_debug_u32(uint32_t num)
{
    char num_ascii[9] = {0};
    sprintf(num_ascii, "%lx", num);
    print_debug(num_ascii);
}

void print_debug_payload(uint8_t *data, size_t len)
{
    if (len > MAX_PAYLOAD_BYTE_SIZE)
    {
        print("[Debug] Length is larger than 16 bytes, only printing 16 bytes!\r\n");
        len = MAX_PAYLOAD_BYTE_SIZE;
    }

    for (size_t i = 0; i < len; i++)
    {
        print_debug_u8(data[i]);
        print_debug(" ");
    }
}

void print_debug_payload_ascii(uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        char ch[2] = {data[i], '\0'};
        print_debug(ch);
    }
}

void print_debug_header(MessageHeader_t *header)
{
    print_debug("Start byte: "); print_debug_u8(header->start_byte); print_debug("\r\n");
    print_debug("Command type: "); print_debug_u16(header->command_type); print_debug("\r\n");
    print_debug("Source ID: "); print_debug_u32(header->id_src); print_debug("\r\n");
    print_debug("Payload length: "); print_debug_u8(header->length); print_debug("\r\n");
}

void print_debug_message(Message_t *msg)
{
    print_debug_header(&msg->header);
    print_debug("\r\nCRC16: ");
    print_debug_u16(msg->checksum);
    print_debug("\r\nPayload: ");
    print_debug_payload(msg->payload, msg->header.length);
    print_debug("\r\n");
}

#else
void print_debug(char* msg) {};
void print_debug_u8(uint8_t num) {};
void print_debug_u16(uint16_t num) {};
void print_debug_u32(uint32_t num) {};
void print_debug_payload(uint8_t *data, size_t len) {};
void print_debug_payload_ascii(uint8_t *data, size_t len) {};
void print_debug_header(MessageHeader_t *header) {};
void print_debug_message(Message_t *msg) {};
#endif
