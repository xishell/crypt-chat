#ifndef UTILS_H
#define UTILS_H

/*
 * DTEK-V Utility Functions
 * Debug and diagnostic tools
 */

/* Printf-style formatted output */
void printf(const char *format, ...);

/* Memory dump utilities */
void mem_dump(unsigned int address, unsigned int length);
void mem_dump_words(unsigned int address, unsigned int num_words);
void mem_write(unsigned int address, unsigned int value);
unsigned int mem_read(unsigned int address);

/* Register inspection */
void reg_dump_csr(void);
void reg_dump_timer(void);
void reg_dump_switches(void);
void reg_dump_all(void);

/* String utilities */
int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
void strcpy(char *dest, const char *src);
void strcat(char *dest, const char *src);

/* String formatting helpers */
int snprintf(char *buf, int size, const char *format, ...);
void itoa(int value, char *str, int base);
void utoa(unsigned int value, char *str, int base);

/* Timing utilities */
unsigned int get_cycles(void);
unsigned int get_time_ms(void);
void sleep_ms(unsigned int ms);

/* Assert macro for debugging */
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
