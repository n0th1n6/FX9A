#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "tune_logic.h"

static Fx9aTuneLogic tune;
int main(void)
{
    fx9a_tune_logic_init(&tune, 0);
    assert(fx9a_tune_logic_step(&tune, 0, 300, false) == FX9A_TUNE_RUNNING);
    assert(fx9a_tune_logic_step(&tune, 1, 800, false) == FX9A_TUNE_SEVERE);
    fx9a_tune_logic_init(&tune, 0);
    assert(fx9a_tune_logic_step(&tune, 39, 0, false) == FX9A_TUNE_RUNNING);
    assert(fx9a_tune_logic_step(&tune, 40, 50, false) == FX9A_TUNE_RUNNING);
    assert(fx9a_tune_logic_step(&tune, 89, 50, false) == FX9A_TUNE_RUNNING);
    assert(fx9a_tune_logic_step(&tune, 90, 50, false) == FX9A_TUNE_SUCCESS);

    fx9a_tune_logic_init(&tune, 0);
    assert(fx9a_tune_logic_step(&tune, 50, 30, false) == FX9A_TUNE_RUNNING);
    assert(fx9a_tune_logic_step(&tune, 80, 400, false) == FX9A_TUNE_RUNNING);
    assert(fx9a_tune_logic_step(&tune, 100, 30, false) == FX9A_TUNE_RUNNING);
    assert(fx9a_tune_logic_step(&tune, 150, 30, false) == FX9A_TUNE_SUCCESS);
    fx9a_tune_logic_init(&tune, 0);
    assert(fx9a_tune_logic_step(&tune, 800, 500, true) == FX9A_TUNE_CANCEL);
    assert(fx9a_tune_logic_step(&tune, 1000, 500, false) == FX9A_TUNE_TIMEOUT);
    fx9a_tune_logic_init(&tune, 0xfffffff0u);
    assert(fx9a_tune_logic_step(&tune, 0x00000020u, 50, false) == FX9A_TUNE_RUNNING);
    puts("PASS: TUNE settle, steady success, reset, severe SWR, cancel, timeout and wrapping ticks");
    return 0;
}
