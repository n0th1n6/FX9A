/* Standalone FX-9A full application entry point.
 * Reset and clock setup are supplied by cmsis_ll/platform, not Mbed CE.
 */
#include "bsp_board.h"
#include "bsp_timer.h"

int main(void)
{
    board_init();
    TMR_Start(TIM2);
    for (;;) {
        board_task();
    }
}
