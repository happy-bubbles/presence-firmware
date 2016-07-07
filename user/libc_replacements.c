
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"

char* ICACHE_FLASH_ATTR strtok_r(char* s, const char* delim, char** last) {
    const char* spanp;
    char* tok;
    char c;
    char sc;

    if (s == NULL && (s = *last) == NULL) {
        return (NULL);
    }


    // Skip (span) leading delimiters 
    //
cont:
    c = *s++;
    for (spanp = delim; (sc = *spanp++) != 0;) {
        if (c == sc) {
            goto cont;
        }
    }

    // check for no delimiters left
    //
    if (c == '\0') {
        *last = NULL;
        return (NULL);
    }

    tok = s - 1;


    // Scan token 
    // Note that delim must have one NUL; we stop if we see that, too.
    //
    for (;;) {
        c = *s++;
        spanp = (char *)delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0) {
                    s = NULL;
                }
                else {
                    s[-1] = '\0';
                }
                *last = s;
                return (tok);
            }

        } while (sc != 0);
    }

    // NOTREACHED EVER
}

char* ICACHE_FLASH_ATTR strtok(char* s, const char* delim) {
    static char* last;

    return (strtok_r(s, delim, &last));
}
