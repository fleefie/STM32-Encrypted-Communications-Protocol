#include "main.h"
#include "print.h"
#include "protocol.h"
#include "protocol_icd.h"
#include "state.h"
#include "stm32f4xx_hal.h"
#include <string.h>

Message_t make_ack()
{
    Message_t ack = 
    {
        .header =
        {
            .start_byte = ICD_START_BYTE,
            .length = 0,
            .command_type = CMD_ACK,
            .id_src = HAL_GetUIDw1(),
            .reserve = 0,
        },
        .payload = {0},
        .checksum = 0,
    };

    ack.checksum = calculate_crc16(ack.payload, 0);

    return ack;
}

uint8_t is_message_valid(Message_t *msg)
{
    uint16_t crc16 = calculate_crc16(msg->payload, msg->header.length);
    if (crc16 != msg->checksum)
    {
        print_debug("[Debug] Bad CRC!\r\n[Debug] Header CRC: ");
        print_debug_u16(msg->checksum);
        print_debug("\r\n[Debug] Computed CRC: ");
        print_debug_u16(crc16);
        print_debug("\r\n");
        return 0U;
    }

    switch (msg->header.command_type)
    {
        case CMD_ACK:
        case CMD_INIT:
        case CMD_ERROR:
        case CMD_RESET:
        case CMD_MSG:
            break;
        default:
            print_debug("[Debug] Bad command type!\r\n[Debug] Command type: ");
            print_debug_u8(msg->header.command_type);
            print_debug("\r\n");
            return 0U;
    }

    if (msg->header.length > MAX_PAYLOAD_BYTE_SIZE)
    {
            print_debug("[Debug] Payload too long!\r\n[Debug] Length: ");
            print_debug_u8(msg->header.length);
            print_debug("\r\n");
            return 0U;
    }

    if (msg->header.start_byte > ICD_START_BYTE)
    {
            print_debug("[Debug] Wrong start byte!\r\n[Debug] Start byte: ");
            print_debug_u8(msg->header.start_byte);
            print_debug("\r\n");
            return 0U;
    }

    return 1U;
}

uint16_t calculate_crc16(uint8_t *data, size_t length)
{
    uint16_t crc16 = 0;

    for (size_t i = 0; i < length; i++)
    {
        crc16 ^= data[i] << 8;
        for (uint16_t bit = 0; bit < 8; bit++)
            // polynomial values taken from arduino lib
            if (crc16 & 0x8000)
                crc16 = (crc16 << 1) ^ 0x1021;
            else
                crc16 = crc16 << 1;
    }
    return crc16;
}

void encrypt_decrypt(uint8_t *in, uint8_t *out, size_t length)
{
    for (size_t i = 0; i < length; i++)
        out[i] = in[i] ^ ((uint8_t*) State.encryption_key)[i & (KEY_BYTE_SIZE - 1)];
}

Message_t make_reset()
{
    Message_t reset = 
    {
        .header = 
        {
            .start_byte = ICD_START_BYTE,
            .command_type = CMD_RESET,
            .id_src = HAL_GetUIDw1(),
            .length = 0,
            .reserve = 0,
        },
        .payload = {0},
        .checksum = 0
    };

    return reset;
}

Message_t make_error()
{
    Message_t error = 
    {
        .header = 
        {
            .start_byte = ICD_START_BYTE,
            .command_type = CMD_ERROR,
            .id_src = HAL_GetUIDw1(),
            .length = 0,
            .reserve = 0,
        },
        .payload = {0},
        .checksum = 0
    };

    return error;
}

Message_t make_handshake()
{
    Message_t handshake = 
    {
        .header = 
        {
            .start_byte = ICD_START_BYTE,
            .command_type = CMD_INIT,
            .id_src = HAL_GetUIDw1(),
            .length = KEY_BYTE_SIZE,
            .reserve = 0,
        },
        .payload = {0},
        .checksum = 0
    };

    memcpy(handshake.payload, (void*) State.encryption_key, KEY_BYTE_SIZE);
    handshake.checksum = calculate_crc16(handshake.payload, KEY_BYTE_SIZE);

    return handshake;
}

void send_reset()
{
    print_debug("\r\nButton pressed: Reseting network!\r\n");
    reset_state();
    Message_t reset = make_reset();
    
    HAL_UART_Transmit(&huart1, (uint8_t*) &reset, MESSAGE_BYTE_LENGTH, 100);
    HAL_UART_Transmit(&huart3, (uint8_t*) &reset, MESSAGE_BYTE_LENGTH, 100);

    HAL_NVIC_SystemReset();
}

void send_error()
{
    print_debug("\r\nError! Propagating!!\r\n");
    reset_state();
    Message_t error = make_error();
    
    if (!State.is_last_node)
        HAL_UART_Transmit(&huart1, (uint8_t*) &error, MESSAGE_BYTE_LENGTH, 100);
    if (!State.is_first_node)
        HAL_UART_Transmit(&huart3, (uint8_t*) &error, MESSAGE_BYTE_LENGTH, 100);

    State.is_error = 1U;
}
