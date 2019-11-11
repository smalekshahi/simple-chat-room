#ifndef _SERVER_H
#define _SERVER_H

#include <sys/queue.h>
#include "common.h"
#include "protocol.h"

#define MAX_CLIENT 16
#define NO_NAME "No Name"
#define JOINED " joined\n"
#define YOU_JOINED "You joined\n"

typedef enum
{
    SENDER,
    OTHERS
} member_t;

typedef struct client_t
{
    int fd;
    char ip[INET_ADDRSTRLEN];
    char nickname[MAX_LEN_NICKNAME];
    TAILQ_ENTRY(client_t) entries;
    char* buffer;
    uint16_t len;
    int offset;
} client_t;

TAILQ_HEAD(, client_t) client_tailq_head;

extern uint8_t verbose;

int listen_connections(connection_t* listener);
int server_handler();
void handle_clients_socket(int i);
int handle_server_socket();

int get_len(char *buffer);
int receive_no_data(int receive_value, int client_fd);
void send_msg(int client_fd, char* buffer);
void send_data_msg(int client_fd, packet_t* packet);
void send_broadcast_msg(int client_fd, packet_t* packet);
void send_to_clients(int client_fd, uint8_t* data, int len);

packet_t* prepare_broadcast(char* nickname, member_t member);
void set_nickname(int client_fd, char* nickname);

void print_clients();
int add_client(int fd, char* ip);
int remove_client(int fd);
int get_maxfd();
struct client_t* find_client (int fd);
void show_client_info(int fd);
void update_recv_data(char* buffer, int len, int offset);
void set_client_buffer(client_t *client, int len);
void release_client(client_t *client);
void release_client_buffer(client_t *client);
int read_recv_data(client_t *client, int offset);
void on_stop_server();

#endif
