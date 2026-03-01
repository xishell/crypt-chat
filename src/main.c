#include "chat_framer.h"
#include "chat_protocol.h"
#include "devices.h"
#include "dtekv-lib.h"
#include "uart.h"

#include "aead.h"
#include "crypto_key.h"

volatile int send_flag = 0;

static chat_framer_t framer;

/* Button interrupt handler */
void my_button_handler(unsigned int button_state) {
    if (button_state & (1 << 0)) {
        send_flag = 1;
    }
}

void my_timer_isr(void) { uart_rx_isr(); }

void init(void) {
    gpio_init();

    display_init();
    display_clear_all();

    button_isr = my_button_handler;
    *BTN_IRQ_MASK = 0x0F;
    *BTN_EDGE_CAPTURE = 0x0F;

    uart_config_t esp_uart;
    esp_uart.tx_pin = 0; /* GPIO 0 -> ESP8266 RX */
    esp_uart.rx_pin = 1; /* GPIO 1 <- ESP8266 TX */
    esp_uart.baud = UART_BAUD_9600;
    uart_init(&esp_uart);

    timer_isr = my_timer_isr;
    uart_rx_init_interrupt(&esp_uart);

    enable_interrupt();

    print("\n=== Encrypted Chat Application ===\n");
    print("System initialized\n");
    print("Press BTN1 to send switch values\n");
    print("Listening for protocol frames...\n\n");

    chat_framer_init(&framer);
}
volatile int total_bytes_received = 0;

void send_user_message(uart_config_t *uart, unsigned char user_id,
                       const unsigned char *message, int msg_len) {
    if (msg_len <= 0 || msg_len > CHAT_MAX_MESSAGE - 1) {
        print("[ERROR] Message too long or empty\n");
        return;
    }

    print("\n[TX] ");
    for (int i = 0; i < msg_len; i++) {
        printc(message[i]);
    }
    print("\n");

    unsigned char payload_with_id[CHAT_MAX_MESSAGE + 1];
    payload_with_id[0] = user_id;
    for (int i = 0; i < msg_len; i++) {
        payload_with_id[i + 1] = message[i];
    }
    int payload_len = msg_len + 1;

    unsigned short crc = crc16_ccitt(payload_with_id, payload_len);

    unsigned char payload[CHAT_MAX_MESSAGE + 3];
    for (int i = 0; i < payload_len; i++) {
        payload[i] = payload_with_id[i];
    }
    payload[payload_len] = (crc >> 8) & 0xFF;
    payload[payload_len + 1] = crc & 0xFF;

    unsigned char stuffed[CHAT_MAX_FRAME];
    int stuffed_len =
        byte_stuff(payload, payload_len + 2, stuffed, CHAT_MAX_FRAME);

    if (stuffed_len > 0) {
        /* Send frame */
        uart_tx_char(uart, CHAT_SYNC_BYTE);
        uart_tx_char(uart, CHAT_SYNC_BYTE);
        uart_tx_char(uart, (unsigned char)stuffed_len);
        uart_tx_bytes(uart, stuffed, stuffed_len);

        print("[OK] Sent\n");
    } else {
        print("[FAIL] Stuffing error\n");
    }

}

/* Send message triggered by button (switch values) */
void send_message(uart_config_t *uart) {
    /* Read switch values as message */
    unsigned int switches = *SW_DATA;

    unsigned char message[32];
    message[0] = 'M';
    message[1] = 'S';
    message[2] = 'G';
    message[3] = ':';
    message[4] = ' ';

    /* Convert switch value to hex string */
    int idx = 5;
    for (int i = 2; i >= 0; i--) {
        unsigned int nibble = (switches >> (i * 4)) & 0xF;
        if (nibble < 10) {
            message[idx++] = '0' + nibble;
        } else {
            message[idx++] = 'A' + (nibble - 10);
        }
    }
    message[idx++] = '\r';
    message[idx++] = '\n';
    int msg_len = idx;

    /* TODO: Encrypt message */

    /* Use the send_user_message helper with board user ID */
    send_user_message(uart, USER_ID_BOARD, message, msg_len);
    led_toggle(0);
}

static int transform_message(unsigned char user_id,
                              const unsigned char *msg, int msg_len,
                              unsigned char *out, int out_size) {
    int alt_len;
    if (user_id == USER_ID_BOARD) {
        alt_len = aead_encrypt_pack(CRYPTO_KEY, msg, msg_len, out, out_size);
        if (alt_len < 0) {
            print("[WARN] ENC fail, dropping message\n");
            return -1;
        }
        print("[ENC]\n");
    } else {
        alt_len = aead_decrypt_unpack(CRYPTO_KEY, msg, msg_len, out, out_size);
        if (alt_len < 0) {
            print("[WARN] DEC fail, dropping message\n");
            return -1;
        }
        print("[DEC]\n");
    }
    return alt_len;
}

static void process_frame(uart_config_t *uart) {
    unsigned char stuffed[CHAT_MAX_FRAME];
    int stuffed_len = chat_framer_take(&framer, stuffed, CHAT_MAX_FRAME);

    unsigned char unstuffed[CHAT_MAX_FRAME];
    int unstuffed_len = byte_unstuff(stuffed, stuffed_len, unstuffed, CHAT_MAX_FRAME);

    if (unstuffed_len < 4) {
        print("\n[ERROR] Invalid frame\n");
        return;
    }

    unsigned char user_id = unstuffed[0];
    int msg_with_id_len = unstuffed_len - 2;
    int msg_len = msg_with_id_len - 1;

    unsigned short rx_crc = ((unsigned short)unstuffed[msg_with_id_len] << 8) |
                             unstuffed[msg_with_id_len + 1];
    unsigned short calc_crc = crc16_ccitt(unstuffed, msg_with_id_len);

    if (rx_crc != calc_crc) {
        print("\n[ERROR] CRC mismatch\n");
        return;
    }

    print("\n[RX from user ");
    print_hex(user_id, 2);
    print("] ");
    for (int i = 1; i <= msg_len; i++)
        printc(unstuffed[i]);
    print("\n");

    unsigned char alt[CHAT_MAX_MESSAGE];
    int alt_len = transform_message(user_id, &unstuffed[1], msg_len, alt, CHAT_MAX_MESSAGE);

    if (alt_len < 0) {
        print("[ERROR] Crypto transform failed, not sending\n");
        return;
    }

    send_user_message(uart, user_id, alt, alt_len);
    led_toggle(0);
}

int main(void) {
    uart_config_t esp_uart;

    /* Initialize system */
    init();

    /* Store UART config for main loop */
    esp_uart.tx_pin = 0;
    esp_uart.rx_pin = 1;
    esp_uart.baud = UART_BAUD_9600;

    /* Main loop */
    while (1) {
        /* Collect and process */
        while (uart_rx_available() > 0) {
            int c = uart_rx_getc();
            if (c == -1)
                break;
            total_bytes_received++;
            chat_framer_result_t r =
                chat_framer_feed(&framer, (unsigned char)c);
            if (r == CHAT_FRAMER_FRAME_READY)
                process_frame(&esp_uart);
        }

        /* Check if send button pressed */
        if (send_flag) {
            send_flag = 0;
            send_message(&esp_uart);
        }

    /* Timeout recovery */
        chat_framer_tick(&framer, 1);
        chat_framer_result_t tr = chat_framer_check_timeout(&framer, 1000);
        if (tr == CHAT_FRAMER_TIMEOUT) {
            print("[WARN] RX frame timeout, resetting state\n");
        }

        /* Breathe a little to keep CPU use reasonable */
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    return 0;
}
