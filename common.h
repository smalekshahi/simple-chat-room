#ifndef _COMMON_H
#define _COMMON_H

#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_LEN_NICKNAME 128
#define BUFFER_SIZE 1024
#define MAX_LEN_PATH 128

#define MIN(a,b) (((a)<(b))?(a):(b))

typedef struct
{
    int listener;
    unsigned char ip[INET_ADDRSTRLEN];
    int port;
    char nickname[MAX_LEN_NICKNAME];
    short verbose;
    char log_path[MAX_LEN_PATH];
} args_t;

typedef struct
{
    int fd;
    struct sockaddr_in address;
} connection_t;

extern uint8_t verbose;

int create_endpoint(connection_t* connector, args_t input_values);
int ipvalidte(char* ip);
void show_message(const char* msg, ...);

#endif
