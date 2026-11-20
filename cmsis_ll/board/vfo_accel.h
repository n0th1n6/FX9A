#ifndef FX9A_VFO_ACCEL_H
#define FX9A_VFO_ACCEL_H
#include <stdint.h>
#include <stdbool.h>

/* Optional acceleration for the optical VFO encoder only.
 * tick is the independent 10 ms TIM2 clock; original TS step is retained.
 * This policy is disabled by default in CMake until mechanical/RF testing.
 */
typedef struct {
    uint32_t last_tick;
    bool initialized;
} Fx9aVfoAccel;

static inline int32_t fx9a_vfo_accel_step(Fx9aVfoAccel *state,
                                          uint32_t tick, int8_t direction,
                                          bool coarse)
{
    if (direction == 0) return 0;
    uint32_t delta = state->initialized ? tick - state->last_tick : 1000u;
    state->initialized = true;
    state->last_tick = tick;
    if (coarse) return direction > 0 ? 100 : -100;
    uint32_t increment = delta <= 1u ? 100u :
                         delta <= 3u ? 10u : 1u;
    return direction > 0 ? (int32_t)increment : -(int32_t)increment;
}
#endif
