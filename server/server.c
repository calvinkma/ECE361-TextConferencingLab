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

#define MAX_NUM_CLIENTS 3
#define MAX_NUM_SESSIONS 30
#define MAX_SESSION_CLIENTS 30


struct Session;

struct Client {
    char client_ID[150];
    char password[150];
    bool connected;
    int client_FD;
    struct Session *joined_sessions[MAX_NUM_SESSIONS];
};

struct Client client_list[MAX_NUM_CLIENTS] = {
    { .client_ID = "calvin", .password = "stanford", .connected = false },
    { .client_ID = "jerry", .password = "microsoft", .connected = false },
    { .client_ID = "joe", .password = "google", .connected = false }
};

struct Session {
    int session_index;
    char session_ID[150];
    int num_users;
    struct Client *session_clients[MAX_SESSION_CLIENTS];
};

struct Session *sessions[MAX_NUM_SESSIONS];

fd_set read_fds;


bool is_input_valid(int argc, char *argv[]) {
    return argc == 2; // Must be supplied exactly 1 argument
}

short get_tcp_listen_port_from_input(char *argv[]) {
    return (short)(atoi(argv[1]));
}

int open_tcp_socket(void) {
    return socket(AF_INET, SOCK_STREAM, 0);
}

struct sockaddr_in* get_server_addr(short port) {
    struct sockaddr_in* server_addr = (struct sockaddr_in*) malloc (sizeof(struct sockaddr_in));
    server_addr -> sin_family = AF_INET;
    server_addr -> sin_port = htons(port);
    (server_addr -> sin_addr).s_addr = htonl(INADDR_ANY);
    return server_addr;
}

void bind_socket_to_port(int socket_fd, struct sockaddr_in* server_addr) {
    int status = bind(socket_fd, (struct sockaddr *)server_addr, sizeof(struct sockaddr));
    if (status != 0) {
        printf("Error: Failed to bind socket to port. Retry with another port.\n");
        exit(EXIT_FAILURE);
    }
}

void start_listening(int socket) {
    if (listen(socket, 5) != 0) {
        printf("Error: Could not listen.");
        exit(EXIT_FAILURE);
    }
}

struct Message decode_char_array(char* buffer, int message_strlen) {
    struct Message message;
    int delimiter_indexes[3] = {0, 0, 0};
    int current_delimiter_count = 0;

    for (int i = 0; i < message_strlen; i++) {
        if (current_delimiter_count > 3) {
            break;
        }
        if (buffer[i] == ':') {
            delimiter_indexes[current_delimiter_count] = i;
            current_delimiter_count += 1;
        }
    }

    message.type = atoi(buffer);
    message.size = atoi(buffer + sizeof(char) * delimiter_indexes[0] + sizeof(char));

    int num_source_chars = delimiter_indexes[2] - delimiter_indexes[1] - 1;
    (message.source)[num_source_chars] = '\0';
    memcpy(message.source, buffer + sizeof(char) * delimiter_indexes[1] + sizeof(char), num_source_chars);
    (message.data)[message_strlen - delimiter_indexes[2] - 1] = '\0';
    memcpy(message.data, buffer + sizeof(char) * delimiter_indexes[2] + sizeof(char), message_strlen - delimiter_indexes[2] - 1);

    return message;
}


bool user_login(int new_client) {
    char buffer[MAX_MESSAGE_LENGTH];
    unsigned int message_strlen;
    recv(new_client, buffer, 4, 0);

    message_strlen = ((buffer[0] << 24) & 0xFF000000) | ((buffer[1] << 16) & 0xFF0000) | ((buffer[2] << 8) & 0xFF00) | (buffer[3]& 0xFF);

    recv(new_client, buffer, message_strlen, 0);
    struct Message message = decode_char_array(buffer, message_strlen);

    if (message.type == LOGIN) {
        for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
            if ((strcmp(client_list[i].client_ID, message.source) == 0) && (strcmp(client_list[i].password, message.data) == 0)) {
                if (client_list[i].connected) {
                    struct Message response = { .type = LO_NAK, .source = "Server", .data = "Client already connected" };
                    response.size = strlen(response.data);
                    char* message_string = serialize_message(response);
                    send_message_string(message_string, new_client);
                    return false;
                } else {
                    struct Message response = { .type = LO_ACK, .source = "Server", .data = "" };
                    response.size = strlen(response.data);
                    char* message_string = serialize_message(response);
                    send_message_string(message_string, new_client);

                    client_list[i].client_FD = new_client;
                    client_list[i].connected = true;
                    printf("Client %s connected to server\n", message.source);
                    return true;
                }
            }
        }
        struct Message response = { .type = LO_NAK, .source = "Server", .data = "Wrong username or password" };
        response.size = strlen(response.data);
        char* message_string = serialize_message(response);
        send_message_string(message_string, new_client);
        return false;
    } else {
        struct Message response = { .type = LO_NAK, .source = "Server", .data = "Wrong message type" };
        response.size = strlen(response.data);
        char* message_string = serialize_message(response);
        send_message_string(message_string, new_client);
        return false;
    }
    return false;
}


struct Session* get_session(char *session_id) {
    for (int i = 0; i < MAX_NUM_SESSIONS; i++) {
        if (sessions[i] != NULL && (strcmp(sessions[i]->session_ID, session_id) == 0)) {
            return sessions[i];
        }
    }
    return NULL;
}


void process_leave_all(struct Client *client) {
    // Remove client from sessions
    for (int i = 0; i < MAX_NUM_SESSIONS; i++) {
        if (sessions[i] != NULL) {
            for (int j = 0; j < MAX_SESSION_CLIENTS; j++) {
                if (sessions[i]->session_clients[j] == client) {
                    sessions[i]->session_clients[j] = NULL;
                    sessions[i]->num_users--;
                }
            }
            if (sessions[i]->num_users == 0) {
                sessions[i] = NULL;
                free(sessions[i]);
            }
        }
    }

    // Remove sessions from client
    for (int i = 0; i < MAX_NUM_SESSIONS; i++) {
        client->joined_sessions[i] = NULL;
    }
}


void process_exit(struct Client *client) {
    for (int i = 0; i < MAX_NUM_SESSIONS; i++) {
        if (client->joined_sessions[i] != NULL) {
            process_leave_all(client);
        }
    }

    client->connected = false;
    FD_CLR(client->client_FD, &read_fds);
    close(client->client_FD);
    client->client_FD = -1;
}


void process_join(struct Client *client, struct Message message) {
    char response_data[MAX_DATA];
    struct Session *session_to_join = get_session(message.data);
    if (session_to_join == NULL) {
        struct Message response = { .type = JN_NAK, .source = "Server" };
        sprintf(response_data, "Could not find session %s", message.data);
        strcpy(response.data, response_data);
        response.size = strlen(response.data);
        char* message_string = serialize_message(response);
        send_message_string(message_string, client->client_FD);
        return;
    }

    // Add user to session members
    for (int i = 0; i < MAX_SESSION_CLIENTS; i++) {
        if (session_to_join->session_clients[i] == NULL) {
            session_to_join->session_clients[i] = client;
            session_to_join->num_users++;
            break;
        }
    }

    // Add session to user members
    if (client->joined_sessions[session_to_join->session_index] == NULL) {
        client->joined_sessions[session_to_join->session_index] = session_to_join;
    } else {
        printf("Error: User %s has already joined session %s\n", client->client_ID, session_to_join->session_ID);
    }

    // Send response
    struct Message response = { .type = JN_ACK, .source = "Server" };
    sprintf(response_data, "%s", session_to_join->session_ID);
    strcpy(response.data, response_data);
    response.size = strlen(response.data);
    char* message_string = serialize_message(response);
    send_message_string(message_string, client->client_FD);
}


void process_leave(struct Client *client, struct Message message) {
    struct Session *session_to_leave = NULL;
    char session_to_leave_ID[1024];
    strcpy(session_to_leave_ID, message.data);

    printf("Session to leave: %s\n", session_to_leave_ID);

    for (int i = 0; i < MAX_NUM_SESSIONS; i++) {
        if (strcmp(sessions[i]->session_ID, session_to_leave_ID) == 0) {
            session_to_leave = sessions[i];
            break;
        }
    }

    if (session_to_leave == NULL) {
        struct Message response = { .type = MESSAGE, .source = "Server", .data = "Session not found" };
        response.size = strlen(response.data);
        char* message_string = serialize_message(response);
        send_message_string(message_string, client->client_FD);
        return;
    }
    if (client->joined_sessions[session_to_leave->session_index] == NULL) {
        struct Message response = { .type = MESSAGE, .source = "Server", .data = "You are not a member of that session" };
        response.size = strlen(response.data);
        char* message_string = serialize_message(response);
        send_message_string(message_string, client->client_FD);
        return;
    }

    // Remove client from session
    for (int i = 0; i < MAX_SESSION_CLIENTS; i++) {
        if (session_to_leave->session_clients[i] == client) {
            session_to_leave->session_clients[i] = NULL;
            session_to_leave->num_users--;
            if (session_to_leave->num_users == 0) {
                sessions[session_to_leave->session_index] = NULL;
                free(session_to_leave);
            }
        }
    }

    // Remove session from client
    client->joined_sessions[session_to_leave->session_index] = NULL;
}


void process_new_session(struct Client *client, struct Message message) {
    for (int i = 0; i < MAX_NUM_SESSIONS; i++) {
        if (sessions[i] == NULL) {
            sessions[i] = malloc(sizeof(struct Session));
            sessions[i]->session_index = i;
            strcpy(sessions[i]->session_ID, message.data);

            for (int j = 1; j < MAX_SESSION_CLIENTS; j++) {
                sessions[i]->session_clients[j] = NULL;
            }
            sessions[i]->session_clients[0] = client;

            if (client->joined_sessions[i] == NULL) {
                client->joined_sessions[i] = sessions[i];
            } else {
                printf("Error: User %s has already joined session %s\n", client->client_ID, sessions[i]->session_ID);
            }
            
            sessions[i]->num_users = 1;

            struct Message response = { .type = NS_ACK, .source = "Server", .data = "" };
            response.size = strlen(response.data);
            char* message_string = serialize_message(response);
            send_message_string(message_string, client->client_FD);
            return;
        }
    }
    return;
}


void process_message(struct Client *client, struct Message message) {
    // Get session and data
    struct Session *session_to_broadcast = NULL;
    char *session_to_broadcast_ID = strtok(message.data, ":");
    char *data = strtok(NULL, ":");

    printf("Session to broadcast: %s\n", session_to_broadcast_ID);
    printf("Message content: %s\n", data);

    for (int i = 0; i < MAX_NUM_SESSIONS; i++) {
        if (strcmp(sessions[i]->session_ID, session_to_broadcast_ID) == 0) {
            session_to_broadcast = sessions[i];
            break;
        }
    }

    if (session_to_broadcast == NULL) {
        struct Message response = { .type = MESSAGE, .source = "Server", .data = "Session not found" };
        response.size = strlen(response.data);
        char* message_string = serialize_message(response);
        send_message_string(message_string, client->client_FD);
        return;
    }
    if (client->joined_sessions[session_to_broadcast->session_index] == NULL) {
        struct Message response = { .type = MESSAGE, .source = "Server", .data = "You are not a member of that session" };
        response.size = strlen(response.data);
        char* message_string = serialize_message(response);
        send_message_string(message_string, client->client_FD);
        return;
    }

    // Send message to all clients in the session, including sender
    for (int i = 0; i < MAX_SESSION_CLIENTS; ++i) {
        if (session_to_broadcast->session_clients[i] != NULL) {
            struct Message response = { .type = MESSAGE, .source = "Server" };
            strcpy(response.data, "Message from session ");
            strcat(response.data, session_to_broadcast_ID);
            strcat(response.data, ":");
            strcat(response.data, data);
            response.size = strlen(response.data);
            char* message_string = serialize_message(response);
            send_message_string(message_string, session_to_broadcast->session_clients[i]->client_FD);
        }
    }
}


void process_query(struct Client *client) {
    struct Message response = { .type = QU_ACK, .source = "Server" };
    char data[MAX_DATA];

    strcpy(data, "Active sessions: ");
    for (int i = 0; i < MAX_NUM_SESSIONS; i++) {
        if (sessions[i] != NULL) {
            strcat(data, sessions[i]->session_ID);
            strcat(data, ", ");
        }
    }
    strcat(data, "\n");

    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        if (client_list[i].connected) {
            strcat(data, "Client ");
            strcat(data, client_list[i].client_ID);
            strcat(data, " connected to sessions: ");
            for (int j = 0; j < MAX_NUM_SESSIONS; j++) {
                if (client_list[i].joined_sessions[j] != NULL) {
                    strcat(data, client_list[i].joined_sessions[j]->session_ID);
                    strcat(data, ", ");
                }
            }
            strcat(data, "\n");
        }
    }
    strcat(data, "\n");

    strcpy(response.data, data);
    response.size = strlen(response.data);
    char* message_string = serialize_message(response);
    printf("%s", message_string);
    send_message_string(message_string, client->client_FD);
}


void process_DM(struct Client *client, struct Message message) {
    // Get receiver and data
    char *recipient = strtok(message.data, ":");
    char *data = strtok(NULL, ":");

    printf("DM recipient: %s\n", recipient);
    printf("DM content: %s\n", data);

    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        if (strcmp(client_list[i].client_ID, recipient) == 0) {
            if (client_list[i].connected){
                struct Message response = { .type = MESSAGE, .source = "Server" };
                strcpy(response.data, "DM from ");
                strcat(response.data, client->client_ID);
                strcat(response.data, ":");
                strcat(response.data, data);
                response.size = strlen(response.data);
                char* message_string = serialize_message(response);
                send_message_string(message_string, client_list[i].client_FD);
                return;
            } else {
                struct Message response = { .type = MESSAGE, .source = "Server", .data = "DM recipient not connected" };
                response.size = strlen(response.data);
                char* message_string = serialize_message(response);
                send_message_string(message_string, client->client_FD);
                return;
            }
        }
    }

    struct Message response = { .type = MESSAGE, .source = "Server", .data = "DM recipient not found" };
    response.size = strlen(response.data);
    char* message_string = serialize_message(response);
    send_message_string(message_string, client->client_FD);
}


void process_client(struct Client *client) {
    if (!client->connected) {
        return;
    }

    if (FD_ISSET(client->client_FD, &read_fds)) {
        char buffer[MAX_MESSAGE_LENGTH];
        unsigned int message_strlen;
        int status = recv(client->client_FD, buffer, 4, 0);
        if (status == -1) {
            printf("Error: Couldn't read message from client %s.\n", client->client_ID);
            return;
        }

        message_strlen = ((buffer[0] << 24) & 0xFF000000) | ((buffer[1] << 16) & 0xFF0000) | ((buffer[2] << 8) & 0xFF00) | (buffer[3]& 0xFF);

        status = recv(client->client_FD, buffer, message_strlen, 0);
        if (status == -1) {
            printf("Error: Couldn't read message from client %s.\n", client->client_ID);
            return;
        }
        struct Message message = decode_char_array(buffer, message_strlen);

        switch (message.type) {
            case EXIT:
                printf("Case EXIT\n");
                process_exit(client);
                break;
            case JOIN:
                printf("Case JOIN\n");
                process_join(client, message);
                break;
            case LEAVE_SESS:
                printf("Case LEAVE_SESS\n");
                process_leave(client, message);
                break;
            case NEW_SESS:
                printf("Case NEW_SESS\n");
                process_new_session(client, message);
                break;
            case MESSAGE:
                printf("Case MESSAGE\n");
                process_message(client, message);
                break;
            case QUERY:
                printf("Case QUERY\n");
                process_query(client);
                break;
            case DM:
                printf("Case DM\n");
                process_DM(client, message);
                break;
            case LEAVE_ALL:
                printf("Case LEAVE_ALL\n");
                process_leave_all(client);
                break;
        }
    }
}


int main(int argc, char *argv[]) {
    // Check input
    if (!is_input_valid(argc, argv)) {
        printf("Error: Invalid input is provided.\n");
        exit(EXIT_FAILURE);
    }

    // Get input arg
    short tcp_listen_port = get_tcp_listen_port_from_input(argv);

    // Initialize socket
    int socket = open_tcp_socket();
    struct sockaddr_in* server_addr = get_server_addr(tcp_listen_port);
    bind_socket_to_port(socket, server_addr);
    start_listening(socket);
    printf("TCP Port Opened on Port %hi. Listening...\n", tcp_listen_port); // Run `ss -tulpn | grep ':3000'` to verify if there is a tcp socket on Port 3000

    // Initialize all clients to no sessions
    for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
        for (int j = 0; j < MAX_NUM_SESSIONS; j++) {
            client_list[i].joined_sessions[j] = NULL;
        }
    }

    struct sockaddr_in client_addr;
    int client_addr_len;

    while (1) {
        // Create set of FDs and find highest FD for select()
        int highest_fd;

        FD_ZERO(&read_fds);
        highest_fd = socket;
        FD_SET(socket, &read_fds);
        for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
            if (client_list[i].connected) {
                if (client_list[i].client_FD > highest_fd) {
                    highest_fd = client_list[i].client_FD;
                }
                FD_SET(client_list[i].client_FD, &read_fds);
            }
        }
        select(highest_fd + 1, &read_fds, NULL, NULL, NULL);

        // Client login attempt
        if (FD_ISSET(socket, &read_fds)) {
            int new_client = accept(socket, (struct sockaddr *)(&client_addr), &client_addr_len);
            if (!user_login(new_client)) {
                close(new_client);
            }

        } else {
            for (int i = 0; i < MAX_NUM_CLIENTS; i++) {
                process_client(&client_list[i]);
            }
        }
    }
}