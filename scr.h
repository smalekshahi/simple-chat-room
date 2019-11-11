#ifndef _CCR_H
#define _CCR_H

#include "common.h"

#define ESSENTIAL_ARGS 5

void initialize_args(args_t* input_values);
void parse_options(int argc, char* argv[], args_t* inputs);
void usage(char* program_name);
void add_default_logfile(char* log_path);
int handler(args_t input_values);
int start_server(connection_t* connector);
int start_client(connection_t* connector, args_t input_values);
void close_fd(int* fd);

#endif

