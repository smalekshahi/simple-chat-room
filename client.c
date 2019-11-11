#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>
#include <signal.h>
#include "protocol.h"
#include "client.h"

#define INPUT_FD 0

#define MAX(a,b) (((a)>(b))?(a):(b))

char reply[MAX_LEN_MSG] = {0};
char message[MAX_LEN_MSG - LEN_HEADER] = {0};
FILE *log_file;
int client_fd;
volatile sig_atomic_t client_error_in_progress = 1;

void
client_error_signal (int sig) {
    if (client_error_in_progress)
        raise (sig);
    client_error_in_progress = 1;
    on_stop_client();
    signal (sig, SIG_DFL);
    raise (sig);
}

int
initiate_connection(connection_t *talker)
{
    int on = 1;

    if (ioctl(talker->fd, FIONBIO, (char *)&on) < 0) {
        perror("ERROR: client ioctl() failed");
        return 1;
    }

    if (connect(talker->fd, (const struct sockaddr*)&talker->address, sizeof(talker->address)) < 0) {
        if (errno != EINPROGRESS) {
            perror("ERROR: could not connect to server");
            return 1;
        }
    }

    client_fd = talker->fd;
    return 0;
}

void
client_handler(char *nickname, char *log_path)
{
    fd_set rfds;
    fd_set wfds;
    int maxfd, retnfd;

    signal(SIGINT, &client_error_signal);

    log_file = fopen(log_path, "a");
    if (log_file == NULL) {
        show_message("ERROR: There is no such path %s\n", log_path);
        return;
    }

    FD_ZERO(&rfds);
    maxfd = MAX(INPUT_FD, client_fd);
    join(nickname);

    while (1) {
        int i;
        FD_SET(INPUT_FD, &rfds);
        FD_SET(client_fd, &rfds);
        retnfd = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (retnfd == -1) {
            perror("ERROR: select() failed");
            break;
        }

        if (retnfd == 0) {
            show_message("WARN: Timeout on select()\n");
            continue;
        }

        for (i = 0; i <= maxfd; i++) {
            if (!FD_ISSET(i, &rfds))
                continue;
            if (i == INPUT_FD) {
                if (send_message(nickname) != 0)
                    return close_file(log_file);
            } else if (i == client_fd) {
                if (receive_message() != 0)
                    return close_file(log_file);
            } else
                show_message("WARN: Unknown file descriptor\n");
        }
    }
    return close_file(log_file);
}

int
send_message(char *nickname)
{
    packet_t *packet = NULL;
    int8_t *data = NULL;
    int len = 0, ret = 0;

    snprintf(message, MAX_LEN_NICKNAME, "%s", nickname);
    strcat(message, ": ");
    len = strlen(message);
    fgets(message + len, MAX_LEN_MSG - LEN_HEADER - len, stdin);
    packet = create_packet(message, MESSAGE);
    data = make_raw_data(*packet);

    if (send(client_fd, data, ntohs(packet->len) + LEN_HEADER, 0) < 0) {
        show_message("ERROR: could not send msg to server\n");
        ret = 1;
    }
    write_log(message);

    if (packet)
        release(packet);

    if (data)
        free(data);

    return ret;
}

int
receive_message()
{
    char buffer[BUFFER_SIZE];
    int rc, offset = 0;
    memset(reply, 0, sizeof(reply));

    while(1) {
        rc = recv(client_fd, buffer, BUFFER_SIZE, 0);
	if (rc <= 0) {
            if (check_receive_value(rc) != 0)
                return 1;
            break;
	}

        buffer[rc] = '\0';
        if (offset + rc < MAX_LEN_MSG) {
            memcpy(reply + offset, buffer, rc + 1);
            offset += rc;
        } else {
            show_message("ERROR: Received message is too long\n");
            return 1;
        }
    }

    if (offset > 0) {
        packet_t *packet = parse(reply);
        if (ntohs(packet->type) == MESSAGE || ntohs(packet->type) == BROADCAST) {
            printf("%s", packet->body);
            write_log(packet->body);
        }

        if (packet)
            release(packet);
    }

    return 0;
}

int
check_receive_value(int value)
{
    if (value < 0 && errno != EWOULDBLOCK) {
        perror("ERROR: recv() failed");
        return 1;
    }

    if (value == 0) {
        show_message("WARN: Server close connection\n");
        return 1;
    }

    return 0;
}

int
join(char *nickname)
{
    packet_t *packet = NULL;
    int8_t *data = NULL;

    snprintf(message, MAX_LEN_NICKNAME, "%s", nickname);
    packet = create_packet(message, JOIN);
    data = make_raw_data(*packet);

    if (send(client_fd, data, ntohs(packet->len) + LEN_HEADER, 0) < 0)
        perror("ERROR: Cannot send join to server\n");
    if (packet)
        release(packet);
    if (data)
        free(data);

    return 0;
}

void
close_file(FILE* file_name)
{
    if (file_name != NULL) {
        fclose(file_name);
	file_name = NULL;
    }
}

void
write_log(const char* message)
{
    char ftime[20];
    time_t now = time(NULL);
    struct tm timeinfo;
    struct tm* temp = localtime_r(&now, &timeinfo);

    strftime(ftime, sizeof(ftime), "%d/%m/%Y %H:%M:%S", &timeinfo);
    fprintf(log_file, "(%s) %s", ftime, message);
}

void
on_stop_client()
{
    if(client_fd >= 0) {
	close(client_fd);
	client_fd = -1;
    }
    close_file(log_file);
}
