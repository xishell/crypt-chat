#include "utils.h"
#include "dtekv-lib.h"

/* Memory addresses */
#define TIMER_BASE  0x04000020
#define SW_BASE     0x04000010

/* ===== Printf Implementation ===== */

/* Simple variable argument list handling */
typedef char *va_list;
#define va_start(ap, last) ((ap) = (char *)&(last) + sizeof(last))
#define va_arg(ap, type) (*(type *)((ap) += sizeof(type), (ap) - sizeof(type)))
#define va_end(ap) ((void)0)

void printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                print_dec(val);
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                print_udec(val);
                break;
            }
            case 'x':
            case 'X': {
                unsigned int val = va_arg(args, unsigned int);
                print_hex(val, 8);
                break;
            }
            case 'p': {
                void *ptr = va_arg(args, void *);
                print("0x");
                print_hex((unsigned int)ptr, 8);
                break;
            }
            case 's': {
                char *str = va_arg(args, char *);
                print(str ? str : "(null)");
                break;
            }
            case 'c': {
                char ch = (char)va_arg(args, int);
                printc(ch);
                break;
            }
            case '%':
                printc('%');
                break;
            default:
                printc('%');
                printc(*format);
                break;
            }
        } else {
            printc(*format);
        }
        format++;
    }

    va_end(args);
}

/* ===== Memory Dump Utilities ===== */

void mem_dump(unsigned int address, unsigned int length) {
    volatile unsigned char *ptr = (volatile unsigned char *)address;

    printf("Memory dump at 0x%x (%u bytes):\n", address, length);

    for (unsigned int i = 0; i < length; i++) {
        if (i % 16 == 0) {
            printf("\n0x%x: ", address + i);
        }
        printf("%x ", ptr[i]);
    }
    printf("\n");
}

void mem_dump_words(unsigned int address, unsigned int num_words) {
    volatile unsigned int *ptr = (volatile unsigned int *)address;

    printf("Memory dump at 0x%x (%u words):\n", address, num_words);

    for (unsigned int i = 0; i < num_words; i++) {
        if (i % 4 == 0) {
            printf("\n0x%x: ", address + (i * 4));
        }
        printf("0x%x ", ptr[i]);
    }
    printf("\n");
}

void mem_write(unsigned int address, unsigned int value) {
    volatile unsigned int *ptr = (volatile unsigned int *)address;
    *ptr = value;
    printf("Wrote 0x%x to address 0x%x\n", value, address);
}

unsigned int mem_read(unsigned int address) {
    volatile unsigned int *ptr = (volatile unsigned int *)address;
    unsigned int value = *ptr;
    printf("Read 0x%x from address 0x%x\n", value, address);
    return value;
}

/* ===== Register Inspection ===== */

void reg_dump_csr(void) {
    unsigned int mstatus, mie, mip, mcause, mepc, mcycle, minstret;

    /* Read CSRs using inline assembly */
    asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    asm volatile("csrr %0, mie" : "=r"(mie));
    asm volatile("csrr %0, mip" : "=r"(mip));
    asm volatile("csrr %0, mcause" : "=r"(mcause));
    asm volatile("csrr %0, mepc" : "=r"(mepc));
    asm volatile("csrr %0, mcycle" : "=r"(mcycle));
    asm volatile("csrr %0, minstret" : "=r"(minstret));

    printf("\n=== RISC-V CSR Dump ===\n");
    printf("mstatus:  0x%x\n", mstatus);
    printf("mie:      0x%x\n", mie);
    printf("mip:      0x%x\n", mip);
    printf("mcause:   0x%x\n", mcause);
    printf("mepc:     0x%x\n", mepc);
    printf("mcycle:   %u\n", mcycle);
    printf("minstret: %u\n", minstret);
}

void reg_dump_timer(void) {
    volatile unsigned short *status = (volatile unsigned short *)(TIMER_BASE + 0x00);
    volatile unsigned short *control = (volatile unsigned short *)(TIMER_BASE + 0x04);
    volatile unsigned short *periodl = (volatile unsigned short *)(TIMER_BASE + 0x08);
    volatile unsigned short *periodh = (volatile unsigned short *)(TIMER_BASE + 0x0C);

    printf("\n=== Timer Registers ===\n");
    printf("STATUS:  0x%x\n", *status);
    printf("CONTROL: 0x%x\n", *control);
    printf("PERIODL: 0x%x\n", *periodl);
    printf("PERIODH: 0x%x\n", *periodh);

    unsigned int period = (*periodh << 16) | *periodl;
    printf("Period:  %u cycles\n", period);
}

void reg_dump_switches(void) {
    volatile unsigned int *sw_data = (volatile unsigned int *)(SW_BASE + 0x00);
    volatile unsigned int *sw_dir = (volatile unsigned int *)(SW_BASE + 0x04);
    volatile unsigned int *sw_mask = (volatile unsigned int *)(SW_BASE + 0x08);
    volatile unsigned int *sw_edge = (volatile unsigned int *)(SW_BASE + 0x0C);

    printf("\n=== Switch Registers ===\n");
    printf("DATA:         0x%x\n", *sw_data);
    printf("DIRECTION:    0x%x\n", *sw_dir);
    printf("IRQ_MASK:     0x%x\n", *sw_mask);
    printf("EDGE_CAPTURE: 0x%x\n", *sw_edge);
}

void reg_dump_all(void) {
    reg_dump_csr();
    reg_dump_timer();
    reg_dump_switches();
    printf("\n");
}

/* ===== String Formatting Helpers ===== */

void itoa(int value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    int tmp_value;

    if (base < 2 || base > 36) {
        *str = '\0';
        return;
    }

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
    } while (value);

    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';

    /* Reverse string */
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

void utoa(unsigned int value, char *str, int base) {
    char *ptr = str;
    char *ptr1 = str;
    char tmp_char;
    unsigned int tmp_value;

    if (base < 2 || base > 36) {
        *str = '\0';
        return;
    }

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
    } while (value);

    *ptr-- = '\0';

    /* Reverse string */
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

/* ===== Timing Utilities ===== */

unsigned int get_cycles(void) {
    unsigned int cycles;
    asm volatile("csrr %0, mcycle" : "=r"(cycles));
    return cycles;
}

unsigned int get_time_ms(void) {
    /* Assuming 30 MHz clock */
    return get_cycles() / 30000;
}

void sleep_ms(unsigned int ms) {
    unsigned int target = get_time_ms() + ms;
    while (get_time_ms() < target);
}

/* ===== String Utilities ===== */

/* String length */
int strlen(const char *s) {
    int len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

/* String comparison */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* String copy */
void strcpy(char *dest, const char *src) {
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}

/* String concatenation */
void strcat(char *dest, const char *src) {
    while (*dest != '\0') {
        dest++;
    }
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}
