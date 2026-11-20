#ifndef FX9A_TUNE_LOGIC_H
#define FX9A_TUNE_LOGIC_H
#include <stdint.h>
#include <stdbool.h>

/* Provisional thresholds for software verification, not calibrated SWR.
 * All time values are units of 10 ms from the independent TIM2 counter.
 * A hard timeout is required even if the ADC never produces a valid sample.
 */
#define FX9A_TUNE_MAX_TICKS 1000u
#define FX9A_TUNE_SETTLE_TICKS 40u
#define FX9A_TUNE_SUCCESS_TICKS 50u
#define FX9A_TUNE_GOOD_REFLECTED_ADC 75u
#define FX9A_TUNE_SEVERE_REFLECTED_ADC 800u

typedef enum {
    FX9A_TUNE_RUNNING = 0,
    FX9A_TUNE_SUCCESS,
    FX9A_TUNE_CANCEL,
    FX9A_TUNE_TIMEOUT,
    FX9A_TUNE_SEVERE
} Fx9aTuneResult;

typedef struct {
    uint32_t started_at;
    uint32_t good_since;
    bool good;
} Fx9aTuneLogic;

static inline void fx9a_tune_logic_init(Fx9aTuneLogic *logic, uint32_t tick)
{
    logic->started_at = tick;
    logic->good_since = tick;
    logic->good = false;
}

static inline Fx9aTuneResult fx9a_tune_logic_step(
    Fx9aTuneLogic *logic, uint32_t now, uint16_t reflected_adc, bool cancelled)
{
    if (cancelled) return FX9A_TUNE_CANCEL;
    if (reflected_adc >= FX9A_TUNE_SEVERE_REFLECTED_ADC)
        return FX9A_TUNE_SEVERE;
    if ((uint32_t)(now - logic->started_at) >= FX9A_TUNE_MAX_TICKS)
        return FX9A_TUNE_TIMEOUT;
    if ((uint32_t)(now - logic->started_at) < FX9A_TUNE_SETTLE_TICKS)
        return FX9A_TUNE_RUNNING;

    if (reflected_adc <= FX9A_TUNE_GOOD_REFLECTED_ADC) {
        if (!logic->good) {
            logic->good_since = now;
            logic->good = true;
        } else if ((uint32_t)(now - logic->good_since) >= FX9A_TUNE_SUCCESS_TICKS)
            return FX9A_TUNE_SUCCESS;
    } else logic->good = false;

    return FX9A_TUNE_RUNNING;
}
#endif
