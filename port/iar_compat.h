#ifndef FX9A_IAR_COMPAT_H
#define FX9A_IAR_COMPAT_H

#if defined(__GNUC__)
void IntDelay(void) __attribute__((optimize("O0")));
#endif

#endif
