// Standard Libraries
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <regex.h>
#include <arpa/inet.h> // For type conversions, e.g. htons
#include <inttypes.h> // To print uint32_t, uint16_t nicely
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

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

char* get_client_name_from_login_command(char* login_command) {
    char* client_name = (char*) malloc(sizeof(char)*MAX_NAME);
    sscanf(login_command, "%*s %s", client_name);
    return client_name;
}

char* get_password_from_login_command(char* login_command) {
    char* password = (char*) malloc(sizeof(char)*MAX_DATA);
    sscanf(login_command, "%*s %*s %s", password);
    return password;
}

char* get_session_name_from_second_arg(char* session_command) {
    char* session_name = (char*) malloc(sizeof(char)*MAX_DATA);
    sscanf(session_command, "%*s %s", session_name);
    return session_name;
}

Message* build_message_from_input(char* input_buf, char* sender_name) {
    unsigned int type;
    unsigned int size = 0;
    unsigned char* p_source = sender_name;
    unsigned char empty_string[1] = "";
    unsigned char* p_data = empty_string;

    if (is_string_of_pattern(input_buf, LOGIN_COMMAND_PATTERN)) {
        type = LOGIN;
        p_data = get_password_from_login_command(input_buf);
        size = strlen(p_data);
    } else if (is_string_of_pattern(input_buf, LOGOUT_COMMAND_PATTERN)) {
        type = EXIT;
    } else if (is_string_of_pattern(input_buf, JOIN_SESS_COMMAND_PATTERN)) {
        type = JOIN;
        p_data = get_session_name_from_second_arg(input_buf);
        size = strlen(p_data);
    } else if (is_string_of_pattern(input_buf, LEAVE_SESS_COMMAND_PATTERN)) {
        type = LEAVE_SESS;
    } else if (is_string_of_pattern(input_buf, LEAVE_SESS_NAME_COMMAND_PATTERN)) {
        type = LEAVE_SESS_NAME;
        p_data = get_session_name_from_second_arg(input_buf);
        size = strlen(p_data);
    } else if (is_string_of_pattern(input_buf, CREATE_SESS_COMMAND_PATTERN)) {
        type = NEW_SESS;
        p_data = get_session_name_from_second_arg(input_buf);
        size = strlen(p_data);
    } else if (is_string_of_pattern(input_buf, LIST_COMMAND_PATTERN)) {
        type = QUERY;
    } else if (is_string_of_pattern(input_buf, QUIT_COMMAND_PATTERN)) {
        type = EXIT;
    } else {
        type = MESSAGE;
        p_data = input_buf;
        size = strlen(p_data);
    }

    return build_message(type, size, p_source, p_data);
}

void *receiver_thread_call(void* p_socket_fd) {
    char receive_buf[MAX_DATA]; // TODO: Need a bigger length here later
    ssize_t n_bytes_received;
    int socket_fd = *((int*) p_socket_fd);
    while (1) {
        n_bytes_received = recv(socket_fd, receive_buf, MAX_DATA, 0);
        if (n_bytes_received > 0) {
            receive_buf[n_bytes_received] = '\0'; // TODO: HACK!
            printf("Receiver: %s\n", receive_buf); // TODO: Hack to bypass the four length bytes
        } else if (n_bytes_received == 0) {
            printf("Connection is closed by the server.\n");
            break;
        } else {
            printf("Failed to receive from the connection socket: %s\n", strerror(errno));
            break;
        }
    }
}

pthread_t init_receiving_thread(int socket_fd) {
    pthread_t* p_receiver_thread = (pthread_t *) malloc(sizeof(pthread_t));
    int *arg = malloc(sizeof(*arg));
    *arg = socket_fd;
    pthread_create(p_receiver_thread, NULL, receiver_thread_call, arg);
    return *p_receiver_thread;
}

int main(void) {
    // printf("Hello world client %d\n", MAX_COMMAND_LENGTH);

    char input_buf[MAX_COMMAND_LENGTH];
    Message* p_message;

    // State Information
    bool is_connection_setup = false;
    int socket_fd;
    pthread_t receiver_thread;

    // Client Information
    char* client_name;

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
                // Parse server information
                server_addr_binary = get_server_addr_from_login_command(input_buf);
                server_port_number = get_server_port_number_from_login_command(input_buf);
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = server_port_number;
                server_addr.sin_addr.s_addr = server_addr_binary;

                // Parse client information
                client_name = get_client_name_from_login_command(input_buf);

                // Establish TCP connection
                socket_fd = socket(AF_INET, SOCK_STREAM, 0);

                if (socket_fd == -1) {
                    printf("Failed to create a socket: %s. Try again.\n", strerror(errno));
                    continue;
                }

                int connect_rval = connect(socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr));

                if (connect_rval == 0) {
                    printf("Connected to the server.\n");
                    is_connection_setup = true;
                    // Create new thread to receive and print messages from server
                    receiver_thread = init_receiving_thread(socket_fd);
                } else {
                    printf("Failed to connect to the server. Verify your input and retry login.\n");
                    continue;
                }
            }
        }

        if (is_connection_setup) {
            // Send message
            p_message = build_message_from_input(input_buf, client_name);
            // print_message(*p_message);
            char* message_string = serialize_message(*p_message);
            send_message_string(message_string, socket_fd);

            // Cancel the connection, if user logs out
            if (is_string_of_pattern(input_buf, LOGOUT_COMMAND_PATTERN)) {
                is_connection_setup = false;
                int thread_cancel_status = pthread_cancel(receiver_thread);
                if (thread_cancel_status == 0) {
                    printf("Receiver thread terminated.\n");
                } else {
                    printf("Warning: Receiver thread cannot be closed!\n");
                }
                close(socket_fd);
            } 
        }

        // Quit command exits the program, after sending a logout to server
        if (is_string_of_pattern(input_buf, QUIT_COMMAND_PATTERN)) {
            close(socket_fd);
            exit(0);
        }
    }
}



