#!/usr/bin/env python3
"""
Protocol Chat Client - For Remote User (User1)
Implements full protocol: CRC16, byte stuffing, encryption support
Use this for remote user who wants end-to-end encrypted chat

Usage:
    python3 chat_client_protocol.py <ESP8266_IP> [port]

Example:
    python3 chat_client_protocol.py 192.168.4.1 23
"""

import sys
import argparse
import ctypes
import socket
import select
import threading
import os
from enum import Enum
from pathlib import Path

# Protocol constants (must match chat_protocol.h)
CHAT_SYNC_BYTE = 0xAA
CHAT_ESCAPE_BYTE = 0xAB
CHAT_MAX_MESSAGE = 127
CHAT_MAX_FRAME = 256

# User IDs
USER_ID_BOARD = 0x00
USER_ID_CLIENT = 0x02

# Demo key (32 bytes) shared with the board AEAD demo.
KEY = bytearray([
    0x60,0x61,0x62,0x63, 0x64,0x65,0x66,0x67,
    0x68,0x69,0x6A,0x6B, 0x6C,0x6D,0x6E,0x6F,
    0x70,0x71,0x72,0x73, 0x74,0x75,0x76,0x77,
    0x78,0x79,0x7A,0x7B, 0x7C,0x7D,0x7E,0x7F,
])


# ---- Load shared C library ----

def _find_libchatcrypto():
    """Find libchatcrypto.so relative to this script."""
    script_dir = Path(__file__).resolve().parent
    candidates = [
        script_dir / "build" / "libchatcrypto.so",
        script_dir / "builddir" / "libchatcrypto.so",
    ]
    for path in candidates:
        if path.exists():
            return str(path)
    raise FileNotFoundError(
        "libchatcrypto.so not found. Build it with: meson setup build --cross-file cross/riscv32.ini && ninja -C build"
    )

_lib = ctypes.CDLL(_find_libchatcrypto())

# crc16_ccitt(const unsigned char *data, int len) -> unsigned short
_lib.crc16_ccitt.argtypes = [ctypes.c_char_p, ctypes.c_int]
_lib.crc16_ccitt.restype = ctypes.c_ushort

# byte_stuff(input, input_len, output, max_output) -> int
_lib.byte_stuff.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
_lib.byte_stuff.restype = ctypes.c_int

# byte_unstuff(input, input_len, output, max_output) -> int
_lib.byte_unstuff.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
_lib.byte_unstuff.restype = ctypes.c_int

# aead_encrypt_pack(key, pt, pt_len, out, max_out) -> int
_lib.aead_encrypt_pack.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
_lib.aead_encrypt_pack.restype = ctypes.c_int

# aead_decrypt_unpack(key, in, in_len, pt, max_pt) -> int
_lib.aead_decrypt_unpack.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
_lib.aead_decrypt_unpack.restype = ctypes.c_int


# ---- Wrappers around C functions ----

def crc16_ccitt(data: bytes) -> int:
    return _lib.crc16_ccitt(data, len(data))

def byte_stuff(data: bytes) -> bytes | None:
    out = ctypes.create_string_buffer(CHAT_MAX_FRAME)
    n = _lib.byte_stuff(data, len(data), out, CHAT_MAX_FRAME)
    if n < 0:
        return None
    return out.raw[:n]

def byte_unstuff(data: bytes) -> bytes | None:
    out = ctypes.create_string_buffer(CHAT_MAX_FRAME)
    n = _lib.byte_unstuff(data, len(data), out, CHAT_MAX_FRAME)
    if n < 0:
        return None
    return out.raw[:n]

def aead_encrypt(key: bytes, plaintext: bytes) -> bytes | None:
    max_out = 12 + len(plaintext) + 16
    out = ctypes.create_string_buffer(max_out)
    n = _lib.aead_encrypt_pack(key, plaintext, len(plaintext), out, max_out)
    if n < 0:
        return None
    return out.raw[:n]

def aead_decrypt(key: bytes, packed: bytes) -> bytes | None:
    if len(packed) < 28:
        return None
    max_pt = len(packed) - 28
    out = ctypes.create_string_buffer(max_pt + 1)  # +1 to avoid zero-length buffer
    n = _lib.aead_decrypt_unpack(key, packed, len(packed), out, max_pt + 1)
    if n < 0:
        return None
    return out.raw[:n]


# ---- Protocol framing ----

class RxState(Enum):
    PLAINTEXT = 0
    SYNC1 = 1
    SYNC2 = 2
    DATA = 3

def encode_frame(user_id: int, message: bytes) -> bytes:
    """Encode message into protocol frame: [0xAA 0xAA] [LEN] [STUFFED_DATA]
    where STUFFED_DATA contains: [USER_ID] [MESSAGE...] [CRC_HI] [CRC_LO]"""
    payload_with_id = bytes([user_id]) + message
    crc = crc16_ccitt(payload_with_id)
    payload = payload_with_id + bytes([(crc >> 8) & 0xFF, crc & 0xFF])

    stuffed = byte_stuff(payload)
    if stuffed is None or len(stuffed) > CHAT_MAX_FRAME:
        raise ValueError("Frame too large after stuffing")

    return bytes([CHAT_SYNC_BYTE, CHAT_SYNC_BYTE, len(stuffed)]) + stuffed

def decode_frame(stuffed_data: bytes) -> tuple:
    """Decode stuffed frame data, verify CRC, return (user_id, message) or (None, None)"""
    unstuffed = byte_unstuff(stuffed_data)
    if unstuffed is None or len(unstuffed) < 4:
        return (None, None)

    user_id = unstuffed[0]
    msg_with_id_len = len(unstuffed) - 2
    message = unstuffed[1:msg_with_id_len]
    received_crc = (unstuffed[msg_with_id_len] << 8) | unstuffed[msg_with_id_len + 1]
    calculated_crc = crc16_ccitt(unstuffed[:msg_with_id_len])

    if received_crc != calculated_crc:
        print(f"[DEBUG] CRC mismatch: RX={received_crc:04X} Calc={calculated_crc:04X}")
        return (None, None)

    return (user_id, message)


# ---- Chat client ----

class ChatClient:
    def __init__(self, host, port=23, user_id=USER_ID_CLIENT):
        self.host = host
        self.port = port
        self.sock = None
        self.user_id = user_id
        self.stop_event = threading.Event()

        # Protocol state machine
        self.rx_state = RxState.PLAINTEXT
        self.rx_frame_buffer = bytearray()
        self.rx_expected_length = 0

    @property
    def running(self):
        return not self.stop_event.is_set()

    def stop(self):
        self.stop_event.set()

    def connect(self):
        """Connect to ESP8266 telnet server"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            print(f"[Connected to {self.host}:{self.port}]")
            print("[Type messages to send (will be encrypted)]")
            print("[Messages from board will be decrypted and displayed]")
            print("[Press Ctrl+C to quit]\n")
            return True
        except Exception as e:
            print(f"[ERROR] Could not connect: {e}")
            return False

    def _try_decrypt(self, message):
        """Attempt AEAD decryption on a received message. Returns plaintext or original."""
        if self.user_id == USER_ID_BOARD or len(message) < 28:
            return message
        result = aead_decrypt(bytes(KEY), message)
        if result is not None:
            return result
        return message

    def process_rx_byte(self, byte):
        """Process received byte with state machine"""
        if self.rx_state == RxState.PLAINTEXT:
            if byte == CHAT_SYNC_BYTE:
                self.rx_state = RxState.SYNC1
            else:
                try:
                    print(chr(byte), end='', flush=True)
                except ValueError:
                    pass

        elif self.rx_state == RxState.SYNC1:
            if byte == CHAT_SYNC_BYTE:
                self.rx_state = RxState.SYNC2
            else:
                self.rx_state = RxState.PLAINTEXT
                try:
                    print(chr(byte), end='', flush=True)
                except ValueError:
                    pass

        elif self.rx_state == RxState.SYNC2:
            if 0 < byte <= CHAT_MAX_FRAME:
                self.rx_expected_length = byte
                self.rx_frame_buffer = bytearray()
                self.rx_state = RxState.DATA
            else:
                self.rx_state = RxState.PLAINTEXT

        elif self.rx_state == RxState.DATA:
            self.rx_frame_buffer.append(byte)

            if len(self.rx_frame_buffer) >= self.rx_expected_length:
                user_id, message = decode_frame(bytes(self.rx_frame_buffer))

                if message is not None and user_id != self.user_id:
                    message = self._try_decrypt(message)
                    try:
                        msg_str = message.decode('utf-8')
                        print(f"\n[RX from user 0x{user_id:02X}] {msg_str}")
                    except UnicodeDecodeError:
                        print(f"\n[RX from user 0x{user_id:02X}] <binary: {len(message)} bytes>")

                self.rx_state = RxState.PLAINTEXT
                self.rx_frame_buffer = bytearray()

    def receive_thread(self):
        """Thread to receive and decode messages from server"""
        while self.running:
            try:
                ready = select.select([self.sock], [], [], 0.1)
                if ready[0]:
                    data = self.sock.recv(1024)
                    if not data:
                        print("\n[Connection closed by server]")
                        self.stop()
                        break

                    for byte in data:
                        self.process_rx_byte(byte)

            except Exception as e:
                if self.running:
                    print(f"\n[ERROR receiving]: {e}")
                break

    def send_message(self, message):
        """Encode and send message using protocol"""
        try:
            payload = message.encode('utf-8') if isinstance(message, str) else message
            if self.user_id != USER_ID_BOARD:
                payload = aead_encrypt(bytes(KEY), payload)
                if payload is None:
                    print("[ERROR] Encryption failed")
                    return
            frame = encode_frame(self.user_id, payload)
            self.sock.sendall(frame)
            print(f"[TX] {message}")
        except Exception as e:
            print(f"[ERROR sending]: {e}")
            self.stop()

    def run(self):
        """Main client loop"""
        if not self.connect():
            return

        recv_thread = threading.Thread(target=self.receive_thread, daemon=True)
        recv_thread.start()

        try:
            while self.running:
                try:
                    message = input()
                    if not self.running:
                        break

                    if message.strip():
                        self.send_message(message)

                except EOFError:
                    break

        except KeyboardInterrupt:
            print("\n[Disconnecting...]")
        finally:
            self.stop()
            if self.sock:
                self.sock.close()
            print("[Disconnected]")

def main():
    parser = argparse.ArgumentParser(description="Protocol Chat Client")
    parser.add_argument("host", help="ESP8266 IP address")
    parser.add_argument("port", nargs="?", type=int, default=23, help="Telnet port (default 23)")
    parser.add_argument("--user-id", dest="user_id", type=lambda x: int(x, 0), default=USER_ID_CLIENT,
                        help="User ID in hex or decimal (e.g. 0x02). Use 0x00 to act as 'board' (plaintext in, board encrypts).")
    parser.add_argument("--key", dest="key", default=None,
                        help="Optional 32-byte key (64 hex chars or ASCII, truncated/padded)")
    args = parser.parse_args()

    host = args.host
    port = args.port
    user_id = args.user_id & 0xFF

    # Optional key override
    if args.key is not None:
        try:
            if len(args.key) == 64 and all(c in '0123456789abcdefABCDEF' for c in args.key):
                raw = bytes.fromhex(args.key)
            else:
                raw = args.key.encode('utf-8')
            for i in range(32):
                KEY[i] = raw[i] if i < len(raw) else 0
        except Exception as e:
            print(f"[WARN] invalid --key ignored: {e}")

    print("=== Protocol Chat Client ===")
    print(f"User ID: 0x{user_id:02X}")
    print(f"Connecting to {host}:{port}...\n")

    client = ChatClient(host, port, user_id=user_id)
    client.run()

if __name__ == "__main__":
    main()
