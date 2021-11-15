// Standard Libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

// Project Files
#include "constants.h"

typedef struct Message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
} Message;

typedef enum MessageType {
    LOGIN = 1,
    LO_ACK = 2,
    LO_NAK = 3,
    EXIT = 4,
    JOIN = 5,
    JN_ACK = 6,
    JN_NAK = 7,
    LEAVE_SESS = 8,
    NEW_SESS = 9,
    NS_ACK = 10,
    MESSAGE = 11,
    QUERY = 12,
    QU_ACK = 13
} MessageType;

Message* build_message(unsigned int type, unsigned int size, unsigned char* source, unsigned char* data) {
    Message* p_message = (Message*) malloc(sizeof(Message));
    p_message -> type = type;
    p_message -> size = size;
    strncpy(p_message -> source, source, MAX_NAME);
    strncpy(p_message -> data, data, MAX_DATA);
    return p_message;
}

char* serialize_message(Message message) {
    char* message_string = (char*) malloc(sizeof(char)*MAX_MESSAGE_LENGTH);
    sprintf(message_string, "%d:%d:%s:", message.type, message.size, message.source);
    memcpy(message_string + strlen(message_string), message.data, message.size);
    // printf("%s\n", message_string); // TODO: Does not handle case if data contains zero byte
    return message_string;
}

void print_message(Message message) {
    printf("Message: type = %d, size = %d, src = %s, data = %s\n", message.type, message.size, message.source, message.data);
}

void send_message_string(char* message_string, int socket_fd) {
    // First send four bytes that represent length of the message
    unsigned char bytes[4];
    unsigned int message_strlen = strlen(message_string);
    bytes[0] = (message_strlen >> 24) & 0xFF;
    bytes[1] = (message_strlen >> 16) & 0xFF;
    bytes[2] = (message_strlen >> 8) & 0xFF;
    bytes[3] = message_strlen & 0xFF;
    // printf("%x %x %x %x\n", bytes[0], bytes[1], bytes[2], bytes[3]);
    send(socket_fd, bytes, 4, 0);

    // Then send the message
    send(socket_fd, message_string, message_strlen, 0);
}