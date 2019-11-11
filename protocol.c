#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "protocol.h"

int
type_validate(type_t type)
{
    if (type != JOIN && type != MESSAGE && type != BROADCAST) {
        show_message("ERROR: Wrong packet type. %d\n", type);
        return 0;
    }

    return 1;
}

packet_t* 
create_packet(char* data, type_t type)
{
    uint16_t tlen = 0;
    uint16_t ttype = 0;

    packet_t* packet = malloc(sizeof(packet_t));
    if (packet == NULL) {
	perror("ERROR: malloc() failed in create_packet");
	return NULL;
    }

    tlen =  strlen(data) + 1;
    ttype = (int)type;
    packet->len = htons(tlen);
    packet->type = htons(ttype);
    if (type_validate(ttype) == 1) {
        if (strlen(data) > 0 && strlen(data) < MAX_LEN_MSG - LEN_HEADER) {
            packet->body = calloc(tlen, sizeof(uint8_t));
            if (packet->body == NULL) {
                perror("ERROR: calloc() failed in parse");
                return packet;
            }
            memcpy(packet->body, data, tlen);
            return packet;
        }
        show_message("ERROR: Missmatch data length.\n");
    }
    packet->body = NULL;

    return packet;
}

packet_t* 
parse(uint8_t* buffer)
{
    uint16_t tlen = 0;
    uint16_t ttype = 0;
    int offset = 0;

    packet_t* packet = malloc(sizeof(packet_t));
    if (packet == NULL) {
	perror("ERROR: malloc() failed in parse");
	return NULL;
    }

    memcpy(&ttype, buffer, LEN_PACKET_TYPE);
    offset += LEN_PACKET_TYPE;
    memcpy(&tlen, buffer + offset, LEN_PACKET_LENGTH);
    offset += LEN_PACKET_LENGTH;
    packet->type = htons(ttype);
    packet->len = htons(tlen);
    if (type_validate(ttype) == 0) {
        show_message("ERROR: Received data is not in acceptable format. Drop it.\n");
        packet->body = NULL;
        return packet;
    }

    packet->body = calloc(tlen, sizeof(uint8_t));
    if (packet->body == NULL) {
        perror("ERROR: calloc() failed in parse");
        return packet;
    }
    memcpy(packet->body, buffer + offset, tlen);

    return packet;
}

uint8_t* 
make_raw_data(packet_t packet)
{
    uint8_t* buffer;
    int offset = 0;
    uint16_t tlen = 0;
    uint16_t ttype = 0;

    ttype = ntohs(packet.type);
    tlen = ntohs(packet.len);
    buffer = calloc(tlen + LEN_HEADER, sizeof(uint8_t));
    if (buffer == NULL) {
        perror("ERROR: calloc() failed in make_raw_data");
        return NULL;
    }
    memcpy(buffer, (uint8_t* )(&ttype), LEN_PACKET_TYPE);
    offset += LEN_PACKET_TYPE;
    memcpy(buffer + offset, (uint8_t* )(&tlen), LEN_PACKET_LENGTH);
    offset += LEN_PACKET_LENGTH;
    memcpy(buffer + offset, packet.body, tlen);

    return buffer;
}

void
release(packet_t* packet)
{
    if (packet != NULL) {
        if (packet->body != NULL)
            free(packet->body);
        free(packet);
    }
}

void
show_packet(packet_t packet)
{
    if (packet.type != UNKNOWN) {
        printf("------------------------------------------\n");
        printf("Packet info:\n");
        printf("\tpacket.type = %u\n", ntohs(packet.type));
        printf("\tpacket.len = %u\n", ntohs(packet.len));

        if (packet.body != NULL)
            printf("\tpacket.body = %s\n", packet.body);

        printf("------------------------------------------\n");
    } else
        show_message("WARN: Unknown packet to show\n");
}


