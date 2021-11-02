// Standard Libraries
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <regex.h>
#include <arpa/inet.h>
#include <inttypes.h> // To print uint32_t, uint16_t nicely

// Project Files
#include "message.h"
#include "constants.h"
#include "commands.h"

// Macrons
#define PORT_NUMBER_LENGTH 2
#define IPV4_BYTE_LENGTH 4
#define IPV4_STR_LENGTH 15
#define PORT_NUMBER_STR_LENGTH 5
#define MAX_COMMAND_LENGTH MAX_NAME + MAX_DATA + PORT_NUMBER_LENGTH + IPV4_BYTE_LENGTH

// Testing
// 1. Open two terminals, T1 and T2
// 2. On T1, run `nc -l -p <port number>`, e.g. `nc -l -p 3000`. This will be server.
// 3. On T2, run `nc <ip> <port number>` for server's ip and port number.
// 4. Entering whatever text on either T1 or T2 will show up on the other end.

uint32_t get_server_addr_from_login_command(char* login_command) {
    char server_addr_str[IPV4_STR_LENGTH];
    uint32_t server_addr_binary;
    sscanf(login_command, "%*s %*s %*s %s", server_addr_str);
    short is_server_ip_valid = inet_pton(AF_INET, server_addr_str, &server_addr_binary);
    if (is_server_ip_valid != 1) {
        printf("Error: Invalid server address.\n");
        exit(EXIT_FAILURE);
    }
    return server_addr_binary;
}

uint16_t get_server_port_number_from_login_command(char* login_command) {
    char server_port_number_str[PORT_NUMBER_STR_LENGTH];
    uint16_t server_port_number;
    sscanf(login_command, "%*s %*s %*s %*s %s", server_port_number_str);
    return htons((unsigned short)(atoi(server_port_number_str)));
}

Message* build_message_from_input(char* input_buf) {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];

    if (is_string_of_pattern(input_buf, LOGIN_COMMAND_PATTERN)) {
        type = LOGIN;
        printf("IS LOGIN %d\n", type);
    } else if (is_string_of_pattern(input_buf, LOGOUT_COMMAND_PATTERN)) {
        printf("IS LOGOUT\n");
    } else if (is_string_of_pattern(input_buf, JOIN_SESS_COMMAND_PATTERN)) {
        printf("IS JOIN SESS\n");
    } else if (is_string_of_pattern(input_buf, LEAVE_SESS_COMMAND_PATTERN)) {
        printf("IS LEAVE SESS\n");
    } else if (is_string_of_pattern(input_buf, CREATE_SESS_COMMAND_PATTERN)) {
        printf("IS CREATE SESS\n");
    } else if (is_string_of_pattern(input_buf, LIST_COMMAND_PATTERN)) {
        printf("IS LIST\n");
    } else if (is_string_of_pattern(input_buf, QUIT_COMMAND_PATTERN)) {
        printf("IS QUIT\n");
        exit(0);
    } else {
        printf("IS MESSAGE\n");
    }

    // TODO: Create the message
    return NULL;
}

int main(void) {
    printf("Hello world client %d\n", MAX_COMMAND_LENGTH);

    char input_buf[MAX_COMMAND_LENGTH];
    Message* p_message;

    // State Information
    bool is_connection_setup = false;

    // Server Information
    short is_server_ip_valid;
    uint32_t server_addr_binary;
    uint16_t server_port_number;
    struct sockaddr_in server_addr;

    while (1) {
        fgets(input_buf, sizeof(input_buf), stdin);

        if (!is_connection_setup) {
            if (!is_string_of_pattern(input_buf, LOGIN_COMMAND_PATTERN)) {
                printf("You first need to log in to connect to the server.\n");
                continue;
            } else {
                server_addr_binary = get_server_addr_from_login_command(input_buf);
                server_port_number = get_server_port_number_from_login_command(input_buf);
                // printf("%" PRIu32 "\n", server_addr_binary);
                // printf("%" PRIu16 "\n", server_port_number);
                // TODO: establish TCP connection
                //   If successful, set is_connection_setup to true, create new listening thread
                is_connection_setup = true;
                //   If not, just continue in the loop
            }
        }

        p_message = build_message_from_input(input_buf);
        // Send message to server over the TCP connection
    }
}



