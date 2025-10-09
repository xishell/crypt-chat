#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#include "uart.h"

/* Chat protocol helpers (CRC16 + byte stuffing). */

/* CRC16-CCITT (poly 0x1021, init 0xFFFF) */
unsigned short crc16_ccitt(const unsigned char *data, int len);

/* Byte stuffing helpers */
int byte_stuff(const unsigned char *input, int input_len, unsigned char *output, int max_output);
int byte_unstuff(const unsigned char *input, int input_len, unsigned char *output, int max_output);

#endif /* CHAT_PROTOCOL_H */
