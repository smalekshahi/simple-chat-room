#ifndef _CLIENT_H
#define _CLIENT_H

#include "common.h"

extern uint8_t verbose;

int initiate_connection(connection_t* talker);
void client_handler(char* nicknam, char* log_path);
int receive_message();
int send_message(char* nickname);
int join(char* nickname);
int check_receive_value(int receive_value);
void close_file(FILE* file_name);
void write_log(const char* message);
void on_stop_client();

#endif
