/* Minimal standalone CPU bring-up. No peripheral RF drive or radio logic.
 * Observe fx9a_boot_marker, fx9a_system_clock_hz and fx9a_heartbeat in a
 * debugger. Deliberately no SysTick or IRQ enables yet.
 */
#include <stdint.h>

extern volatile uint32_t fx9a_system_clock_hz;
extern volatile uint32_t fx9a_clock_status;
volatile uint32_t fx9a_boot_marker;
volatile uint32_t fx9a_heartbeat;

int main(void)
{
    fx9a_boot_marker = 0xF9A01001u;
    for (;;) {
        ++fx9a_heartbeat;
        __asm volatile ("nop");
    }
}
