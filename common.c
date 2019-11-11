#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include "common.h"

int
create_endpoint(connection_t* connector, args_t input_values)
{
    if ((connector->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("ERROR: Could not create socket");
        return 1;
    }

    memset(&connector->address, 0, sizeof(struct sockaddr_in));
    connector->address.sin_family = AF_INET;
    inet_pton(AF_INET, (const unsigned char* )input_values.ip, &connector->address.sin_addr);
    connector->address.sin_port = htons(input_values.port);

    return 0;
}

int
ipvalidte(char* ip)
{
    unsigned char buf[sizeof(struct sockaddr_in)];
    int ret;

    ret = inet_pton(AF_INET, ip, buf);

    if (ret > 0)
        return 1;

    if (ret == 0)
        fprintf(stderr, "Invalis ip address\n");
    else
        perror("Invalis ip address");

    return 0;
}

void
show_message(const char* msg, ...)
{
    va_list input_args;

    if (verbose == 1) {
        va_start(input_args, msg);
        vprintf(msg, input_args);
        va_end(input_args);
    }
}
