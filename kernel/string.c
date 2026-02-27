#include "kernel.h"

usize strlen(const char* str) {
    usize len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++)) {
    }
    return dest;
}

char* strncpy(char* dest, const char* src, usize n) {
    char* d = dest;
    while (n && (*d++ = *src++)) {
        n--;
    }
    while (n--) {
        *d++ = '\0';
    }
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
        if (*str == c) {
            return (char*)str;
        }
        str++;
    }
    return NULL;
}

char* strrchr(const char* str, int c) {
    const char* last = NULL;
    while (*str) {
        if (*str == c) {
            last = str;
        }
        str++;
    }
    return (char*)last;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const u8*)s1 - *(const u8*)s2;
}

int strncmp(const char* s1, const char* s2, usize n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *(const u8*)s1 - *(const u8*)s2;
}

int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return sign * result;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;

    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;

        while (*h && *n && *h == *n) {
            h++;
            n++;
        }

        if (!*n) return (char*)haystack;
    }

    return NULL;
}

void* memset(void* ptr, int value, usize num) {
    u8* p = (u8*)ptr;
    while (num--) {
        *p++ = (u8)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, usize num) {
    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;

    while (num--) {
        *d++ = *s++;
    }

    return dest;
}

void* memmove(void* dest, const void* src, usize num) {
    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;

    if (d < s) {
        while (num--) {
            *d++ = *s++;
        }
    } else {
        d += num;
        s += num;
        while (num--) {
            *--d = *--s;
        }
    }

    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, usize num) {
    const u8* p1 = (const u8*)ptr1;
    const u8* p2 = (const u8*)ptr2;

    while (num--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

static char* strtok_save = NULL;

char* strtok(char* str, const char* delim) {
    char* token_start;

    if (str == NULL) {
        str = strtok_save;
    }

    if (str == NULL || *str == '\0') {
        strtok_save = NULL;
        return NULL;
    }

    while (*str) {
        const char* d = delim;
        int found = 0;
        while (*d) {
            if (*str == *d) {
                found = 1;
                break;
            }
            d++;
        }
        if (!found) break;
        str++;
    }

    if (*str == '\0') {
        strtok_save = NULL;
        return NULL;
    }

    token_start = str;

    while (*str) {
        const char* d = delim;
        int found = 0;
        while (*d) {
            if (*str == *d) {
                found = 1;
                break;
            }
            d++;
        }
        if (found) {
            *str = '\0';
            strtok_save = str + 1;
            return token_start;
        }
        str++;
    }

    strtok_save = NULL;
    return token_start;
}

char* strdup(const char* str) {
    usize len = strlen(str) + 1;
    char* new_str = kmalloc(len);
    if (new_str) {
        memcpy(new_str, str, len);
    }
    return new_str;
}

/* Reverse string in place */
char* strrev(char* str) {
    if (!str) return NULL;

    usize len = strlen(str);
    usize i = 0;
    usize j = len - 1;

    while (i < j) {
        char temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }

    return str;
}

char* strlwr(char* str) {
    char* s = str;
    while (*s) {
        if (*s >= 'A' && *s <= 'Z') {
            *s = *s - 'A' + 'a';
        }
        s++;
    }
    return str;
}

char* strupr(char* str) {
    char* s = str;
    while (*s) {
        if (*s >= 'a' && *s <= 'z') {
            *s = *s - 'a' + 'A';
        }
        s++;
    }
    return str;
}

int snprintf(char* str, usize size, const char* format, ...) {
    (void)size;

    char* s = str;
    const char* f = format;

    while (*f && s - str < (int)size - 1) {
        if (*f == '%') {
            f++;
            if (*f == 's') {
                const char** arg = (const char**)(&f + 1);
                const char* val = *arg;
                if (!val) val = "(null)";

                while (*val && s - str < (int)size - 1) {
                    *s++ = *val++;
                }
                f++;
            } else if (*f == 'd' || *f == 'i') {
                int* arg = (int*)(&f + 1);
                int val = *arg;
                char num[16];
                itoa(val, num);

                char* n = num;
                while (*n && s - str < (int)size - 1) {
                    *s++ = *n++;
                }
                f++;
            } else if (*f == 'c') {
                int* arg = (int*)(&f + 1);
                char val = (char)*arg;
                if (s - str < (int)size - 1) {
                    *s++ = val;
                }
                f++;
            } else {
                if (s - str < (int)size - 1) {
                    *s++ = '%';
                }
                if (s - str < (int)size - 1) {
                    *s++ = *f++;
                }
            }
        } else {
            *s++ = *f++;
        }
    }

    *s = '\0';
    return (int)(s - str);
}

int vsnprintf(char* str, usize size, const char* format, void* args) {
    (void)args;  /* not implemented */
    return snprintf(str, size, format);
}

usize strnlen(const char* str, usize maxlen) {
    usize len = 0;
    while (len < maxlen && str[len]) {
        len++;
    }
    return len;
}

void* memccpy(void* dest, const void* src, int c, usize n) {
    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;

    while (n--) {
        *d = *s;
        if (*d == (u8)c) {
            return d + 1;
        }
        d++;
        s++;
    }

    return NULL;
}

void* memchr(const void* ptr, int value, usize num) {
    const u8* p = (const u8*)ptr;

    while (num--) {
        if (*p == (u8)value) {
            return (void*)p;
        }
        p++;
    }

    return NULL;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;

        if (c1 >= 'A' && c1 <= 'Z') c1 = c1 - 'A' + 'a';
        if (c2 >= 'A' && c2 <= 'Z') c2 = c2 - 'A' + 'a';

        if (c1 != c2) {
            return c1 - c2;
        }

        s1++;
        s2++;
    }

    return *s1 - *s2;
}

char** strsplit(const char* str, const char* delim, int* count) {
    if (!str || !delim) return NULL;

    int token_count = 0;
    char* copy = strdup(str);
    if (!copy) return NULL;

    char* token = strtok(copy, delim);
    while (token) {
        token_count++;
        token = strtok(NULL, delim);
    }

    kfree(copy);

    char** tokens = kmalloc((token_count + 1) * sizeof(char*));
    if (!tokens) return NULL;

    copy = strdup(str);
    if (!copy) {
        kfree(tokens);
        return NULL;
    }

    token_count = 0;
    token = strtok(copy, delim);
    while (token) {
        tokens[token_count] = strdup(token);
        if (!tokens[token_count]) {
            for (int i = 0; i < token_count; i++) {
                kfree(tokens[i]);
            }
            kfree(tokens);
            kfree(copy);
            return NULL;
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

    for (int i = 0; array[i]; i++) {
        kfree(array[i]);
    }

    kfree(array);
}

void itoa(int n, char* str) {
    int i = 0;
    int sign = n;

    if (sign < 0) n = -n;

    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) str[i++] = '-';

    str[i] = '\0';

    /* string reversed */
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}
