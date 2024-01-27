#ifndef __RVBT_SERIAL_H__
#define __RVBT_SERIAL_H__
#include <sbi_utils/serial/sifive-uart.h>

int rvbt_serial_init();

int rvbt_printf(const char* fmt, ...);

char* rvbt_gets();
#endif
