#include "strtofix16.h"
#include <errno.h>
#include <fix16.h>
#include <stddef.h>
#include <stdbool.h>
#ifndef FIXMATH_NO_CTYPE
#include <ctype.h>
#else
static inline int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

static inline int isxdigit(int c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')
}

static inline int isspace(int c)
{
    return c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '\v' || c == '\f';
}
#endif

static const uint32_t scales[8] = {
    /* 5 decimals is enough for full fix16_t precision */
    1, 10, 100, 1000, 10000, 100000, 100000, 100000
};

static char *itoa_loop(char *buf, uint32_t scale, uint32_t value, bool skip)
{
    while (scale)
    {
        unsigned digit = (value / scale);

        if (!skip || digit || scale == 1)
        {
            skip = false;
            *buf++ = '0' + digit;
            value %= scale;
        }

        scale /= 10;
    }
    return buf;
}


fix16_t strtofix16(const char *nptr, char** endptr)
{
    while (isspace(*nptr))
        nptr++;

    /* Decode the sign */
    bool negative = (*nptr == '-');
    if (*nptr == '+' || *nptr == '-')
        nptr++;

    /* Decode the number */
    fix16_t value;
    uint32_t intpart = 0;
    uint32_t fracpart = 0;
    uint32_t scale = 1;
    int count = 0;

    /* Decode hexadecimal */
    if ((nptr[0] == '0') && (( nptr[1] == 'x') || ( nptr[1] == 'X' ) ))
    {
        nptr++;
        nptr++;

        /* Decode the integer part */
        while (isxdigit(*nptr))
        {
            intpart <<= 4;
            char c = *nptr++;
            intpart += (c > '9')? (c &~ 0x20) - 'A' + 10: (c - '0');;
            count++;
        }

        /* Check for overflow (nb: count == 0 is OK)*/
        if (count > 4 || intpart > 32768 || (!negative && intpart > 32767))
        {
            errno = ERANGE;
            if (endptr) *endptr = (char*) nptr;
            return negative ? -fix16_overflow : fix16_overflow;
        }

        value = intpart << 16;

        /* Decode the decimal part */
        if (*nptr == '.' || *nptr == ',')
        {
            nptr++;

            while (isxdigit(*nptr) && scale < 0xFFFF)
            {
                scale *= 16;
                fracpart *= 16;
                char c = *nptr++;
                fracpart += (c > '9')? (c &~ 0x20) - 'A' + 10: (c - '0');;
            }

            value += fix16_div(fracpart, scale);
        }
    }
    else
    {
        /* Decode the integer part */
        while (isdigit(*nptr))
        {
            intpart *= 10;
            intpart += *nptr++ - '0';
            count++;
        }

        /* Check for overflow */
        if (count > 5 || intpart > 32768 || (!negative && intpart > 32767))
        {
            errno = ERANGE;
            if (endptr) *endptr = (char*) nptr;
            return negative ? -fix16_overflow : fix16_overflow;
        }

        value = intpart << 16;

        /* Decode the decimal part */
        if (*nptr == '.' || *nptr == ',')
        {
            nptr++;

            while (isdigit(*nptr) && scale < 100000)
            {
                scale *= 10;
                fracpart *= 10;
                fracpart += *nptr++ - '0';
            }

            value += fix16_div(fracpart, scale);
        }
      }

    if (endptr) *endptr = (char*) nptr;
    return negative ? -value : value;
}

size_t fix16tostr(fix16_t value, char *buf, int decimals)
{
    size_t start = (size_t) buf;
    uint32_t uvalue = (value >= 0) ? value : -value;
    if (value < 0)
        *buf++ = '-';

    /* Separate the integer and decimal parts of the value */
    unsigned intpart = uvalue >> 16;
    uint32_t fracpart = uvalue & 0xFFFF;
    uint32_t scale = scales[decimals & 7];
    fracpart = fix16_mul(fracpart, scale);

    if (fracpart >= scale)
    {
        /* Handle carry from decimal part */
        intpart++;
        fracpart -= scale;
    }

    /* Format integer part */
    buf = itoa_loop(buf, 10000, intpart, true);

    /* Format decimal part (if any) */
    if (scale != 1)
    {
        *buf++ = '.';
        buf = itoa_loop(buf, scale / 10, fracpart, false);
    }

    *buf = '\0';
    return (size_t) buf - start;
}
