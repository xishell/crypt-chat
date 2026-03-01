# Crypt-Chat

End-to-end encrypted chat over WiFi, running on a bare-metal RISC-V microcontroller with no OS and no standard library.

A DTEK-V board communicates with a Python client over a bit-banged software UART through an ESP8266 WiFi bridge. All messages are authenticated and encrypted using ChaCha20-Poly1305 AEAD, implemented from scratch in C.

```
┌──────────────┐   bit-banged    ┌──────────┐   WiFi/TCP    ┌──────────────┐
│   DTEK-V     │───── UART ────▶│ ESP8266  │◀───────────▶│ Python       │
│   (RV32IM)   │   9600 baud     │ (bridge) │   telnet:23   │ Client       │
│              │                 │          │               │              │
│ ChaCha20     │                 │          │               │ Same C code  │
│ Poly1305     │                 │          │               │ via ctypes   │
│ CRC16+frame  │                 │          │               │              │
└──────────────┘                 └──────────┘               └──────────────┘
```

## Features

- **Software UART** — TX via busy-wait, RX via timer ISR with 8x oversampling and lock-free ring buffer
- **ChaCha20-Poly1305 AEAD** — RFC 8439 authenticated encryption, implemented without any library dependencies
- **Framed protocol** — sync bytes, CRC16-CCITT integrity checks, byte stuffing, and timeout recovery
- **Shared C crypto library** — the Python client calls the exact same C crypto and protocol code via `ctypes`, ensuring no implementation drift

## Building

Requires a `riscv32-unknown-elf` toolchain, Meson, and Ninja.

```bash
# Configure (only needed once)
meson setup build --cross-file cross/riscv32.ini

# Build firmware + host shared library
ninja -C build
```

This produces:

- `build/crypt-chat.bin` — firmware binary for the DTEK-V board
- `build/libchatcrypto.so` — host shared library used by the Python client

To upload firmware to the board:

```bash
ninja -C build upload
```

## Running the Python Client

```bash
python3 chat_client_protocol.py <ESP8266_IP> [port]
```

Options:

- `--user-id 0x02` — client user ID (default)
- `--user-id 0x00` — act as board (send plaintext, let the board encrypt)
- `--key <hex>` — override the 32-byte shared key (64 hex characters)

## Protocol

```
Wire format:  [0xAA] [0xAA] [LEN] [STUFFED_PAYLOAD]

After unstuffing:  [USER_ID] [MESSAGE...] [CRC_HI] [CRC_LO]
```

- **Sync:** two `0xAA` bytes mark frame start
- **Byte stuffing:** `0xAA` → `0xAB 0x00`, `0xAB` → `0xAB 0x01`
- **CRC16-CCITT** over `USER_ID + MESSAGE` (poly `0x1021`, init `0xFFFF`)
- **Encryption:** message payload is `nonce(12) + ciphertext(n) + tag(16)`
- **Timeout:** partial frames are dropped after ~1 second of inactivity

See [PROTOCOL.md](PROTOCOL.md) for details.

## Source Layout

**Firmware (bare-metal C, no libc):**

| File                  | Purpose                                                           |
| --------------------- | ----------------------------------------------------------------- |
| `src/boot.S`          | Boot, trap vector, ISR dispatch                                   |
| `src/dtekv-lib.c`     | JTAG UART I/O, exception/interrupt handlers                       |
| `src/devices.c`       | LEDs, switches, buttons, 7-segment display, GPIO                  |
| `src/uart.c`          | Bit-banged UART: TX busy-wait, RX 8x oversampled ISR, ring buffer |
| `src/chat_protocol.c` | CRC16-CCITT, byte stuffing/unstuffing                             |
| `src/chat_framer.c`   | Frame state machine with timeout recovery                         |
| `src/chacha20.c`      | ChaCha20 stream cipher (RFC 8439)                                 |
| `src/poly1305.c`      | Poly1305 MAC                                                      |
| `src/aead.c`          | ChaCha20-Poly1305 AEAD encrypt/decrypt                            |
| `src/main.c`          | Application: receive frames, encrypt/decrypt, relay               |

**Host (shared library + Python client):**

| File                      | Purpose                                                 |
| ------------------------- | ------------------------------------------------------- |
| `build/libchatcrypto.so`  | Crypto + protocol C code compiled for the host          |
| `chat_client_protocol.py` | Python client using `ctypes` to call the shared library |
