#ifndef FX9A_ENCODER_H
#define FX9A_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t previous;
    int8_t movement;
    bool synchronized;
} EncoderState;

static inline void encoder_init(EncoderState *encoder, uint8_t phases)
{
    encoder->previous = phases & 3;
    encoder->movement = 0;
    encoder->synchronized = encoder->previous == 3;
}

/* One action per complete electrical cycle, matching the previous rising-edge
 * count. Contact bounce retraces and cancels movement. Ignore impossible
 * two-contact jumps without replacing the last valid state: a bouncing common
 * contact can briefly read 11 during the 00 portion of a valid cycle. No
 * direction or missing transition is synthesized. */
static inline int8_t encoder_update(EncoderState *encoder, uint8_t phases)
{
    static const int8_t transitions[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0
    };
    phases &= 3;
    uint8_t previous = encoder->previous;
    if (!encoder->synchronized) {
        encoder->previous = phases;
        encoder->synchronized = phases == 3;
        encoder->movement = 0;
        return 0;
    }
    if ((previous ^ phases) == 3)
        return 0;
    encoder->previous = phases;
    encoder->movement += transitions[(previous << 2) | phases];
    if (phases != 3)
        return 0;
    int8_t step = encoder->movement == 4 ? 1 :
                  encoder->movement == -4 ? -1 : 0;
    encoder->movement = 0;
    return step;
}

#endif
