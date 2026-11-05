#include "io.h"

#include "main.h"
#include "protocol.h"
#include "state.h"
#include "print.h"
#include "stm32f4xx_hal.h"
#include <string.h>

const uint16_t bad_packet_code[16] = SECRET_MALFORMED_PACKET_CODE; 

// USB -> this
void handle_usb_input()
{
    if (State.rawdata_needs_processing == 1U)
    {
        // Initiate handshake if we're not already the first node!
        if (!State.is_first_node)
        {
            State.is_first_node = 1U;
            print("\r\n[Protocol] Got USB input, starting new handshake\r\n");
            print_debug("[Debug] Starting init handshake with this node as the first node!\r\n");

            Message_t msg = make_handshake();
            memcpy((uint8_t*) &State.buff_send_usart1, &msg, MESSAGE_BYTE_LENGTH);
            State.waiting_on_send_usart1 = 1U; 
            State.buff_send_usart1_origin = ORIGIN_USART2;
            State.rawdata_needs_processing = 0U;
            return;
        }

        // If we're waiting on the last node's reply, reject new inputs!
        if (State.waiting_on_final_ack)
        {
            print("\r\n[Protocol] Not ready to accept more inputs yet!\r\n");
            print_debug("[Debug] Rejecting input: final node didn't reply with the final ACK yet!\r\n");
            State.rawdata_needs_processing = 0U;
            HAL_UART_Receive_IT(&huart2, buff_usart2_input, 1);
            return;
        }

        // We got raw data and USART1 is available to send it down
        if (!State.waiting_on_send_usart1)
        {
            // THIS IS SO DIRTY OMG

            // if we didn't get \r and this isn't the 16th input
            if (*State.rawdata_to_process != '\r' && State.usb_input_index < MAX_PAYLOAD_BYTE_SIZE - 1)
            {
                buff_usart2_output[State.usb_input_index++] = *State.rawdata_to_process; 
                HAL_UART_Receive_IT(&huart2, buff_usart2_input, 1);
                State.rawdata_needs_processing = 0U;
            }
            // If this is the 16th input and not \r, we need to add it too:
            else if (*State.rawdata_to_process != '\r' && State.usb_input_index == MAX_PAYLOAD_BYTE_SIZE - 1)
            {
                buff_usart2_output[State.usb_input_index++] = *State.rawdata_to_process; 
                HAL_UART_Receive_IT(&huart2, buff_usart2_input, 1);
                State.rawdata_needs_processing = 0U;
            }

            // Run this if we got the 16th input or if we got \r
            if (*State.rawdata_to_process == '\r' || State.usb_input_index == MAX_PAYLOAD_BYTE_SIZE)
            {
                print_debug("\r\n[Debug] Enough raw data collected, creating packet!\r\n");

                size_t length = State.usb_input_index;

                __attribute__((aligned(sizeof(uint16_t))))
                    Message_t msg =
                    {
                        .header = 
                        {
                            .start_byte = ICD_START_BYTE,
                            .id_src = HAL_GetUIDw1(),
                            .command_type = (uint8_t) CMD_MSG,
                            .length = length,
                            .reserve = 0,
                        },
                        .payload = {0},
                        .checksum = 0,
                    };

                memcpy(&msg.payload, (uint8_t*) buff_usart2_output, length);
                encrypt_decrypt(msg.payload, msg.payload, length);
                msg.checksum = calculate_crc16(msg.payload, length);

                // Secret bad message command!
                #ifdef DEBUG
                uint8_t is_test_packet = 0U;
                if (length == MAX_PAYLOAD_BYTE_SIZE)
                {
                    is_test_packet = 1U;
                    for (size_t i = 0; i < MAX_PAYLOAD_BYTE_SIZE; i++)
                        if (buff_usart2_output[i] != bad_packet_code[i])
                            is_test_packet = 0U;
                }
                if (is_test_packet)
                {
                    print_debug("[Debug] Detected malformed packet code!\r\n");
                    print_debug("[Debug] Sending a bad packet...\r\n");
                    memset(&msg, 123U, MESSAGE_BYTE_LENGTH);
                }
                #endif

                print("\r\n[Protocol] Message ready to be sent!\r\n");
                print_debug("[Debug] Packet created: \r\n");

                print_debug_message(&msg);

                memcpy((uint8_t*) &State.buff_send_usart1, &msg, MESSAGE_BYTE_LENGTH);
                State.waiting_on_send_usart1 = 1U; 
                State.buff_send_usart1_origin = ORIGIN_USART2;
                State.rawdata_needs_processing = 0U;
                State.usb_input_index = 0;
            }
        }
    }
}

// target -> this
void handle_downstream_input()
{
    if (State.message_needs_processing_usart1)
    {
        // Check what kind of message we got
        Message_t *msg = (Message_t*) &State.message_to_process_usart1;

        // Drop every packet other than ERR, RESET and ACK packets if not initialized
        if (!State.is_init)
            switch (msg->header.command_type)
            {
                case CMD_ERROR:
                case CMD_RESET:
                case CMD_ACK:
                    break;
                default:
                    HAL_UART_Receive_IT(&huart1, buff_usart1, MESSAGE_BYTE_LENGTH);
                    State.message_needs_processing_usart1 = 0U;
                    return;
                    break;
            }
        
        switch (msg->header.command_type)
        {
            /*
             * Note: the packets receivable from downstream don't need any
             * real concurrency checks:
             *  - Errors instantly terminate the program
             *  - Resets instantly reset the program
             * So the only type of message we ever send back to upstream
             * are acknowledges, unless we WANT to override the sending logic
             * anyways. So we can just blindly overwrite the send buffers with
             * them!
             * More safety notes: We can NEVER send both an ACK for a MSG
             * and a forwarded ACK from the final node. This is because:
             *  - If we get a typical ACK, it ends there.
             *  - If we get the final node's ACK, we can assume that the ENTIRE
             *    chain worked, so that no more packets will be sent, and as such
             *    we can blindly forward it as we won't ever need to send
             *    another ACK, since the main node never sends another packet 
             *    UNLESS it gets the final node's ACK.
             * A lot of words to say: This works!
             */
            // Acknowledge from downstream received!
            case CMD_ACK:
                // Were we waiting for one?
                if (State.waiting_on_ack_for_usart1)
                {
                    print_debug("\r\n[Debug] Acknowledge message received! Freeing up downstream sending buffer. Packet:\r\n");
                    print_debug_message(msg);
                    print_debug("\r\n");

                    rearm_for_usart1_output();
                }
                
                if (State.waiting_on_final_ack && msg->header.id_src == DEFAULT_ID)
                {
                    if (!State.is_first_node)
                    {
                        print_debug("\r\n[Debug] Forwarding terminal node's ACK back to the master!\r\n");
                        memcpy((void*) State.buff_send_usart3, msg, MESSAGE_BYTE_LENGTH);
                        State.waiting_on_send_usart3 = 1U;
                        if (!State.is_init)
                        {
                            print_debug("\r\n[Debug] We're not initialized!\r\n");
                            State.sent_init = 0U;
                            State.is_init = 1U;
                        }
                    }
                    else
                    {
                        if (State.is_init)
                            print("[Protocol] Message acknowledged!\r\n[Input]> ");
                        else
                            print("[Protocol] Network initialized, Ready to send messages!\r\n[Input]> ");

                        print_debug("\r\n[Debug]Got final ACK! Good to send more packets!\r\n");
                        State.sent_init = 0U;
                        State.is_init = 1U;
                    }
                    State.waiting_on_final_ack = 0U;
                }
                break;
            case CMD_ERROR:
                print("[Protocol] Error packet received, uh oh!\r\n");
                print_debug("\r\n[Debug] Error packet received from downstream, forwarding up!\r\n");
                Message_t error = make_error();
                HAL_UART_Transmit(&huart3, (uint8_t*) &error, MESSAGE_BYTE_LENGTH, 100);
                State.is_error = 1U;;
                break;
            case CMD_RESET:
                print("\r\n[Protocol] Reset packet received! Goodnight!\r\n");
                print_debug("\r\n[Debug] Reset packet received from downstream, forwarding up!\r\n");
                reset_state();
                Message_t reset = make_reset();
                HAL_UART_Transmit(&huart3, (uint8_t*) &reset, MESSAGE_BYTE_LENGTH, 100);
                HAL_NVIC_SystemReset();
                break;
            default:
                break;
        }

        HAL_UART_Receive_IT(&huart1, buff_usart1, MESSAGE_BYTE_LENGTH);
        State.message_needs_processing_usart1 = 0U;
    }
}

// source -> this
void handle_upstream_input()
{
    if (State.message_needs_processing_usart3)
    {
        // We got a message, neat!
        Message_t *msg = (Message_t*) &State.message_to_process_usart3;

        if (!State.is_init)
        {
            // Drop every packet other than INIT, ERR and RESET packets
            switch (msg->header.command_type)
            {
                case CMD_ERROR:
                case CMD_RESET:
                case CMD_INIT:
                    break;
                default:
                    State.message_needs_processing_usart3 = 0U;
                    HAL_UART_Receive_IT(&huart3, buff_usart3, MESSAGE_BYTE_LENGTH);
                    return;
            }
        }

        // Drop the message if it isn't good!
        if (!is_message_valid(msg))
        {
            print("[Protocol] Received a malformed message, erroring out!\r\n");
            print_debug("\r\n[Debug] Malformed message! Erroring out!.\r\n");
            State.message_needs_processing_usart3 = 0U;
            HAL_UART_Receive_IT(&huart3, buff_usart3, MESSAGE_BYTE_LENGTH);
            send_error();
            State.is_error = 1U;
            return;
        }

        Message_t ack = make_ack();
        switch (msg->header.command_type)
        {
            case CMD_INIT:
                memcpy((void*) State.encryption_key, msg->payload, KEY_BYTE_SIZE);
                print("\r\n[Protocol] Got an init signal, setting up!\r\n"); 
                print_debug("\r\n[Debug] Received init signal: Setting key!\r\n");

            case CMD_MSG:
                // Process the message later if we can't send an ACK for it.
                if (State.waiting_on_send_usart3)
                    return;

                // Special ID if we're the last node!
                if (State.is_last_node)
                    ack.header.id_src = DEFAULT_ID;

                memcpy((void*)State.buff_send_usart3, &ack, MESSAGE_BYTE_LENGTH);
                State.waiting_on_send_usart3 = 1U;
                print_debug("[Debug] Queued ACK for upstream!\r\n");
                break;

            case CMD_RESET:
                print("\r\n[Protocol] Reset packet received! Goodnight!\r\n");
                print_debug("\r\n[Debug] Reset packet received from upstream, forwarding down!\r\n");
                reset_state();
                Message_t reset = make_reset();
                HAL_UART_Transmit(&huart1, (uint8_t*) &reset, MESSAGE_BYTE_LENGTH, 100);
                HAL_NVIC_SystemReset();
                break;

            case CMD_ERROR:
                print("[Protocol] Error packet received, uh oh!\r\n");
                print_debug("\r\n[Debug] Error packet received from upstream, forwarding down!\r\n");
                Message_t error = make_error();
                HAL_UART_Transmit(&huart1, (uint8_t*) &error, MESSAGE_BYTE_LENGTH, 100);
                State.is_error = 1U;
                break;
        }

        if (!State.waiting_on_send_usart1)
        {
            print_debug("[Debug] Processing packet from upstream!\r\n");
            // Prepare forwarding
            if (!State.is_last_node)
            {
                print_debug("[Debug] Added packet to downstream sending queue for forwarding.\r\n");
                memcpy((void*)State.buff_send_usart1, msg, MESSAGE_BYTE_LENGTH);
                State.waiting_on_send_usart1 = 1U;
                State.buff_send_usart1_origin = ORIGIN_USART3;
            }

            switch (msg->header.command_type)
            {
                case CMD_INIT:
                    print_debug("[Debug] Processed init packet fully!\r\n");
                    break;
                case CMD_MSG:
                    print_debug("[Debug] Processed message packet!\r\n");
                    // Process packet locally
                    print_debug("[Debug] Raw packet data: \r\n");
                    print_debug_message(msg);
                    print_debug("[Debug] Decrypted packet data:\r\nHex: ");
                    encrypt_decrypt(msg->payload, msg->payload, MAX_PAYLOAD_BYTE_SIZE);
                    print_debug_payload(msg->payload, msg->header.length);
                    print_debug("\r\n[Debug] ASCII: ");
                    print_debug_payload_ascii(msg->payload, msg->header.length);
                    print_debug("\r\n");

                    print("[Protocol] You got mail! Message: ");
                    print_payload(msg->payload, msg->header.length);
                    print("\r\n");
                    break;
            }

            State.message_needs_processing_usart3 = 0U;
        }
    }
}

// this -> target
void handle_downstream_output()
{
    if (State.waiting_on_send_usart1)
    {
        if (State.waiting_on_ack_for_usart1 
                && (HAL_GetTick() - State.waiting_on_ack_for_usart1_since < MAX_ACK_WAIT_TIME_MS))
            return;

        else
        {

            // If we aren't initialized, but haven't gotten an ACK,
            // We're the final node!
            if (!State.is_first_node && State.sent_init && State.waiting_on_ack_for_usart1)
            {
                print("[Protocol] Seems like we're the final node!\r\n");
                print_debug("[Debug] No next node detected: We're the final node!\r\n");
                State.is_init = 1U;
                State.is_last_node = 1U;
                State.waiting_on_ack_for_usart1 = 0U;
                State.waiting_on_final_ack = 0U;

                Message_t ack = make_ack();
                ack.header.id_src = DEFAULT_ID;
                memcpy((void*)State.buff_send_usart3, &ack, MESSAGE_BYTE_LENGTH);
                State.waiting_on_send_usart3 = 1U;
                print_debug("[Debug] Queued Final ACK for upstream!\r\n");

                rearm_for_usart1_output();
                return;
            }

            // Wait for acknowledge for message and handshake messages
            Message_t *msg = (Message_t*) State.buff_send_usart1;


            print_debug("[Debug] Sending a message to downstream! (type: ");
            print_debug_u8(msg->header.command_type);
            print_debug(")\r\n");

            if (msg->header.command_type == CMD_INIT)
                State.sent_init = 1U;

            HAL_UART_Transmit(&huart1, (uint8_t*) State.buff_send_usart1, MESSAGE_BYTE_LENGTH, 100);
            if (!State.is_last_node && 
                    (msg->header.command_type == CMD_MSG || msg->header.command_type == CMD_INIT))
            {
                print_debug("[Debug] Waiting on an acknowledge from downstream.\r\n");
                State.waiting_on_ack_for_usart1 = 1U;
                State.waiting_on_ack_for_usart1_since = HAL_GetTick();
                State.waiting_on_final_ack = 1U;
            }
            else
                // Rearm the source of the interupt now if we don't want an ack
                rearm_for_usart1_output();
        }
    }
}

// this -> source
void handle_upstream_output()
{
    if (State.waiting_on_send_usart3)
    {
        Message_t *msg = (Message_t*) &State.buff_send_usart3; 
        print_debug("[Debug] Sending a message to upstream! (type: ");
        print_debug_u8(msg->header.command_type);
        print_debug(")\r\n");
        HAL_UART_Transmit(&huart3, (uint8_t*) State.buff_send_usart3, MESSAGE_BYTE_LENGTH, 100);

        // For now we're not sending anything upstream that needs a delayed rearm...
        // so this is fine
        State.waiting_on_send_usart3 = 0U;

        if (State.is_last_node)
            HAL_UART_Receive_IT(&huart3, buff_usart3, MESSAGE_BYTE_LENGTH);
    }
}

void rearm_for_usart1_output()
{
    switch (State.buff_send_usart1_origin)
    {
        case ORIGIN_USART1:
            State.buff_send_usart1_origin = ORIGIN_NONE;
            HAL_UART_Receive_IT(&huart1, buff_usart1, MESSAGE_BYTE_LENGTH);
            break;
        case ORIGIN_USART2:
            State.buff_send_usart1_origin = ORIGIN_NONE;
            HAL_UART_Receive_IT(&huart2, buff_usart2_input, 1);
            break;
        case ORIGIN_USART3:
            State.buff_send_usart1_origin = ORIGIN_NONE;
            HAL_UART_Receive_IT(&huart3, buff_usart3, MESSAGE_BYTE_LENGTH);
            break;
        default:
            break;
    }

    State.waiting_on_ack_for_usart1 = 0;
    State.waiting_on_send_usart1 = 0U;
}


