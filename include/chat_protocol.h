#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

/* Protocol constants */
#define CHAT_SYNC_BYTE   0xAA
#define CHAT_ESCAPE_BYTE 0xAB
#define CHAT_MAX_MESSAGE 127
#define CHAT_MAX_FRAME   256

/* User IDs */
#define USER_ID_BOARD  0x00
#define USER_ID_CLIENT 0x02

/* CRC16-CCITT (poly 0x1021, init 0xFFFF) */
unsigned short crc16_ccitt(const unsigned char *data, int len);

/* Byte stuffing helpers */
int byte_stuff(const unsigned char *input, int input_len, unsigned char *output, int max_output);
int byte_unstuff(const unsigned char *input, int input_len, unsigned char *output, int max_output);

#endif /* CHAT_PROTOCOL_H */
