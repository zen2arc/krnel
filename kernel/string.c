#include "kernel.h"
#include <stdarg.h>

usize strlen(const char* str) {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, usize n) {
    char* d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char* strchr(const char* str, int c) {
    while (*str) {
        if (*str == c) return (char*)str;
        str++;
    }
    return NULL;
}

char* strrchr(const char* str, int c) {
    const char* last = NULL;
    while (*str) {
        if (*str == c) last = str;
        str++;
    }
    return (char*)last;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const u8*)s1 - *(const u8*)s2;
}

int strncmp(const char* s1, const char* s2, usize n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(const u8*)s1 - *(const u8*)s2;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 - 'A' + 'a' : *s1;
        char c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 - 'A' + 'a' : *s2;
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return *s1 - *s2;
}

int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return sign * result;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char* h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
    }
    return NULL;
}

void* memset(void* ptr, int value, usize num) {
    u8* p = (u8*)ptr;
    while (num--) *p++ = (u8)value;
    return ptr;
}

void* memcpy(void* dest, const void* src, usize num) {
    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;
    while (num--) *d++ = *s++;
    return dest;
}

void* memmove(void* dest, const void* src, usize num) {
    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;
    if (d < s) {
        while (num--) *d++ = *s++;
    } else {
        d += num; s += num;
        while (num--) *--d = *--s;
    }
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, usize num) {
    const u8* p1 = (const u8*)ptr1;
    const u8* p2 = (const u8*)ptr2;
    while (num--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

void* memccpy(void* dest, const void* src, int c, usize n) {
    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;
    while (n--) {
        *d = *s;
        if (*d == (u8)c) return d + 1;
        d++; s++;
    }
    return NULL;
}

void* memchr(const void* ptr, int value, usize num) {
    const u8* p = (const u8*)ptr;
    while (num--) {
        if (*p == (u8)value) return (void*)p;
        p++;
    }
    return NULL;
}

static char* strtok_save = NULL;

char* strtok(char* str, const char* delim) {
    if (str == NULL) str = strtok_save;
    if (str == NULL || *str == '\0') { strtok_save = NULL; return NULL; }

    /* skip leading delimiters */
    while (*str) {
        const char* d = delim;
        int found = 0;
        while (*d) { if (*str == *d++) { found = 1; break; } }
        if (!found) break;
        str++;
    }
    if (*str == '\0') { strtok_save = NULL; return NULL; }

    char* token_start = str;
    while (*str) {
        const char* d = delim;
        int found = 0;
        while (*d) { if (*str == *d++) { found = 1; break; } }
        if (found) { *str = '\0'; strtok_save = str + 1; return token_start; }
        str++;
    }
    strtok_save = NULL;
    return token_start;
}

char* strdup(const char* str) {
    usize len = strlen(str) + 1;
    char* s = kmalloc(len);
    if (s) memcpy(s, str, len);
    return s;
}

char* strrev(char* str) {
    if (!str) return NULL;
    usize i = 0, j = strlen(str) - 1;
    while (i < j) {
        char t = str[i]; str[i] = str[j]; str[j] = t;
        i++; j--;
    }
    return str;
}

char* strlwr(char* str) {
    char* s = str;
    while (*s) { if (*s >= 'A' && *s <= 'Z') *s = *s - 'A' + 'a'; s++; }
    return str;
}

char* strupr(char* str) {
    char* s = str;
    while (*s) { if (*s >= 'a' && *s <= 'z') *s = *s - 'a' + 'A'; s++; }
    return str;
}

usize strnlen(const char* str, usize maxlen) {
    usize len = 0;
    while (len < maxlen && str[len]) len++;
    return len;
}

char** strsplit(const char* str, const char* delim, int* count) {
    if (!str || !delim) return NULL;
    int token_count = 0;
    char* copy = strdup(str);
    if (!copy) return NULL;
    char* token = strtok(copy, delim);
    while (token) { token_count++; token = strtok(NULL, delim); }
    kfree(copy);

    char** tokens = kmalloc((token_count + 1) * sizeof(char*));
    if (!tokens) return NULL;
    copy = strdup(str);
    if (!copy) { kfree(tokens); return NULL; }

    token_count = 0;
    token = strtok(copy, delim);
    while (token) {
        tokens[token_count] = strdup(token);
        if (!tokens[token_count]) {
            for (int i = 0; i < token_count; i++) kfree(tokens[i]);
            kfree(tokens); kfree(copy); return NULL;
        }
        token_count++;
        token = strtok(NULL, delim);
    }
    tokens[token_count] = NULL;
    if (count) *count = token_count;
    kfree(copy);
    return tokens;
}

void strfree(char** array) {
    if (!array) return;
    for (int i = 0; array[i]; i++) kfree(array[i]);
    kfree(array);
}

void itoa(int n, char* str) {
    int i = 0;
    int sign = n;
    if (sign < 0) n = -n;
    do { str[i++] = n % 10 + '0'; } while ((n /= 10) > 0);
    if (sign < 0) str[i++] = '-';
    str[i] = '\0';
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char t = str[j]; str[j] = str[k]; str[k] = t;
    }
}

int vsnprintf(char* str, usize size, const char* format, va_list args) {
    if (!str || size == 0) return 0;

    char* out   = str;
    usize left  = size - 1;  /* leave room for NUL */
    const char* f = format;

#define PUT(ch) do { if (left > 0) { *out++ = (ch); left--; } } while(0)

    while (*f) {
        if (*f != '%') { PUT(*f++); continue; }
        f++;  /* skip '%' */

        /* flags */
        int zero_pad = 0;
        int left_align = 0;
        if (*f == '0') { zero_pad = 1; f++; }
        if (*f == '-') { left_align = 1; f++; }

        /* width */
        int width = 0;
        while (*f >= '0' && *f <= '9') { width = width * 10 + (*f++ - '0'); }

        switch (*f) {
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                char buf[20];
                itoa(val, buf);
                int len = (int)strlen(buf);
                if (!left_align) {
                    char pad = zero_pad ? '0' : ' ';
                    for (int i = len; i < width; i++) PUT(pad);
                }
                for (char* p = buf; *p; p++) PUT(*p);
                if (left_align) {
                    for (int i = len; i < width; i++) PUT(' ');
                }
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                char buf[20];
                int i = 0;
                if (val == 0) { buf[i++] = '0'; }
                else { while (val) { buf[i++] = '0' + (val % 10); val /= 10; } }
                buf[i] = '\0';
                strrev(buf);
                int len = i;
                if (!left_align) {
                    char pad = zero_pad ? '0' : ' ';
                    for (int j = len; j < width; j++) PUT(pad);
                }
                for (char* p = buf; *p; p++) PUT(*p);
                if (left_align) for (int j = len; j < width; j++) PUT(' ');
                break;
            }
            case 'x':
            case 'X': {
                unsigned int val = va_arg(args, unsigned int);
                const char* hex = (*f == 'x') ? "0123456789abcdef" : "0123456789ABCDEF";
                char buf[20];
                int i = 0;
                if (val == 0) { buf[i++] = '0'; }
                else { while (val) { buf[i++] = hex[val & 0xF]; val >>= 4; } }
                buf[i] = '\0';
                strrev(buf);
                int len = i;
                if (!left_align) {
                    char pad = zero_pad ? '0' : ' ';
                    for (int j = len; j < width; j++) PUT(pad);
                }
                for (char* p = buf; *p; p++) PUT(*p);
                if (left_align) for (int j = len; j < width; j++) PUT(' ');
                break;
            }
            case 'p': {
                unsigned int val = va_arg(args, unsigned int);
                PUT('0'); PUT('x');
                char buf[20];
                int i = 0;
                if (val == 0) { buf[i++] = '0'; }
                else { while (val) { buf[i++] = "0123456789abcdef"[val & 0xF]; val >>= 4; } }
                buf[i] = '\0';
                strrev(buf);
                /* always pad pointers to 8 digits */
                int len = i;
                for (int j = len; j < 8; j++) PUT('0');
                for (char* p = buf; *p; p++) PUT(*p);
                break;
            }
            case 's': {
                const char* val = va_arg(args, const char*);
                if (!val) val = "(null)";
                int len = (int)strlen(val);
                if (!left_align) for (int j = len; j < width; j++) PUT(' ');
                while (*val) PUT(*val++);
                if (left_align) for (int j = len; j < width; j++) PUT(' ');
                break;
            }
            case 'c': {
                char val = (char)va_arg(args, int);
                PUT(val);
                break;
            }
            case '%':
                PUT('%');
                break;
            default:
                PUT('%');
                PUT(*f);
                break;
        }
        f++;
    }
#undef PUT
    *out = '\0';
    return (int)(out - str);
}

int snprintf(char* str, usize size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}
