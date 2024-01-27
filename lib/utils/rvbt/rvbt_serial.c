#include <sbi_utils/rvbt/rvbt_serial.h>
#include <stdint.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/rvbt/mfmt.h>


#define va_start(v, l) __builtin_va_start((v), l)
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg
typedef __builtin_va_list va_list;
char buffer[100];
volatile uint8_t* uart16550;
// some devices require a shifted register index
// (e.g. 32 bit registers instead of 8 bit registers)
uintptr_t uart16550_reg_shift;
uintptr_t uart16550_clock = 3686400;   // a "common" base clock

#define UART_REG_QUEUE     0    // rx/tx fifo data
#define UART_REG_DLL       0    // divisor latch (LSB)
#define UART_REG_IER       1    // interrupt enable register
#define UART_REG_DLM       1    // divisor latch (MSB) 
#define UART_REG_FCR       2    // fifo control register
#define UART_REG_LCR       3    // line control register
#define UART_REG_MCR       4    // modem control register
#define UART_REG_LSR       5    // line status register
#define UART_REG_MSR       6    // modem status register
#define UART_REG_SCR       7    // scratch register
#define UART_REG_STATUS_RX 0x01
#define UART_REG_STATUS_TX 0x20


// We cannot use the word DEFAULT for a parameter that cannot be overridden due to -Werror
#ifndef UART_DEFAULT_BAUD
#define UART_DEFAULT_BAUD  38400
#endif
int uart16550_putchar(uint8_t ch)
{
  while ((uart16550[UART_REG_LSR << uart16550_reg_shift] & UART_REG_STATUS_TX) == 0);
  uart16550[UART_REG_QUEUE << uart16550_reg_shift] = ch;
  return 0;
}

int uart16550_getchar()
{
  int ch = -1;
  if ((uart16550[UART_REG_LSR << uart16550_reg_shift] & UART_REG_STATUS_RX) != 0) {
    ch = uart16550[UART_REG_QUEUE << uart16550_reg_shift];
    sbi_printf("%c", ch);
  }
  return ch;
}

int uart16550_destroy()
{
  return 0;
}

int uart16550_init(uintptr_t reg_addr) 
{
    // uint32_t clock_freq;

    // uint32_t divisor = uart16550_clock / (16 * UART_DEFAULT_BAUD);
    // // If the divisor is out of range, don't assert, set the rate back to the default
    // if (divisor >= 0x10000u)
    //     divisor = uart16550_clock / (16 * UART_DEFAULT_BAUD);

    uart16550 = (void*)reg_addr;
    uart16550_reg_shift = 0;
    
    uart16550[UART_REG_IER << uart16550_reg_shift] = 0x00;                // Disable all interrupts
    uart16550[UART_REG_LCR << uart16550_reg_shift] = 0x03;                // Enable DLAB (set baud rate divisor)
    // uart16550[UART_REG_DLL << uart16550_reg_shift] = (uint8_t)divisor;    // Set divisor (lo byte)
    // uart16550[UART_REG_DLM << uart16550_reg_shift] = (uint8_t)(divisor >> 8);     //     (hi byte)
    // uart16550[UART_REG_LCR << uart16550_reg_shift] = 0x03;                // 8 bits, no parity, one stop bit
    // uart16550[UART_REG_FCR << uart16550_reg_shift] = 0xC7;                // Enable FIFO, clear them, with 14-byte threshold
    return 0;
}

int rvbt_printf(const char* fmt, ...) {
  char *buf_ptr = (char*) buffer;
  va_list args;
  va_start(args, fmt);
  print((char**)&buf_ptr, NULL, fmt, args);
  va_end(args);
  int idx = 0;
  while(buffer[idx])
    uart16550_putchar(buffer[idx++]);
  return 0;
}

char* rvbt_gets() {
  int ch, cnt = 0;
  while((ch = uart16550_getchar()) != '\r') {
    if(ch == -1)
      continue;
    buffer[cnt++] = ch;
  }
  buffer[cnt] = '\0';
  sbi_printf("\n");
  return buffer;
}


int rvbt_serial_init() {
  uart16550_init(0x10000000);
  return 0;
}
