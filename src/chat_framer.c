/* Byte-wise framer for the on-wire format. */
#include "chat_framer.h"

void chat_framer_init(chat_framer_t *f) {
    f->state = CHAT_FRAMER_STATE_PLAINTEXT;
    f->index = 0;
    f->expected_length = 0;
    f->inactivity_ticks = 0;
    f->has_frame = 0;
    f->ready_length = 0;
}

chat_framer_result_t chat_framer_feed(chat_framer_t *f, unsigned char byte) {
    f->inactivity_ticks = 0;

    switch (f->state) {
    case CHAT_FRAMER_STATE_PLAINTEXT:
        if (byte == CHAT_SYNC_BYTE) {
            f->state = CHAT_FRAMER_STATE_SYNC1;
        }
        return CHAT_FRAMER_NEED_MORE;

    case CHAT_FRAMER_STATE_SYNC1:
        if (byte == CHAT_SYNC_BYTE) {
            f->state = CHAT_FRAMER_STATE_SYNC2;
        } else {
            f->state = CHAT_FRAMER_STATE_PLAINTEXT;
        }
        return CHAT_FRAMER_NEED_MORE;

    case CHAT_FRAMER_STATE_SYNC2:
        if (byte > 0 && byte <= CHAT_MAX_FRAME) {
            f->expected_length = byte;
            f->index = 0;
            f->state = CHAT_FRAMER_STATE_DATA;
        } else {
            f->state = CHAT_FRAMER_STATE_PLAINTEXT;
        }
        return CHAT_FRAMER_NEED_MORE;

    case CHAT_FRAMER_STATE_DATA:
        f->buffer[f->index++] = byte;
        if (f->index >= f->expected_length) {
            f->has_frame = 1;
            f->ready_length = f->expected_length;
            f->state = CHAT_FRAMER_STATE_PLAINTEXT;
            return CHAT_FRAMER_FRAME_READY;
        }
        return CHAT_FRAMER_NEED_MORE;
    }
    return CHAT_FRAMER_ERROR;
}

void chat_framer_tick(chat_framer_t *f, unsigned int ticks) {
    if (f->state == CHAT_FRAMER_STATE_PLAINTEXT) return;
    unsigned int next = f->inactivity_ticks + ticks;
    if (next < f->inactivity_ticks) {
        next = f->inactivity_ticks; /* saturate on wrap */
    }
    f->inactivity_ticks = next;
}

chat_framer_result_t chat_framer_check_timeout(chat_framer_t *f, unsigned int timeout_ticks) {
    if (f->state == CHAT_FRAMER_STATE_PLAINTEXT) return CHAT_FRAMER_NEED_MORE;
    if (f->inactivity_ticks > timeout_ticks) {
        f->state = CHAT_FRAMER_STATE_PLAINTEXT;
        f->index = 0;
        f->expected_length = 0;
        f->has_frame = 0;
        f->ready_length = 0;
        f->inactivity_ticks = 0;
        return CHAT_FRAMER_TIMEOUT;
    }
    return CHAT_FRAMER_NEED_MORE;
}

int chat_framer_take(chat_framer_t *f, unsigned char *out, int max_len) {
    if (!f->has_frame) return -1;
    if (f->ready_length > max_len) return -2;
    for (int i = 0; i < f->ready_length; i++) {
        out[i] = f->buffer[i];
    }
    int len = f->ready_length;
    f->has_frame = 0;
    f->ready_length = 0;
    return len;
}
