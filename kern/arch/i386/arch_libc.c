#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "sys/types.h"
#include "asm.h"

void *memset(void *dst, int c, size_t n) {
    stosb(dst, c, (uint32_t)n);
    return dst;
}
int strcmp(const char *a, const char *b) {
    while(*a == *b && *a!='\0') {
        a++; b++;
    }
    if(*a == *b) return 0;
    else
        return *a - *b;
}