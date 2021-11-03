// Standard Libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Project Files
#include "constants.h"

typedef struct Message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
} Message;

typedef enum MessageType {
    LOGIN,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK
} MessageType;

Message* build_message(unsigned int type, unsigned int size, unsigned char* source, unsigned char* data) {
    Message* p_message = (Message*) malloc(sizeof(Message));
    p_message -> type = type;
    p_message -> size = size;
    strncpy(p_message -> source, source, MAX_NAME);
    strncpy(p_message -> data, data, MAX_DATA);
    return p_message;
}

void print_message(Message message) {
    printf("Message: type = %d, size = %d\n");
}