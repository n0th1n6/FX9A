#include "bsp_board.h"
#include "bsp_timer.h"

int main(void)
{
    /* Mbed CE startup has already initialized the core and 72 MHz clock. */
    board_init();
    TMR_Start(TIM2);

    while (1) {
        board_task();
    }
}
