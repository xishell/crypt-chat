#ifndef DTEKV_LIB_H
#define DTEKV_LIB_H

/*
 * DTEK-V Core Library
 * Low-level I/O, exception handling, and interrupt management
 */

/* ===== JTAG UART I/O Functions ===== */

void printc(char c);                            /* Print single character */
void print(char *s);                            /* Print string */
char readc(void);                               /* Read character (non-blocking) */
int read_available(void);                       /* Check if input available */

/* ===== Number Formatting ===== */

void print_dec(int x);                          /* Print signed decimal */
void print_udec(unsigned int x);                /* Print unsigned decimal */
void print_hex32(unsigned int x);               /* Print 32-bit hex (8 digits) */
void print_hex(unsigned int x, int digits);     /* Print hex with N digits */
void print_bin(unsigned int x, int bits);       /* Print binary with N bits */

/* ===== Exception and Interrupt Handlers ===== */

void handle_exception(unsigned arg0, unsigned arg1, unsigned arg2, unsigned arg3,
                     unsigned arg4, unsigned arg5, unsigned mcause, unsigned syscall_num);
void handle_interrupt(unsigned cause);

/* ===== ISR Function Pointers ===== */
/* Set these to your interrupt handlers */

extern void (*timer_isr)(void);
extern void (*switch_isr)(unsigned int switch_state);
extern void (*button_isr)(unsigned int button_state);

/* ===== Utility Functions ===== */

void delay(unsigned int cycles);                /* Simple delay loop */
void enable_interrupt(void);                    /* Enable global interrupts */

#endif /* DTEKV_LIB_H */




