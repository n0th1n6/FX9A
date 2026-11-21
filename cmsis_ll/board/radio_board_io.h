#ifndef FX9A_NATIVE_BOARD_IO_H
#define FX9A_NATIVE_BOARD_IO_H
/* Board power/audio GPIO owned by bsp_board.c. */
void gpio_init(void);
void power_key_init(void);
void power_on(void);
void power_off(void);
void power_high_on(void);
void power_high_off(void);
void voice_control_start(void);
void voice_control_stop(void);
#endif
