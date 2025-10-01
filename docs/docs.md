# DTEK-V Library Documentation

## Overview

This document describes the DTEK-V library functions, memory-mapped I/O regions, and interrupt handling for the RISC-V based DTEK-V board.

---

## Library Functions

### Output Functions

#### `void printc(char c)`

Prints a single character to the JTAG UART.

- **Parameters**: `c` - character to print
- **Notes**: Blocks until UART buffer has space available

#### `void print(char *s)`

Prints a null-terminated string to the JTAG UART.

- **Parameters**: `s` - pointer to string (null-terminated)
- **Notes**: Returns immediately if `s` is NULL

#### `void print_dec(int x)`

Prints a signed decimal integer.

- **Parameters**: `x` - signed integer to print
- **Example**: `print_dec(-42)` outputs `-42`

#### `void print_udec(unsigned int x)`

Prints an unsigned decimal integer.

- **Parameters**: `x` - unsigned integer to print
- **Example**: `print_udec(255)` outputs `255`

#### `void print_hex32(unsigned int x)`

Prints a 32-bit value in hexadecimal (8 digits).

- **Parameters**: `x` - value to print
- **Example**: `print_hex32(0x12AB)` outputs `0x000012AB`

#### `void print_hex(unsigned int x, int digits)`

Prints a hexadecimal value with specified number of digits.

- **Parameters**:
  - `x` - value to print
  - `digits` - number of hex digits (1-8)
- **Example**: `print_hex(0xFF, 2)` outputs `0xFF`

#### `void print_bin(unsigned int x, int bits)`

Prints a value in binary format.

- **Parameters**:
  - `x` - value to print
  - `bits` - number of bits to display
- **Example**: `print_bin(5, 8)` outputs `0b00000101`

---

### Input Functions

#### `char readc(void)`

Reads a single character from JTAG UART (non-blocking).

- **Returns**: Character if available, `0` if no data
- **Notes**: Non-blocking - check with `read_available()` first

#### `int read_available(void)`

Checks if input data is available.

- **Returns**: Non-zero if data available, `0` otherwise
- **Example**:

```c
if (read_available()) {
    char c = readc();
    printc(c);
}
```

---

### String Utilities

#### `int strlen(const char *s)`

Calculates string length.

- **Parameters**: `s` - pointer to string
- **Returns**: Length of string (excluding null terminator)

#### `int strcmp(const char *s1, const char *s2)`

Compares two strings.

- **Parameters**: `s1`, `s2` - strings to compare
- **Returns**:
  - `0` if strings are equal
  - `< 0` if s1 < s2
  - `> 0` if s1 > s2

#### `void strcpy(char *dest, const char *src)`

Copies a string.

- **Parameters**:
  - `dest` - destination buffer
  - `src` - source string
- **Notes**: Destination must have enough space

#### `void strcat(char *dest, const char *src)`

Concatenates strings.

- **Parameters**:
  - `dest` - destination string (will be modified)
  - `src` - string to append

---

### Utility Functions

#### `void delay(unsigned int cycles)`

Simple delay loop.

- **Parameters**: `cycles` - number of loop iterations
- **Notes**: Actual time depends on CPU clock; not cycle-accurate

#### `void enable_interrupt(void)`

Enables all DTEK-V interrupts (defined in boot.S).

- **Notes**: Enables IRQ 16 (Timer), 17 (Switches), 18 (Button) and sets global interrupt enable

---

### Exception and Interrupt Handlers

#### `void handle_exception(...)`

Internal exception handler (called by boot.S).

- **Handles**:
  - Instruction misalignment (mcause=0)
  - Illegal instruction (mcause=2)
  - Environment calls/ecall (mcause=11)
- **Notes**: System calls 4 and 11 are used for printing

#### `void handle_interrupt(unsigned cause)`

Interrupt handler (called by boot.S) with skeleton handlers for all devices.

- **Parameters**: `cause` - interrupt IRQ number (16, 17, or 18)
- **Handles**:
  - IRQ 16: Timer - clears timeout flag and prints message
  - IRQ 17: Switches - reads and displays switch state
  - IRQ 18: Button - reads and displays button state
- **Notes**: Each case includes a TODO comment for adding custom logic

---

## Memory Map

### System Memory

| Address Range             | Size  | Description                  |
| ------------------------- | ----- | ---------------------------- |
| `0x00000000 - 0x01FFFFFF` | 32 MB | Main RAM (code, data, stack) |

### Memory-Mapped I/O

#### Complete I/O Device Map

| I/O Device   | Base Address            | Type         | IRQ # |
| ------------ | ----------------------- | ------------ | ----- |
| LEDs         | 0x04000000              | Output       | -     |
| Switches     | 0x04000010              | Input        | 17    |
| Timer        | 0x04000020              | Device       | 16    |
| UART (JTAG)  | 0x04000040              | Device       | -     |
| Hex Displays | 0x04000050 + (n × 0x10) | Output       | -     |
| Mutex        | 0x040000C0              | Device       | -     |
| Button       | 0x040000D0              | Input        | 18    |
| GPIO (2×20)  | 0x040000E0, 0x040000F0  | Input/Output | -     |
| VGA DMA      | 0x04000100              | Device       | -     |
| VGA Buffer   | 0x08000000              | Device       | -     |

#### Timer/Counter (`0x04000020 - 0x0400002F`)

| Offset | Address      | Register    | Description                     |
| ------ | ------------ | ----------- | ------------------------------- |
| +0x00  | `0x04000020` | **STATUS**  | Status register                 |
| +0x04  | `0x04000024` | **CONTROL** | Control register                |
| +0x08  | `0x04000028` | **PERIODL** | Timeout period (low 16 bits)    |
| +0x0C  | `0x0400002C` | **PERIODH** | Timeout period (high 16 bits)   |
| +0x10  | `0x04000030` | **SNAPL**   | Counter snapshot (low 16 bits)  |
| +0x14  | `0x04000034` | **SNAPH**   | Counter snapshot (high 16 bits) |

**STATUS Register (Offset +0x00)**

- Bit 0 (TO): Timeout flag - set when counter reaches 0, write 1 to clear

**CONTROL Register (Offset +0x04)**

- Bit 0 (ITO): Interrupt enable on timeout
- Bit 1 (CONT): Continuous mode (1=reload automatically, 0=one-shot)
- Bit 2 (START): Write 1 to start/restart counter
- Bit 3 (STOP): Write 1 to stop counter

**Recommended Memory-Mapped I/O Pattern**

```c
/* Define base address and register macros */
#define TIMER_BASE  0x04000020
#define T_STATUS  (*(volatile unsigned short *)(TIMER_BASE + 0x00))
#define T_CONTROL (*(volatile unsigned short *)(TIMER_BASE + 0x04))
#define T_PERIODL (*(volatile unsigned short *)(TIMER_BASE + 0x08))
#define T_PERIODH (*(volatile unsigned short *)(TIMER_BASE + 0x0C))

/* Define bit masks using enum */
enum {
    TO = 1 << 0,      /* Timeout flag */
    ITO = 1 << 0,     /* Interrupt on timeout */
    CONT = 1 << 1,    /* Continuous mode */
    START = 1 << 2,   /* Start timer */
    STOP = 1 << 3     /* Stop timer */
};

/* Usage example */
T_STATUS = TO;                      /* Clear timeout flag */
T_PERIODL = 3000000 & 0xFFFF;      /* Set period (low) */
T_PERIODH = (3000000 >> 16);       /* Set period (high) */
T_CONTROL = ITO | CONT | START;    /* Enable interrupt, continuous, start */
```

#### Other Common I/O Devices

**LEDs (0x04000000)**

```c
#define LED_ADDRESS 0x04000000

void set_leds(int led_mask) {
    volatile unsigned int *leds = (volatile unsigned int *)LED_ADDRESS;
    *leds = led_mask & 0x3FF;  /* 10 LEDs */
}
```

**Switches (0x04000010)**

```c
#define SW_BASE 0x04000010

int get_switches(void) {
    volatile unsigned int *switches = (volatile unsigned int *)SW_BASE;
    return *switches & 0x3FF;  /* 10 switches */
}

/* IMPORTANT: Switches share address with External Interrupt Controller (EIC) */
/* To clear switch interrupt (IRQ 17), write bit 17 to the pending register */
#define EIC_PENDING (*(volatile unsigned int*)SW_BASE)
#define EXTIRQ_SW_BIT (1 << 17)

/* In your interrupt handler: */
void handle_switch_interrupt(void) {
    unsigned int switch_state = *(volatile unsigned int *)SW_BASE;
    EIC_PENDING = EXTIRQ_SW_BIT;  /* Clear interrupt pending bit */
    /* ... handle switch change ... */
}
```

**Buttons (0x040000D0)**

```c
#define BTN_BASE 0x040000D0

int get_button(void) {
    volatile unsigned int *button = (volatile unsigned int *)BTN_BASE;
    return *button & 0x1;  /* 1 button */
}

/* Button interrupts also need to clear pending bit */
#define EXTIRQ_BTN_BIT (1 << 18)

/* In your interrupt handler: */
void handle_button_interrupt(void) {
    unsigned int button_state = *(volatile unsigned int *)BTN_BASE;
    EIC_PENDING = EXTIRQ_BTN_BIT;  /* Clear interrupt pending bit */
    /* ... handle button press ... */
}
```

**Hex Displays (0x04000050 + n×0x10)**

```c
#define DISP_BASE   0x04000050
#define NUM_DISPLAYS 6
#define DISP_STRIDE 0x10

void set_display(int display_number, unsigned char value) {
    if (display_number < 0 || display_number >= NUM_DISPLAYS)
        return;
    volatile unsigned int *addr =
        (volatile unsigned int *)(DISP_BASE + (display_number * DISP_STRIDE));
    *addr = value & 0xFF;
}
```

#### JTAG UART (`0x04000040 - 0x04000047`)

| Address      | Register    | Description                |
| ------------ | ----------- | -------------------------- |
| `0x04000040` | **DATA**    | Data register (read/write) |
| `0x04000044` | **CONTROL** | Control/status register    |

**DATA Register (0x04000040)**

- Bits 7:0 - Data byte to write or read
- Bit 15 (RVALID) - Read valid (1=data available when reading)

**CONTROL Register (0x04000044)**

- Bits 31:16 (WSPACE) - Write space available in FIFO

---

## RISC-V Interrupts and Exceptions

### Exception Causes (mcause register)

| mcause | Exception Type                 | Description                     |
| ------ | ------------------------------ | ------------------------------- |
| 0      | Instruction address misaligned | PC not properly aligned         |
| 1      | Instruction access fault       | Invalid instruction fetch       |
| 2      | Illegal instruction            | Invalid/unsupported instruction |
| 3      | Breakpoint                     | EBREAK instruction              |
| 4      | Load address misaligned        | Unaligned memory read           |
| 5      | Load access fault              | Invalid memory read             |
| 6      | Store address misaligned       | Unaligned memory write          |
| 7      | Store access fault             | Invalid memory write            |
| 11     | Environment call (ecall)       | System call from M-mode         |

### Interrupt Causes

When bit 31 of mcause is set (0x80000000), it indicates an interrupt.
The lower bits indicate the interrupt source:

| IRQ # | Source   | Device Address | Description                           | Clear Method                 |
| ----- | -------- | -------------- | ------------------------------------- | ---------------------------- |
| 16    | Timer    | 0x04000020     | Fires when timer counter reaches 0    | Write 1 to T_STATUS TO bit   |
| 17    | Switches | 0x04000010     | Fires when switch state changes       | Write (1<<17) to EIC_PENDING |
| 18    | Button   | 0x040000D0     | Fires when button is pressed/released | Write (1<<18) to EIC_PENDING |

**CRITICAL:** Switch and Button interrupts use an External Interrupt Controller (EIC) that shares the switch base address (0x04000010). You **must** clear the pending bit by writing `(1 << IRQ_NUMBER)` to this address, or the interrupt will keep firing continuously.

### System Calls (ecall)

The boot code supports basic system calls via `ecall`:

| Syscall # (a7) | Function        | Parameters             |
| -------------- | --------------- | ---------------------- |
| 4              | Print string    | a0 = pointer to string |
| 11             | Print character | a0 = character value   |

**Example:**

```assembly
la a0, my_string
li a7, 4
ecall  # Prints string
```

### Enabling Interrupts

```c
enable_interrupt();  // Enable all machine-mode interrupts
```

This function (in boot.S) sets:

- MIE CSR bit 16 (timer interrupt)
- MIE CSR bit 17 (switch interrupt)
- MIE CSR bit 18 (button interrupt)
- MSTATUS MIE bit (global interrupt enable)

### Handling Interrupts

The `handle_interrupt()` function in dtekv-lib.c provides skeleton handlers for all three interrupt sources. Each handler:

- Prints a debug message
- Reads the device state
- Clears the interrupt flag (for timer)
- Includes a TODO comment for adding custom logic

**Example: Customizing Timer Interrupt**

```c
// In handle_interrupt() (dtekv-lib.c), replace the TODO with:
case IRQ_TIMER:
    *TIMER_STATUS = 0;  // Clear timeout flag
    // Your custom code here
    tick_count++;
    if (tick_count >= 100) {
        print("1 second elapsed\n");
        tick_count = 0;
    }
    break;
```

---

## Boot Sequence

1. **Entry Point**: `_start` in boot.S
2. **Stack Setup**: Stack pointer set to `_stack_end`
3. **Global Pointer**: Set for optimized data access
4. **Welcome Message**: Printed via ecall
5. **Jump to main()**: User code begins
6. **Infinite Loop**: After main returns

### Interrupt Service Routine (ISR)

Located at `_isr_handler` in boot.S:

1. Save all registers to stack
2. Read `mcause` CSR
3. If MSB set → call `handle_interrupt()`
4. If exception → call `handle_exception()`
5. For ecall: handle and return
6. For others: increment PC by 4 to skip faulting instruction
7. Restore registers
8. Return via `mret`

---

## Usage Example

```c
#include "dtekv-lib.h"

int main(void) {
    print("Hello, DTEK-V!\n");

    print("Enter a character: ");
    while (!read_available());
    char c = readc();

    print("\nYou entered: ");
    printc(c);
    print("\n");

    print("ASCII value: ");
    print_dec((int)c);
    print(" (");
    print_hex(c, 2);
    print(")\n");

    return 0;
}
```
