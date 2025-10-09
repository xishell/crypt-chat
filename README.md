# Crypt-Chat on DTEK‑V

RISC‑V firmware and tools for a UART‑over‑ESP8266 chat demo using a robust framed protocol (CRC16‑CCITT + byte stuffing). Includes a Python client for interacting over telnet.

## Build

Prereqs:

- RISC‑V toolchain (`riscv32-unknown-elf-*`)
- `dtekv-run` and Intel `jtagd`

Commands:

- `make` (or `make release`) → `build/main.bin`
- `make debug` → no optimizations, with symbols
- `make run` → build + upload

## Run the Client

Use the single protocol client:

```bash
python3 chat_client_protocol.py 192.168.4.1 [port]
```

Notes:

- Frame: `[0xAA 0xAA][LEN][STUFFED_DATA]`; unstuffed: `[USER_ID][MESSAGE][CRC16]` (CRC over USER_ID+MESSAGE)
- Client `USER_ID=0x02`; board `USER_ID=0x01`
- Client drops frames with its own `USER_ID`

## Source Layout

- `src/boot.S` — boot and trap handlers
- `src/dtekv-lib.c` — JTAG UART I/O, exceptions, interrupts, delay
- `src/devices.c` — LEDs, switches, button, 7‑seg, GPIO
- `src/uart.c` — bit‑banged UART: TX (busy‑wait + accurate timer), RX ISR + ring buffer
- `src/chat_protocol.c` — CRC16‑CCITT and byte stuffing
- `src/chat_framer.c` — frame collection state machine (+timeout)
- `src/main.c` — app glue: use framer, validate, echo; BTN1 sends message
- `include/` — headers
- `PROTOCOL.md` — protocol details (concise)
- `chat_client_protocol.py` — Python telnet client (protocol)
