#pragma once

#define SECRET_MALFORMED_PACKET_CODE {'m', 'a', 'l', 'f', 'o', 'r', 'm', 'e', 'd', 'p', 'a', 'c', 'k', 'e', 't', '!'}

void handle_usb_input();
void handle_upstream_input();
void handle_downstream_input();

void handle_upstream_output();
void handle_downstream_output();

void rearm_for_usart1_output();
