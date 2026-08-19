#ifndef FX9A_LEGACY_CORE_CMINSTR_H
#define FX9A_LEGACY_CORE_CMINSTR_H

#ifndef __ASM
#define __ASM __asm
#endif

#ifndef __DSB
#define __DSB() __ASM volatile ("dsb 0xF" ::: "memory")
#endif

#endif
