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
import socket
import select
import threading
from enum import Enum

# Protocol constants
CHAT_SYNC_BYTE = 0xAA
CHAT_ESCAPE_BYTE = 0xAB
CHAT_MAX_MESSAGE = 127
CHAT_MAX_FRAME = 256

# User IDs
USER_ID_BOARD = 0x01
USER_ID_CLIENT = 0x02

class RxState(Enum):
    PLAINTEXT = 0
    SYNC1 = 1
    SYNC2 = 2
    DATA = 3

class ChatProtocol:
    """Implements the robust chat protocol with CRC16 and byte stuffing"""

    @staticmethod
    def crc16_ccitt(data):
        """Calculate CRC16-CCITT (polynomial 0x1021, initial 0xFFFF)"""
        crc = 0xFFFF
        for byte in data:
            crc ^= (byte << 8)
            for _ in range(8):
                if crc & 0x8000:
                    crc = (crc << 1) ^ 0x1021
                else:
                    crc = crc << 1
                crc &= 0xFFFF  # Keep 16-bit
        return crc

    @staticmethod
    def byte_stuff(data):
        """Apply byte stuffing: 0xAA -> 0xAB 0x00, 0xAB -> 0xAB 0x01"""
        result = bytearray()
        for byte in data:
            if byte == CHAT_SYNC_BYTE:
                result.append(CHAT_ESCAPE_BYTE)
                result.append(0x00)
            elif byte == CHAT_ESCAPE_BYTE:
                result.append(CHAT_ESCAPE_BYTE)
                result.append(0x01)
            else:
                result.append(byte)
        return bytes(result)

    @staticmethod
    def byte_unstuff(data):
        """Remove byte stuffing"""
        result = bytearray()
        i = 0
        while i < len(data):
            if data[i] == CHAT_ESCAPE_BYTE:
                i += 1
                if i >= len(data):
                    return None  # Invalid
                if data[i] == 0x00:
                    result.append(CHAT_SYNC_BYTE)
                elif data[i] == 0x01:
                    result.append(CHAT_ESCAPE_BYTE)
                else:
                    return None  # Invalid
                i += 1
            else:
                result.append(data[i])
                i += 1
        return bytes(result)

    @staticmethod
    def encode_frame(user_id, message):
        """Encode message into protocol frame: [0xAA 0xAA] [LEN] [STUFFED_DATA]
        where STUFFED_DATA contains: [USER_ID] [MESSAGE...] [CRC_HI] [CRC_LO]"""
        # Convert message to bytes if string
        if isinstance(message, str):
            message = message.encode('utf-8')

        # Prepare payload with user ID: [USER_ID] [MESSAGE]
        payload_with_id = bytes([user_id]) + message

        # Calculate CRC over [USER_ID] [MESSAGE]
        crc = ChatProtocol.crc16_ccitt(payload_with_id)

        # Prepare final payload: [USER_ID] [MESSAGE] + CRC (big-endian)
        payload = payload_with_id + bytes([(crc >> 8) & 0xFF, crc & 0xFF])

        # Apply byte stuffing
        stuffed = ChatProtocol.byte_stuff(payload)

        if len(stuffed) > CHAT_MAX_FRAME:
            raise ValueError("Frame too large after stuffing")

        # Build frame: [SYNC SYNC] [LEN] [STUFFED_DATA]
        frame = bytes([CHAT_SYNC_BYTE, CHAT_SYNC_BYTE, len(stuffed)]) + stuffed
        return frame

    @staticmethod
    def decode_frame(stuffed_data):
        """Decode stuffed frame data, verify CRC, return (user_id, message) or (None, None)"""
        # Unstuff
        unstuffed = ChatProtocol.byte_unstuff(stuffed_data)
        if unstuffed is None or len(unstuffed) < 4:  # USER_ID + 1 byte msg + 2 byte CRC
            return (None, None)

        # Extract user_id
        user_id = unstuffed[0]

        # Extract message and CRC (CRC is over [USER_ID] [MESSAGE])
        msg_with_id_len = len(unstuffed) - 2
        msg_len = msg_with_id_len - 1  # Exclude USER_ID
        message = unstuffed[1:msg_with_id_len]  # Message starts at index 1
        received_crc = (unstuffed[msg_with_id_len] << 8) | unstuffed[msg_with_id_len + 1]

        # Verify CRC over [USER_ID] [MESSAGE]
        calculated_crc = ChatProtocol.crc16_ccitt(unstuffed[:msg_with_id_len])

        if received_crc != calculated_crc:
            print(f"[DEBUG] CRC mismatch: RX={received_crc:04X} Calc={calculated_crc:04X}")
            return (None, None)

        return (user_id, message)

class ChatClient:
    def __init__(self, host, port=23, user_id=USER_ID_CLIENT):
        self.host = host
        self.port = port
        self.sock = None
        self.running = False
        self.user_id = user_id

        # Protocol state machine
        self.rx_state = RxState.PLAINTEXT
        self.rx_frame_buffer = bytearray()
        self.rx_expected_length = 0

    def connect(self):
        """Connect to ESP8266 telnet server"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            print(f"[Connected to {self.host}:{self.port}]")
            print("[Type messages to send (will be encrypted)]")
            print("[Messages from board will be decrypted and displayed]")
            print("[Press Ctrl+C to quit]\n")
            self.running = True
            return True
        except Exception as e:
            print(f"[ERROR] Could not connect: {e}")
            return False

    def process_rx_byte(self, byte):
        """Process received byte with state machine"""
        if self.rx_state == RxState.PLAINTEXT:
            if byte == CHAT_SYNC_BYTE:
                self.rx_state = RxState.SYNC1
            else:
                # Plaintext byte - just print it
                try:
                    print(chr(byte), end='', flush=True)
                except:
                    pass

        elif self.rx_state == RxState.SYNC1:
            if byte == CHAT_SYNC_BYTE:
                self.rx_state = RxState.SYNC2
            else:
                # False alarm
                self.rx_state = RxState.PLAINTEXT
                try:
                    print(chr(byte), end='', flush=True)
                except:
                    pass

        elif self.rx_state == RxState.SYNC2:
            # This should be length byte
            if byte > 0 and byte <= CHAT_MAX_FRAME:
                self.rx_expected_length = byte
                self.rx_frame_buffer = bytearray()
                self.rx_state = RxState.DATA
            else:
                # Invalid length
                self.rx_state = RxState.PLAINTEXT

        elif self.rx_state == RxState.DATA:
            # Collecting frame data
            self.rx_frame_buffer.append(byte)

            if len(self.rx_frame_buffer) >= self.rx_expected_length:
                # Complete frame received - process it
                user_id, message = ChatProtocol.decode_frame(bytes(self.rx_frame_buffer))

                if message is not None:
                    # Filter out our own messages (echo from board)
                    if user_id == self.user_id:
                        # Ignore - this is our own message echoed back
                        pass
                    else:
                        # TODO: Add decryption here
                        try:
                            msg_str = message.decode('utf-8')
                            print(f"\n[RX from user 0x{user_id:02X}] {msg_str}")
                        except:
                            print(f"\n[RX from user 0x{user_id:02X}] <binary: {len(message)} bytes>")

                # Reset to plaintext mode
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
                        self.running = False
                        break

                    # Process each byte through state machine
                    for byte in data:
                        self.process_rx_byte(byte)

            except Exception as e:
                if self.running:
                    print(f"\n[ERROR receiving]: {e}")
                break

    def send_message(self, message):
        """Encode and send message using protocol"""
        try:
            # TODO: Add encryption here before encoding
            frame = ChatProtocol.encode_frame(self.user_id, message)
            self.sock.sendall(frame)
            print(f"[TX] {message}")
        except Exception as e:
            print(f"[ERROR sending]: {e}")
            self.running = False

    def run(self):
        """Main client loop"""
        if not self.connect():
            return

        # Start receive thread
        recv_thread = threading.Thread(target=self.receive_thread, daemon=True)
        recv_thread.start()

        # Main input loop
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
            self.running = False
            if self.sock:
                self.sock.close()
            print("[Disconnected]")

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 chat_client.py <ESP8266_IP> [port]")
        print("Example: python3 chat_client.py 192.168.4.1 23")
        sys.exit(1)

    host = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 23

    print("=== Protocol Chat Client (Remote User) ===")
    print(f"Connecting to {host}:{port}...\n")

    client = ChatClient(host, port)
    client.run()

if __name__ == "__main__":
    main()
