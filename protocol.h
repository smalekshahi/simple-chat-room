#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <stdint.h>

#define MAX_LEN_MSG 65536
#define LEN_PACKET_TYPE 2
#define LEN_PACKET_LENGTH 2
#define LEN_HEADER LEN_PACKET_TYPE + LEN_PACKET_LENGTH

typedef enum
{
    UNKNOWN = 0,
    JOIN,
    MESSAGE,
    BROADCAST
} type_t;

typedef struct
{
    uint16_t len;
    uint16_t type;
    uint8_t* body;
} packet_t;

extern uint8_t verbose;

int type_validate(type_t type);
packet_t* create_packet(char* data, type_t type);
void release(packet_t* packet);
packet_t* parse(uint8_t* buffer);
void show_packet(packet_t packet);
packet_t empty();
uint8_t* make_raw_data(packet_t packet);

#endif

