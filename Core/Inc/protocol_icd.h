#ifndef PROTOCOL_ICD_H
#define PROTOCOL_ICD_H

#include <stdint.h>

// A specific value to identify if a received message if one of this protocol.
#define ICD_START_BYTE ((uint8_t)0x7E)
// The maximum number of bytes in the payload.
#define MAX_PAYLOAD_BYTE_SIZE 16
// The default ID that a host has.
#define DEFAULT_ID ((uint32_t)0x0)
// Size in byte of the encryption key.
#define KEY_BYTE_SIZE (sizeof(uint32_t) / sizeof(uint8_t))

/**
* @brief Command flags enum.
* Enum of flags sent in the header of the message:
*/
typedef enum _CommandType_t {
    /**
     * @brief Initialisation.
     * The initialisation command. The encryption key is passed in the payload.
     */
    CMD_INIT = 0,
    /**
     * @brief Reinitialisation.
     * Shoud propagate the event, then the board must reset itself like it has
     * been turned on and off. Propagate BOTH ways, so that any board resetting
     * resets all of them in the network.
     */
    CMD_RESET,
    /**
     * @brief Acknowledgement.
     * Sent back to the source to acknowledge the reception of a message.
     */
    CMD_ACK,
    /**
     * @brief Message.
     * The payload is filled with a message.
     */
    CMD_MSG,
    /**
     * @brief Error.
     * An error happened, and this is propagated to the source. May have more
     * informations in the payload.
     */
    CMD_ERROR,

    /**
     * @brief Enum count
     */
    CMD_MAX
} CommandFlag_t;

/**
 * @brief Message header.
 * The header of the message, with its fields.
 */
typedef struct __attribute__((packed)) {
    /**
     * @brief The starting value of the packet.
     * This value must be equal to `ICD_START_BYTE`, to make sure it is not a
     * unknown protocol using the same communication line.
     */
    uint8_t start_byte;
    /**
     * @brief
     * The `CommandType_t` code for the goal of this message packet.
     */
    uint8_t command_type;
    /**
     * @brief Length of the payload.
     * The length in bytes of the payload.
     */
    uint8_t length;
    /**
     * @brief Reserved space.
     * Reserved space for future use.
     */
    uint8_t reserve;
    /**
     * @brief Source ID.
     * The ID of the source sending the message.
     */
    uint32_t id_src;
} MessageHeader_t;

/**
 * @brief The header length.
 * The length in bytes of the message's header.
 */
#define HEADER_BYTE_LENGTH (sizeof(MessageHeader_t) / sizeof(uint8_t))

 /**
  * @brief Message.
  * A message packet. It has at least the length of the header.
  */
typedef struct __attribute__((packed, aligned(sizeof(uint8_t)))) {
    /**
     * @brief The message's header.
     * The message's header structure.
     */
    MessageHeader_t header;
    /**
     * @brief Payload.
     * The message content. The size of the payload is described by the header.
     * The maximum length of the header is `MAX_PAYLOAD_BYTE_SIZE`.
     */
    uint8_t payload[MAX_PAYLOAD_BYTE_SIZE];
    /**
     * @brief The checksum of the payload.
     * The value of the checksum of the payload by a CRC16 algorithm.
     */
    uint16_t checksum;
} Message_t;

/**
 * @brief The message length.
 * The length in bytes of the message.
 */
#define MESSAGE_BYTE_LENGTH (sizeof(Message_t) / sizeof(uint8_t))

#endif  // PROTOCOL_ICD_H
