#ifndef CHAT_FRAMER_H
#define CHAT_FRAMER_H

#include "uart.h"

typedef enum {
    CHAT_FRAMER_STATE_PLAINTEXT = 0,
    CHAT_FRAMER_STATE_SYNC1,
    CHAT_FRAMER_STATE_SYNC2,
    CHAT_FRAMER_STATE_DATA
} chat_framer_state_t;

typedef enum {
    CHAT_FRAMER_NEED_MORE = 0,
    CHAT_FRAMER_FRAME_READY = 1,
    CHAT_FRAMER_ERROR = -1,
    CHAT_FRAMER_TIMEOUT = 2
} chat_framer_result_t;

typedef struct {
    chat_framer_state_t state;
    unsigned char buffer[CHAT_MAX_FRAME];
    int index;
    int expected_length;
    unsigned int inactivity_ticks;
    int has_frame;
    int ready_length;
} chat_framer_t;

void chat_framer_init(chat_framer_t *f);

/* Feed next byte. */
chat_framer_result_t chat_framer_feed(chat_framer_t *f, unsigned char byte);

/* Advance inactivity counter. */
void chat_framer_tick(chat_framer_t *f, unsigned int ticks);

/* Drop partial frame on timeout. */
chat_framer_result_t chat_framer_check_timeout(chat_framer_t *f, unsigned int timeout_ticks);

/* Copy stuffed frame; clear ready flag. */
int chat_framer_take(chat_framer_t *f, unsigned char *out, int max_len);

#endif /* CHAT_FRAMER_H */
