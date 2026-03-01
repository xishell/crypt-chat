# Protocol

## Frame Format

```
Wire:       [0xAA] [0xAA] [LEN] [STUFFED_DATA]

Unstuffed:  [USER_ID] [MESSAGE...] [CRC_HI] [CRC_LO]
```

- `LEN` — length of stuffed data (1-256)
- `CRC16-CCITT` — polynomial `0x1021`, initial `0xFFFF`, computed over `USER_ID + MESSAGE`

## Byte Stuffing

Escape byte `0xAB` is used to avoid sync byte collisions in the payload:

| Raw byte | Encoded as  |
| -------- | ----------- |
| `0xAA`   | `0xAB 0x00` |
| `0xAB`   | `0xAB 0x01` |

## Encryption

Message payloads are encrypted with ChaCha20-Poly1305 AEAD (RFC 8439):

```
MESSAGE = [NONCE (12 bytes)] [CIPHERTEXT (n bytes)] [TAG (16 bytes)]
```

- Nonce is generated per message (counter-based on board, random on client)
- Tag authenticates the ciphertext; messages with invalid tags are dropped

## RX Flow

1. Detect two consecutive `0xAA` sync bytes
2. Read length byte
3. Collect `LEN` bytes of stuffed data
4. Unstuff, verify CRC, deliver message
5. Timeout: drop partial frame after ~1 second of inactivity

## TX Flow

1. Prepend `USER_ID` to message
2. Compute CRC16 over `USER_ID + MESSAGE`
3. Append CRC (big-endian)
4. Byte-stuff the result
5. Send `[0xAA] [0xAA] [LEN] [STUFFED_DATA]`

## User IDs

| ID     | Role   |
| ------ | ------ |
| `0x00` | Board  |
| `0x02` | Client |

The client filters frames containing its own user ID to suppress echo.
