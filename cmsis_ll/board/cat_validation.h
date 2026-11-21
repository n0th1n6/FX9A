#ifndef FX9A_CAT_VALIDATION_H
#define FX9A_CAT_VALIDATION_H
#include <stdint.h>
#include <stdbool.h>

static inline bool fx9a_cat_decimal(const uint8_t *data, uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
        if (data[i] < '0' || data[i] > '9') return false;
    return true;
}
static inline bool fx9a_cat_valid_frame(const uint8_t *buf, uint8_t len)
{
    if (!buf || len < 2u || len > 32u) return false;
    if (buf[0] < 'A' || buf[0] > 'Z' || buf[1] < 'A' || buf[1] > 'Z') return false;
    uint16_t command = ((uint16_t)buf[0] << 8) | buf[1];
    switch (command) {
    case ('F' << 8) | 'A':
    case ('F' << 8) | 'B':
        return len == 2u || (len == 13u && fx9a_cat_decimal(&buf[2], 11u) &&
          buf[2] == '0' && buf[3] == '0' && buf[4] == '0');
    case ('A' << 8) | 'G':
        return len == 2u || (len == 6u && buf[2] == '0' &&
                              fx9a_cat_decimal(&buf[3], 3u) &&
          ((buf[3]-'0')*100u + (buf[4]-'0')*10u + buf[5]-'0') <= 255u);
    case ('B' << 8) | 'D':
    case ('B' << 8) | 'U':
        return len == 2u || (len == 4u && buf[2] == '0' && fx9a_cat_decimal(&buf[2], 2u));
    case ('M' << 8) | 'C':
        return len == 2u || (len == 4u && fx9a_cat_decimal(&buf[2], 2u)) ||
          (len == 5u && (buf[2] == '0' || buf[2] == ' ') && fx9a_cat_decimal(&buf[3], 2u));
    case ('F' << 8) | 'R':
        return len == 2u || (len == 3u && buf[2] >= '0' && buf[2] <= '2');
    case ('M' << 8) | 'D':
        return len == 2u || (len == 3u && buf[2] >= '1' && buf[2] <= '3');
    case ('R' << 8) | 'T':
    case ('X' << 8) | 'T':
        return len == 2u || (len == 3u && (buf[2] == '0' || buf[2] == '1'));
    case ('R' << 8) | 'X':
    case ('R' << 8) | 'C':
        return len == 2u;
    case ('K' << 8) | 'S':
        return len == 2u || (len == 5u && fx9a_cat_decimal(&buf[2], 3u));
    case ('R' << 8) | 'D':
    case ('R' << 8) | 'U':
        return len == 2u || (len == 7u && fx9a_cat_decimal(&buf[2], 5u));
    case ('M' << 8) | 'R':
        return len == 4u && buf[2] == '0' && buf[3] == '1';
    case ('C' << 8) | 'A':
        return len == 2u || (len == 3u && (buf[2] == '0' || buf[2] == '1'));
    case ('T' << 8) | 'X':
        return len == 3u && buf[2] == '0';
    case ('S' << 8) | 'M':
        return len == 3u && buf[2] == '0';
    default:
        return len == 2u; /* Legacy read-only compatibility replies; reject unsupported writes. */
    }
}
#endif
