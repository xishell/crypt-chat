# Protocol

## Frame

```
[0xAA][0xAA] [LEN] [STUFFED_DATA]

After unstuffing:
  [USER_ID] [MESSAGE...] [CRC_HI] [CRC_LO]

CRC16-CCITT over: [USER_ID] + [MESSAGE]
```

## Stuffing

- Escape with 0xAB:
  - 0xAA → 0xAB 0x00
  - 0xAB → 0xAB 0x01

## RX flow

- SYNC1 → SYNC2 → LENGTH → DATA → CRC check.
- Timeout: drop partial frame (~1s).

## TX flow

1. [USER_ID][MESSAGE]
2. CRC16
3. Stuff
4. Send header + data

## Notes

- Client filters its own `USER_ID` to avoid echo.
  │ │
  ├─ CRC OK: Deliver msg │
  │ │
  └─ CRC fail ────────────┘

```

## Next Steps

1. **Add real encryption** - Replace `pseudo_encrypt/decrypt` with real implementation

```
