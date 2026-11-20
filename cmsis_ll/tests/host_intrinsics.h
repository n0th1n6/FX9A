#ifndef FX9A_HOST_INTRINSICS_H
#define FX9A_HOST_INTRINSICS_H
/* Native Linux register simulation only.
 * STM32F1 CMSIS/LL uses Cortex-M LDREX/STREX for atomic RMW operations.
 * The test is single-threaded, so a plain read/write models their effect.
 * Never include this header in ARM firmware.
 */
#if !defined(__arm__) && !defined(__thumb__)
#include <stdint.h>
static inline uint32_t fx9a_host_ldrexw(volatile uint32_t *ptr)
{
    return *ptr;
}
static inline uint32_t fx9a_host_strexw(uint32_t value, volatile uint32_t *ptr)
{
    *ptr = value;
    return 0;
}
#define __LDREXW(addr) fx9a_host_ldrexw((volatile uint32_t *)(addr))
#define __STREXW(value, addr) fx9a_host_strexw((value), (volatile uint32_t *)(addr))
#else
#error "Host register-test intrinsics must never be included in ARM firmware"
#endif
#endif
