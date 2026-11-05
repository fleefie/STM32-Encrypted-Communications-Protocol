#pragma once

#include <stdint.h>
#include <stddef.h>

#include "protocol_icd.h"

typedef enum
{
    ORIGIN_NONE = 0,
    ORIGIN_USART1,
    ORIGIN_USART2,
    ORIGIN_USART3
} Send_Buffer_Origin_t;

typedef struct
{
    volatile uint8_t encryption_key[KEY_BYTE_SIZE];

    __attribute__((aligned(sizeof(uint16_t))))
        volatile uint8_t buff_send_usart1[MESSAGE_BYTE_LENGTH];
    __attribute__((aligned(sizeof(uint16_t))))
        volatile uint8_t buff_send_usart3[MESSAGE_BYTE_LENGTH];

    volatile uint8_t waiting_on_send_usart1;
    volatile uint8_t waiting_on_send_usart3;

    volatile uint8_t waiting_on_ack_for_usart1;
    volatile uint32_t waiting_on_ack_for_usart1_since;

    volatile uint8_t waiting_on_ack_for_usart3;
    volatile uint32_t waiting_on_ack_for_usart3_since;

    volatile Send_Buffer_Origin_t buff_send_usart1_origin;

    __attribute__((aligned(sizeof(uint16_t))))
        volatile Message_t message_to_process_usart1;
    volatile uint8_t message_needs_processing_usart1;

    __attribute__((aligned(sizeof(uint16_t))))
        volatile Message_t message_to_process_usart3;
    volatile uint8_t message_needs_processing_usart3;

    __attribute((aligned(sizeof(uint16_t))))
        volatile uint8_t rawdata_to_process[1];
    volatile uint8_t rawdata_needs_processing;

    volatile uint8_t is_last_node;
    volatile uint8_t is_first_node;
    volatile uint8_t waiting_on_final_ack;
    volatile uint8_t is_init;
    volatile uint8_t sent_init;
    volatile uint8_t is_error;

    volatile uint8_t usb_input_index;

    volatile uint32_t led_next_action;
    volatile uint32_t led_next_action_offset;
} State_t;

extern State_t State;

void reset_state();
