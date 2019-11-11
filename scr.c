#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include "server.h"
#include "client.h"
#include "protocol.h"
#include "scr.h"

uint8_t verbose = 0;

int
main(int argc, char* argv[])
{
    args_t input_values;

    initialize_args(&input_values);
    parse_options(argc, argv, &input_values);

    if (input_values.listener == 0 && strlen(input_values.nickname) == 0) {
        printf("WARN: Nickname is mandatory.\n");
        return 1;
    }

    if (argc > ESSENTIAL_ARGS && input_values.verbose == 1) {
        const char* name;
        input_values.listener == 1 ? (name = "Server") : (name = "Client");
        printf("%s is running. ip: %s port: %d\n", name, input_values.ip,
               input_values.port);
        verbose = 1;
    }

    if (strlen(input_values.log_path) == 0)
	add_default_logfile(input_values.log_path);

    return handler(input_values);
}

void
initialize_args(args_t* input_values)
{
    input_values->listener = 0;
    memset(input_values->ip, 0, INET_ADDRSTRLEN);
    input_values->port = 0;
    memset(input_values->nickname, 0, MAX_LEN_NICKNAME) ;
    input_values->verbose = 0;
    memset(input_values->log_path, 0, MAX_LEN_PATH) ;
}

void
parse_options(int argc, char* argv[], args_t* input_values)
{
    char opt;
    int option_index = 0, port, index;
    int this_option_optind = optind ? optind : 1;
    static struct option long_options[] =
    {
        {"listen", no_argument, NULL, 'l'},
        {"ip", required_argument, NULL, 'i'},
        {"port", required_argument, NULL, 'p'},
        {"nickname", required_argument, NULL, 'n'},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {0, no_argument, NULL, 0}
    };

    if (argc < ESSENTIAL_ARGS) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    while ((opt = getopt_long(argc, argv, "i:p:n:f:lvh", long_options,
                              &option_index)) != -1) {
        if (optarg && optarg[0] == '-') {
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }

	switch (opt) {
        case 'l':
            input_values->listener = 1;
            break;

        case 'p':
            if ((port = strtoul(optarg, (char **)NULL, 10)) == 0) {
                usage(argv[0]);
                exit(EXIT_FAILURE);
            }

            input_values->port = port;
            break;

        case 'i':
            if (ipvalidte(optarg) == 0)
                exit(EXIT_FAILURE);
            else
                strncpy(input_values->ip, optarg, INET_ADDRSTRLEN);
            break;

        case 'n':
            snprintf(input_values->nickname, MIN(strlen(optarg) + 1, MAX_LEN_NICKNAME),
                     "%s", optarg);
            break;

        case 'f':
            strncpy(input_values->log_path, optarg, MAX_LEN_PATH);
            break;

        case 'v':
            input_values->verbose = 1;
            break;

        case '?':
        case 'h':
            usage(argv[0]);
            exit(EXIT_FAILURE);

        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (input_values->listener == 1 && strlen(input_values->nickname) > 0) {
        printf("WARN: Incompatible usage of -n and -l.\nServer cannot set nickname.\n");
        exit(EXIT_FAILURE);
    }

    for (index = optind; index < argc; index++)
        printf("Non-option argument %s\n", argv[index]);
}

void
usage(char* program_name)
{
    printf("Usage: %s [-l listen][-i ip][-p port][-n nickname][-f file][-v verbose]\n",
           program_name);
}

void
add_default_logfile(char* log_path)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    struct tm* temp = localtime_r(&now, &timeinfo);

    strftime(log_path, MAX_LEN_PATH, "./log_%Y%m%d_%H%M%S", &timeinfo);
}

int
handler(args_t input_values)
{
    connection_t connector;

    if (create_endpoint(&connector, input_values) != 0) {
        close_fd(&connector.fd);
        return 1;
    }

    if (input_values.listener == 1)
        return start_server(&connector);
    else 
        return start_client(&connector, input_values);
}

int 
start_server(connection_t* connector)
{
    if (listen_connections(connector) != 0) {
        close_fd(&connector->fd);  
        return 1;
    }
    server_handler();

    return 0;
}

int 
start_client(connection_t* connector, args_t input_values)
{
    if (initiate_connection(connector) != 0) {
        close_fd(&connector->fd);  
        return 1;
    }
    client_handler(input_values.nickname, input_values.log_path);

    return 0;
}

void
close_fd(int* fd)
{
    if (*fd >= 0)
        close(*fd);
    *fd = -1;
}
