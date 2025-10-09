# Chat Client

Single client: `chat_client_protocol.py`.

Usage:
```bash
python3 chat_client_protocol.py 192.168.4.1 [port]
```

Protocol:
```
[0xAA 0xAA] [LEN] [STUFFED_DATA]
Unstuffed: [USER_ID][MESSAGE][CRC16]
CRC over USER_ID+MESSAGE
```

User IDs:
- 0x01 = Board (BTN1 messages)
- 0x02 = Client (default)

Notes:
- Client ignores frames with its own USER_ID (echo).
- ESP8266 may allow only one telnet connection.
