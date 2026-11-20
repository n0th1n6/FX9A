/* FX-9A minimal standalone image; base matches legacy IAR and Mbed.
 * Flashing at 0x08000000 may overwrite a bootloader; see DFU_BACKUP.md.
 * Size matches current Mbed target configuration (256K flash, 48K RAM).
 */
ENTRY(Reset_Handler)
MEMORY {
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 48K
}
__StackTop = ORIGIN(RAM) + LENGTH(RAM);
_Min_Stack_Size = 0x400;
SECTIONS {
  .isr_vector : {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > FLASH
  .text : {
    . = ALIGN(4);
    *(.text*) *(.rodata*)
    . = ALIGN(4);
  } > FLASH
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
  . = ALIGN(4);
  __data_load__ = LOADADDR(.data);
  .data : {
    . = ALIGN(4);
    __data_start__ = .;
    *(.data*)
    . = ALIGN(4);
    __data_end__ = .;
  } > RAM AT > FLASH
  .bss (NOLOAD) : {
    . = ALIGN(4);
    __bss_start__ = .;
    *(.bss*) *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
  } > RAM
  .noinit (NOLOAD) : {
    . = ALIGN(4);
    *(.noinit*)
    . = ALIGN(4);
  } > RAM
  .stack_guard (NOLOAD) : {
    . = ALIGN(8);
    __end_of_static_ram__ = .;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > RAM
  ASSERT(__end_of_static_ram__ + _Min_Stack_Size <= __StackTop, "RAM overflow: stack reserve")
  /DISCARD/ : { *(.note.GNU-stack) *(.comment) }
}
