#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "vfo_accel.h"

int main(void)
{
    Fx9aVfoAccel state = {0};
    assert(fx9a_vfo_accel_step(&state, 100u, 1, false) == 1);
    assert(fx9a_vfo_accel_step(&state, 110u, -1, false) == -1);
    assert(fx9a_vfo_accel_step(&state, 113u, 1, false) == 10);
    assert(fx9a_vfo_accel_step(&state, 114u, 1, false) == 100);
    assert(fx9a_vfo_accel_step(&state, 115u, -1, true) == -100);
    assert(fx9a_vfo_accel_step(&state, 150u, 1, false) == 1);
    assert(fx9a_vfo_accel_step(&state, 151u, 0, false) == 0);
    state.initialized = false;
    assert(fx9a_vfo_accel_step(&state, 0xfffffffdu, 1, false) == 1);
    assert(fx9a_vfo_accel_step(&state, 0x00000000u, -1, false) == -10);
    puts("PASS: optional VFO acceleration slow/medium/fast/reverse/coarse/wrap");
    return 0;
}
