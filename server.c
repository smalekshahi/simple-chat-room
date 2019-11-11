#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include "server.h"

fd_set master_set;
int maxfd;
int listen_fd;
char received_message[MAX_LEN_MSG] = {0};
volatile sig_atomic_t server_error_in_progress = 1;

void
server_error_signal(int sig)
{
    if (server_error_in_progress)
        raise (sig);
    server_error_in_progress = 1;
    on_stop_server();
    signal (sig, SIG_DFL);
    raise (sig);
}

void
on_stop_server()
{
    client_t* client;
    if(listen_fd >= 0) {
        close(listen_fd);
        listen_fd = -1;
    }

    while (client = TAILQ_FIRST(&client_tailq_head)) {
        TAILQ_REMOVE(&client_tailq_head, client, entries);
        release_client(client);
    }

    if (!TAILQ_EMPTY(&client_tailq_head))
        printf("Client queue is NOT empty\n");
    else
        printf("Client queue IS empty\n");
}

int
listen_connections(connection_t* listener)
{
    int on = 1;

    if (setsockopt(listener->fd, SOL_SOCKET,  SO_REUSEADDR, (char* )&on,
                   sizeof(on)) < 0) {
        perror("ERROR: setsockopt() failed");
        return 1;
    }

    if (ioctl(listener->fd, FIONBIO, (char *)&on) < 0) {
        perror("ERROR: server ioctl() failed");
        return 1;
    }

    if (bind(listener->fd, (struct sockaddr* )&listener->address, sizeof(listener->address)) < 0) {
        perror("ERROR: bind() failed");
        return 1;
    }

    if (listen(listener->fd, MAX_CLIENT) < 0) {
        perror("ERROR: listen()");
        return 1;
    }

    listen_fd = listener->fd;
    return 0;
}

int
server_handler()
{
    fd_set read_set;
    int i ;
    TAILQ_INIT(&client_tailq_head);

    maxfd = listen_fd;
    FD_ZERO(&master_set);
    FD_ZERO(&read_set);
    FD_SET(listen_fd, &master_set);

    signal(SIGINT, &server_error_signal);

    while (1) {
        memcpy(&read_set, &master_set, sizeof(master_set));

        if (select(maxfd + 1, &read_set, NULL, NULL, NULL) == -1) {
            perror("ERROR: select");
            return 1;
        }

        for (i = 0; i <= maxfd; i++) {
            if (!FD_ISSET(i, &read_set))
                continue;
            if (i == listen_fd) {
                if (handle_server_socket() != 0)
                    return 1;
            } else
                handle_clients_socket(i);
        }
    }

    return 0;
}

int
handle_server_socket()
{
    int newfd;
    char remote_ip[INET_ADDRSTRLEN];
    struct sockaddr_in remoteaddr;
    socklen_t addrlen = sizeof(remoteaddr);

    newfd = accept(listen_fd, (struct sockaddr* )&remoteaddr, &addrlen);
    if (newfd < 0) {
        perror("ERROR: Cannot accept client");
        return 1;
    } else {
        inet_ntop(AF_INET, &remoteaddr.sin_addr, remote_ip, sizeof(remote_ip));
        if (add_client(newfd, remote_ip) < 0)
            return 0;

        FD_SET(newfd, &master_set);
        if (newfd > maxfd)
            maxfd = newfd;
    }

    return 0;
}

int
add_client(int fd, char* ip)
{
    client_t* client;
    client = calloc(1, sizeof(client_t));

    if (client == NULL) {
        show_message("ERROR: Cannot add new client.\n");
        perror("ERROR: calloc() failed");
        return 1;
    }

    client->fd = fd;
    strncpy(client->ip, ip, INET_ADDRSTRLEN);
    client->offset = 0;
    client->len = 0;
    client->buffer = NULL;
    TAILQ_INSERT_TAIL(&client_tailq_head, client, entries);
    if (verbose == 1) {
        printf("INFO: New connection from %s on socket %d\n", ip, fd);
        print_clients();
    }

    return 0;
}

void
handle_clients_socket(int client_fd)
{
    int offset = 0, remainder_len = 0, end_point = 0;
    client_t *client = NULL;

    memset(received_message, 0, sizeof(received_message));
    if((client = find_client(client_fd)) == NULL )
        return;
    if (client->offset > 0) {
        memcpy(received_message, client->buffer, client->offset);
        offset += client->offset;
    }

    remainder_len = read_recv_data(client, offset);
    if (remainder_len == -1)
        return;

    set_client_buffer(client, client->len + LEN_HEADER - client->offset);
    send_msg(client_fd, client->buffer);
    end_point = client->len + LEN_HEADER;

    while (remainder_len >= 4) {
        if((client->len = get_len(received_message + end_point)) < 0)
            return;
        if (remainder_len >= client->len + LEN_HEADER) {
            client->buffer = realloc(client->buffer, (client->len + LEN_HEADER) * sizeof(char*));
            if (client->buffer == NULL) {
                perror("ERROR: calloc() failed in recv");
                return;
            }
            memcpy(client->buffer, received_message + end_point, client->len + LEN_HEADER);
            send_msg(client_fd, client->buffer);
            end_point += client->len + LEN_HEADER;
            remainder_len -= client->len + LEN_HEADER;
        } else
	    break;
    }

    if (remainder_len > 0) {
        client->buffer = realloc(client->buffer, remainder_len * sizeof(char*));
        if (client->buffer == NULL) {
            perror("ERROR: calloc() failed in recv");
            return;
        }

        memcpy(client->buffer, received_message + end_point, remainder_len);
        client->offset = remainder_len;
	printf("client->offset=%d\n", client->offset);
        return;
    }

    release_client_buffer(client);
}

client_t*
find_client (int fd)
{
    client_t* client;

    TAILQ_FOREACH (client, &client_tailq_head, entries)
    {
        if (client->fd == fd)
            return client;
    }

    show_message("There is no client with file descriptor (%d)", fd);
    return NULL;
}

int
read_recv_data(client_t *client, int offset)
{
    char buffer[BUFFER_SIZE];
    int rc, read_len = 0, remainder_len = -1;

    while(1) {
        if (client->len > 0)
            read_len = MIN(BUFFER_SIZE, (client->len + LEN_HEADER - client->offset));
        else
            read_len = BUFFER_SIZE;

        if ((rc = recv(client->fd, buffer, read_len, 0)) <= 0)
        {
            if (receive_no_data(rc, client->fd) != 0) {
                show_message("DEBUG: client with fd = %d has timeout. Back again\n", client->fd);
                if(offset > client->offset) {
                    set_client_buffer(client, offset - client->offset);
                    client->offset = offset;
                }
            }
            return -1;
        }

        update_recv_data(buffer, rc, offset);
        offset += rc;

        if(client->len == 0) {
            if(offset < 4)
                continue;
            if((client->len = get_len(received_message)) < 0)
                return -1;
        }

        if (offset >= client->len + LEN_HEADER) {
            remainder_len = offset - (client->len + LEN_HEADER);
            return remainder_len;
        }
    }
}

int
receive_no_data(int receive_value, int client_fd)
{
    if (receive_value == 0)
        show_message("INFO: client with fd = %d left.\n", client_fd);
    else {
        if (errno == EWOULDBLOCK)
            return 1;
        perror("ERROR: recv() failed. Client is removing...");
    }

    if (remove_client(client_fd) == 0) {
        FD_CLR(client_fd, &master_set);
        if (client_fd == maxfd)
            maxfd = get_maxfd();
        if (verbose == 1)
            print_clients();
    }

    return 0;
}

int
get_maxfd()
{
    client_t* client;
    int maxfd = listen_fd;

    TAILQ_FOREACH(client, &client_tailq_head, entries) {
        if (client->fd > maxfd)
            maxfd = client->fd;
    }

    show_message("INFO: Maximun file descriptor is %d\n", maxfd);
    return maxfd;
}

int
remove_client(int fd)
{
    client_t* client;
    TAILQ_FOREACH(client, &client_tailq_head, entries) {
        if (client->fd == fd) {
            TAILQ_REMOVE(&client_tailq_head, client, entries);
            release_client(client);
            show_message("Client with ID = %d removed\n", fd);
            return 0;
        }
    }
    show_message("Cannot remove client with ID: %d", fd);
    return 1;
}

void
release_client_buffer(client_t *client)
{
    client->len = 0;
    client->offset = 0;
    if(client->buffer != NULL) {
        free(client->buffer);
        client->buffer = NULL;
    }
}

void
release_client(client_t *client)
{
    if (client == NULL)
        return;
    close(client->fd);
    release_client_buffer(client);
    free(client);
    client = NULL;
}

int
get_len(char *buffer)
{
    int len = -1;
    packet_t* packet;

    packet = parse(buffer);
    if (packet && ntohs(packet->len) < MAX_LEN_MSG - LEN_HEADER && (ntohs(packet->type) == MESSAGE
            || ntohs(packet->type) == JOIN))
        len = ntohs(packet->len);
    else
        show_message("Wrong Message!!!");

    if (packet)
        release(packet);

    return len;
}

void
set_client_buffer(client_t *client, int len)
{
    if(client->buffer == NULL)
        client->buffer = calloc(len, sizeof(char*));
    else
        client->buffer = realloc(client->buffer, (client->offset + len) * sizeof(char*));
    if (client->buffer == NULL) {
        perror("ERROR: calloc()/realloc failed in recv");
        return;
    }
    memcpy(client->buffer + client->offset, received_message + client->offset, len);
}

void
update_recv_data(char* buffer, int len, int offset)
{
    memcpy(received_message + offset, buffer, len);
}

void
send_msg(int client_fd, char* buffer)
{
    packet_t* packet = NULL;

    if ((packet = parse(buffer)) == NULL)
        return;

    if (ntohs(packet->type) != MESSAGE && ntohs(packet->type) != JOIN) {
        show_message("Wrong Data!!!");
        release(packet);
        return;
    }

    if (ntohs(packet->type) == MESSAGE)
        send_data_msg(client_fd, packet);
    else
        send_broadcast_msg(client_fd, packet);

    if (packet)
        release(packet);
}

void
send_data_msg(int client_fd, packet_t* packet)
{
    uint8_t* data = NULL;

    data = make_raw_data(*packet);
    if (data) {
        send_to_clients(client_fd, data, ntohs(packet->len));
        free(data);
    }
}

void
send_broadcast_msg(int client_fd, packet_t* packet)
{
    uint8_t* data = NULL;
    packet_t* boadcast_packet = NULL;
    char nickname[MAX_LEN_NICKNAME] = {0};
    int len;

    memcpy(nickname, packet->body, ntohs(packet->len));
    if ((boadcast_packet = prepare_broadcast(nickname, SENDER)) != NULL) {
        data = make_raw_data(*boadcast_packet);
        if (data)
            if (send(client_fd, data, ntohs(boadcast_packet->len) + LEN_HEADER, 0) < 0)
                perror("ERROR: Cannot send join response to new joined client.");

        set_nickname(client_fd, nickname);
        if (boadcast_packet)
            release(boadcast_packet);
        if (data)
            free(data);
    } else
        show_message("ERROR: Cannot create joined packet for sender\n");

    if ((boadcast_packet = prepare_broadcast(nickname, OTHERS)) != NULL) {
        len = ntohs(boadcast_packet->len);
        data = make_raw_data(*boadcast_packet);
    } else
        show_message("ERROR: Cannot create joined packet for others\n");

    if (data) {
        send_to_clients(client_fd, data, len);
        free(data);
    }
    if (boadcast_packet)
        release(boadcast_packet);
}

void
set_nickname(int client_fd, char* nickname)
{
    client_t* client;
    TAILQ_FOREACH(client, &client_tailq_head, entries) {
        if (client->fd == client_fd) {
            snprintf(client->nickname, MAX_LEN_NICKNAME, "%s", nickname);
            return;
        }
    }
    sprintf(client->nickname, "%s", NO_NAME);
}

packet_t*
prepare_broadcast(char* nickname, member_t member)
{
    char body[MAX_LEN_NICKNAME + 10] = {0};

    if (member == OTHERS) {
        snprintf(body, MAX_LEN_NICKNAME, "%s", nickname);
        sprintf(body + strlen(nickname), "%s", JOINED);
    } else if (member == SENDER)
        sprintf(body, "%s", YOU_JOINED);

    return create_packet(body, BROADCAST);
}

void
send_to_clients(int client_fd, uint8_t* data, int len)
{
    int j;

    for (j = 0; j <= maxfd; j++) {
        if (!FD_ISSET(j, &master_set))
            continue;
        if (j == listen_fd || j == client_fd)
            continue;
        if (send(j, data, len + LEN_HEADER, 0) < 0)
            perror("ERROR: Could not send msg");
    }
}

void
print_clients()
{
    client_t* client;
    TAILQ_FOREACH(client, &client_tailq_head, entries) {
        printf("------------------------------------------\n");
        printf("client info:\n");
        printf("\tID: %d\n", client->fd);
        printf("\tIP Address: %s\n", client->ip);
        if (strlen(client->nickname) > 0)
            printf("\tNickname: %s\n", client->nickname);

        printf("------------------------------------------\n");
    }
}

void
show_client_info(int fd)
{
    client_t* client;
    TAILQ_FOREACH(client, &client_tailq_head, entries) {
        if (client->fd == fd) {
            printf("#################################################\n");
            printf("\tID: %d\n", client->fd);
            printf("\tIP Address: %s\n", client->ip);
            printf("\tlen: %d\n", client->len);
            printf("\toffset: %d\n", client->offset);
            if (strlen(client->nickname) > 0)
                printf("\tNickname: %s\n", client->nickname);
            printf("#################################################\n");
            break;
        }
    }
}

