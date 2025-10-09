/* Chat protocol helpers. */
#include "chat_protocol.h"

unsigned short crc16_ccitt(const unsigned char *data, int len) {
    unsigned short crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= ((unsigned short)data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

int byte_stuff(const unsigned char *input, int input_len, unsigned char *output, int max_output) {
    int out_idx = 0;
    for (int i = 0; i < input_len; i++) {
        if (out_idx >= max_output - 1) {
            return -1;
        }
        if (input[i] == CHAT_SYNC_BYTE) {
            output[out_idx++] = CHAT_ESCAPE_BYTE;
            if (out_idx >= max_output) return -1;
            output[out_idx++] = 0x00;
        } else if (input[i] == CHAT_ESCAPE_BYTE) {
            output[out_idx++] = CHAT_ESCAPE_BYTE;
            if (out_idx >= max_output) return -1;
            output[out_idx++] = 0x01;
        } else {
            output[out_idx++] = input[i];
        }
    }
    return out_idx;
}

int byte_unstuff(const unsigned char *input, int input_len, unsigned char *output, int max_output) {
    int out_idx = 0;
    int i = 0;
    while (i < input_len) {
        if (out_idx >= max_output) {
            return -1;
        }
        if (input[i] == CHAT_ESCAPE_BYTE) {
            i++;
            if (i >= input_len) {
                return -1;
            }
            if (input[i] == 0x00) {
                output[out_idx++] = CHAT_SYNC_BYTE;
            } else if (input[i] == 0x01) {
                output[out_idx++] = CHAT_ESCAPE_BYTE;
            } else {
                return -1;
            }
            i++;
        } else {
            output[out_idx++] = input[i++];
        }
    }
    return out_idx;
}
