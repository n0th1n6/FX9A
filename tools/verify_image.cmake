if(NOT DEFINED ELF_FILE OR NOT EXISTS "${ELF_FILE}")
    message(FATAL_ERROR "FX9A ELF was not provided to image verification")
endif()

execute_process(
    COMMAND "${NM_TOOL}" -n "${ELF_FILE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE nm_output
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Unable to inspect FX9A symbols: ${nm_error}")
endif()

function(require_symbol pattern description)
    if(NOT nm_output MATCHES "${pattern}")
        message(FATAL_ERROR "FX9A image check failed: ${description}")
    endif()
endfunction()

require_symbol("(^|\n)08000000 T g_pfnVectors(\n|$)"
               "vector table is not at the original 0x08000000 address")
require_symbol("(^|\n)[0-9a-f]+ T TIM2_IRQHandler(\n|$)"
               "legacy scheduler does not own TIM2")
require_symbol("(^|\n)[0-9a-f]+ T TIM3_IRQHandler(\n|$)"
               "legacy PWM does not own TIM3")
require_symbol("(^|\n)[0-9a-f]+ W TIM4_IRQHandler(\n|$)"
               "TIM4 is no longer reserved for the Mbed ticker")
require_symbol("(^|\n)[0-9a-f]+ T TIM5_IRQHandler(\n|$)"
               "legacy keyer does not own its remapped TIM5 vector")
require_symbol("(^|\n)[0-9a-f]+ T USART1_IRQHandler(\n|$)"
               "legacy CAT interface does not own USART1")
require_symbol("(^|\n)[0-9a-f]+ T DMA1_Channel4_IRQHandler(\n|$)"
               "legacy CAT interface does not own DMA1 Channel 4")
require_symbol("(^|\n)[0-9a-f]+ T IntDelay(\n|$)"
               "rotary encoder debounce delay was optimized away")

execute_process(
    COMMAND "${OBJDUMP_TOOL}" -d --disassemble=delay_us "${ELF_FILE}"
    RESULT_VARIABLE objdump_result
    OUTPUT_VARIABLE delay_disassembly
    ERROR_VARIABLE objdump_error
)
if(NOT objdump_result EQUAL 0)
    message(FATAL_ERROR "Unable to inspect FX9A delay loop: ${objdump_error}")
endif()

if(NOT delay_disassembly MATCHES
   "nop[\t ]*\n[^\n]*subs[^\n]*\n[^\n]*cmp[^\n]*\n[^\n]*bne")
    message(FATAL_ERROR
            "FX9A delay loop no longer matches the original IAR timing sequence")
endif()

message(STATUS "FX9A linked-image compatibility checks passed")
