#ifndef UTILS_H
#define UTILS_H

/* Small printf, dumps, tiny string helpers, simple timing. */

/* Minimal printf-style output to JTAG UART */
void printf(const char *format, ...);

/* Quick and dirty memory dump helpers */
void mem_dump(unsigned int address, unsigned int length);
void mem_dump_words(unsigned int address, unsigned int num_words);
void mem_write(unsigned int address, unsigned int value);
unsigned int mem_read(unsigned int address);

/* A couple of register dumps handy during bring-up */
void reg_dump_csr(void);
void reg_dump_timer(void);
void reg_dump_switches(void);
void reg_dump_all(void);

/* Tiny string helpers (non-standard, just what's needed) */
int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
void strcpy(char *dest, const char *src);
void strcat(char *dest, const char *src);

/* Integer to string helpers */
int snprintf(char *buf, int size, const char *format, ...);
void itoa(int value, char *str, int base);
void utoa(unsigned int value, char *str, int base);

/* Coarse timing */
unsigned int get_cycles(void);
unsigned int get_time_ms(void);
void sleep_ms(unsigned int ms);

/* Assert for quick checks during development */
#ifdef DEBUG
#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("ASSERT FAILED: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            while(1); \
        } \
    } while(0)
#else
#define ASSERT(cond) ((void)0)
#endif

#endif /* UTILS_H */
