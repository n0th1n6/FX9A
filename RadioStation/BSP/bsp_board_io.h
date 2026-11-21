#ifndef FX9A_BSP_BOARD_IO_H
#define FX9A_BSP_BOARD_IO_H
/* Board GPIO operations, shared by radio control and bring-up. */
/* Include after bsp_board.h so the Mbed GPIO symbol remap is active. */
void gpio_init(void);
void power_key_init(void);
void WaitForPowerOn(void);
void voice_control_stop(void);
void voice_control_start(void);
void power_high_on(void);
void power_high_off(void);
void power_on(void);
void power_off(void);

#endif
