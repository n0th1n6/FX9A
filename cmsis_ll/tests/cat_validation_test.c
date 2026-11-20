#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "cat_validation.h"

#define CHECK_OK(literal) assert(fx9a_cat_valid_frame((const uint8_t *)(literal), (uint8_t)(sizeof(literal)-1u)))
#define CHECK_BAD(literal) assert(!fx9a_cat_valid_frame((const uint8_t *)(literal), (uint8_t)(sizeof(literal)-1u)))
int main(void)
{
    CHECK_OK("FA");
    CHECK_OK("FA00014195000");
    CHECK_OK("FB00014195000");
    CHECK_BAD("FA123");
    CHECK_BAD("FA0001419X000");
    CHECK_OK("AG");
    CHECK_OK("AG0255");
    CHECK_BAD("AG025");
    CHECK_BAD("AG02x5");
    CHECK_OK("BD03");
    CHECK_BAD("BD3");
    CHECK_OK("KS060");
    CHECK_BAD("KS06X");
    CHECK_OK("RD00010");
    CHECK_BAD("RD0001");
    CHECK_OK("MR01");
    CHECK_BAD("MR00");
    CHECK_OK("CA");
    CHECK_OK("CA1");
    CHECK_OK("CA0");
    CHECK_BAD("CA9");
    CHECK_OK("TX0");
    CHECK_BAD("TX");
    CHECK_BAD("TX1");
    CHECK_OK("SM0");
    CHECK_BAD("SM");
    CHECK_BAD("SM9");
    CHECK_BAD("A");
    assert(!fx9a_cat_valid_frame((const uint8_t *)0, 2));
    puts("PASS: CAT framing, numeric fields, transmit and tune commands");
    return 0;
}
