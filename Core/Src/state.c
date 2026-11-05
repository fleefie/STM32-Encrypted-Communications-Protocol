#include "state.h"
#include <string.h>

const State_t Default_State =
{
    .encryption_key = { 0xca, 0xfe, 0xba, 0xbe },

    .buff_send_usart1 = {0},
    .buff_send_usart3 = {0},

    .waiting_on_send_usart1 = 0U,
    .waiting_on_send_usart3 = 0U,

    .waiting_on_ack_for_usart1 = 0U,
    .waiting_on_ack_for_usart1_since = 0U,
    .waiting_on_ack_for_usart3 = 0U,

    .waiting_on_ack_for_usart3_since = 0U,

    .buff_send_usart1_origin = ORIGIN_NONE,

    .message_to_process_usart1 = {},
    .message_needs_processing_usart1 = 0U,

    .message_to_process_usart3 = {},
    .message_needs_processing_usart3 = 0U,

    .rawdata_to_process = {0},
    .rawdata_needs_processing = 0U,

    .is_last_node = 0U,
    .is_first_node = 0U,
    .waiting_on_final_ack = 0U,
    .is_init = 0U,
    .sent_init = 0U,
    .is_error = 0U,

    .usb_input_index = 0U,

    .led_next_action = 0U,
    .led_next_action_offset = 0U,
};

State_t State = Default_State;

void reset_state()
{
    memcpy(&State, &Default_State, sizeof(State_t));
}
