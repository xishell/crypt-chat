# DTEK-V framework

RISC-V embedded system framework for the DTEK-V board.
This repository was created to help the completion of various labs and projects
found within the KTH IS1200/1500 course.

## Project Structure

```
main/
├── src/              Source files
│   ├── main.c        Application entry point
│   ├── boot.S        Boot code and interrupt handlers
│   ├── dtekv-lib.c   Core system library
│   ├── devices.c     Hardware device drivers
│   └── utils.c       Utility functions and debug tools
├── include/          Header files
│   ├── dtekv-lib.h   Core library API
│   ├── devices.h     Device driver API
│   └── utils.h       Utility functions API
├── docs/             Documentation
│   └── docs.md       Complete API and hardware reference
├── build/            Build artifacts (generated)
├── Makefile          Build system
├── dtekv-script.lds  Linker script
├── run_lab.sh        Upload and run script
└── softfloat.a       Floating point library
```

## Quick Start

### Build

```bash
make          # Build release version
make debug    # Build debug version with symbols
make clean    # Clean build artifacts
```

### Upload and Run

```bash
make run      # Build and upload to DTEK-V board
```

### Help

```bash
make help     # Show all available targets
```

## Features

### Core Library (dtekv-lib)

- **Output**: `print()`, `printc()`, `print_dec()`, `print_hex()`, `print_bin()`
- **Input**: `readc()`, `read_available()`
- **String**: `strlen()`, `strcmp()`, `strcpy()`, `strcat()`
- **Timing**: `delay()`
- **Interrupts**: Automatic handling with user callbacks

### Device Drivers (devices)

- **LEDs**: `led_init()`, `led_set()`, `led_on()`, `led_off()`, `led_toggle()`
- **7-Segment Displays**: `display_set()`, `display_number()`, `display_time()`
- **Buttons**: `button_is_pressed()`, `button_wait_press()`
- **Switches**: `switch_read()`, `switch_get()`
- **GPIO**: `gpio_set_direction()`, `gpio_write()`, `gpio_read()`

### Utilities (utils)

- **Printf**: `printf()` with format specifiers (%d, %u, %x, %s, %c, %p)
- **Memory Dump**: `mem_dump()`, `mem_dump_words()`, `mem_read()`, `mem_write()`
- **Register Inspection**: `reg_dump_csr()`, `reg_dump_timer()`, `reg_dump_all()`
- **Timing**: `get_cycles()`, `get_time_ms()`, `sleep_ms()`
- **Debug**: `ASSERT()` macro

## Example Usage

```c
#include "dtekv-lib.h"
#include "devices.h"
#include "utils.h"

int main(void) {
    // Initialize devices
    led_init();
    display_init();

    // Use printf
    printf("Hello, DTEK-V! Time: %u ms\n", get_time_ms());

    // Control LEDs
    led_set(0x3FF);  // All on
    sleep_ms(1000);
    led_set(0x000);  // All off

    // Read switches
    unsigned int sw = switch_read();
    printf("Switch state: 0x%x\n", sw);

    // Display number
    display_number(0x123456);

    // Dump registers for debugging
    reg_dump_all();

    return 0;
}
```

## Interrupt Handling

### Timer Interrupt Example

```c
volatile unsigned int tick_count = 0;

void timer_isr(void) {
    tick_count++;
}

int main(void) {
    setup_timer();
    enable_interrupt();

    while (1) {
        printf("Ticks: %u\n", tick_count);
        sleep_ms(1000);
    }
}
```

### Switch Interrupt Example

```c
void switch_isr(unsigned int switch_state) {
    printf("Switch changed: 0x%x\n", switch_state);
    led_set(switch_state);
}

int main(void) {
    led_init();
    SW_IRQ_MASK = 0x3FF;         // Enable all switches
    SW_EDGE_CAPTURE = 0x3FF;     // Clear pending
    enable_interrupt();

    while (1) {
        // Main loop
    }
}
```

## Debug vs Release Builds

**Release** (default):

- Optimized with `-O3`
- Smaller binary size
- Faster execution

**Debug**:

- No optimization (`-O0`)
- Debug symbols (`-g`)
- `DEBUG` macro defined
- `ASSERT()` macros enabled

## License

See `docs/COPYING` for license information.
